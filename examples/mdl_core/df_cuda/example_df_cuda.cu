/******************************************************************************
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <curand_kernel.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include "example_df_cuda.h"
#include "texture_support_cuda.h"

// Custom structure representing the resources used by the generated code of a target code object.
struct Target_code_data
{
    size_t               num_textures;      // number of elements in the textures field
    Texture              *textures;          // a list of Texture objects, if used
    unsigned char const  *ro_data_segment;   // the read-only data segment, if used
};


// The number of generated MDL DF functions available.
extern __constant__ unsigned int     mdl_df_functions_count;

// The target code indices for the generated MDL DF functions.
extern __constant__ unsigned int     mdl_df_target_code_indices[];

// The target argument block indices for the generated MDL sub-expression functions.
// Note: the original indices are incremented by one to allow us to use 0 as "not-used".
extern __constant__ unsigned int     mdl_df_arg_block_indices[];

// The function pointers of the generated MDL DF functions.
extern __constant__ mi::mdl::Bsdf_init_function      *mdl_df_functions_init[];
extern __constant__ mi::mdl::Bsdf_sample_function    *mdl_df_functions_sample[];
extern __constant__ mi::mdl::Bsdf_evaluate_function  *mdl_df_functions_evaluate[];
extern __constant__ mi::mdl::Bsdf_pdf_function       *mdl_df_functions_pdf[];


// Identity matrix
__constant__ const float4 id[4] = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f}
};


// 3d vector math utilities
__device__ inline float3 operator+(const float3& a, const float3& b)
{
    return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
}
__device__ inline float3 operator-(const float3& a, const float3& b)
{
    return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
}
__device__ inline float3 operator*(const float3& a, const float3& b)
{
    return make_float3(a.x * b.x, a.y * b.y, a.z * b.z);
}
__device__ inline float3 operator*(const float3& a, const float s)
{
    return make_float3(a.x * s, a.y * s, a.z * s);
}
__device__ inline float3 operator/(const float3& a, const float s)
{
    return make_float3(a.x / s, a.y / s, a.z / s);
}
__device__ inline void operator+=(float3& a, const float3& b)
{
    a.x += b.x; a.y += b.y; a.z += b.z;
}
__device__ inline void operator*=(float3& a, const float& s)
{
    a.x *= s; a.y *= s; a.z *= s;
}
__device__ inline float squared_length(const float3 &d)
{
    return d.x * d.x + d.y * d.y + d.z * d.z;
}
__device__ inline float3 normalize(const float3 &d)
{
    const float inv_len = 1.0f / sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
    return make_float3(d.x * inv_len, d.y * inv_len, d.z * inv_len);
}
__device__ inline float dot(const float3 &u, const float3 &v)
{
    return u.x * v.x + u.y * v.y + u.z * v.z;
}
__device__ inline float3 cross(const float3 &u, const float3 &v)
{
    return make_float3(
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x);
}

typedef curandStatePhilox4_32_10_t Rand_state;

// direction to environment map texture coordinates
__device__ inline float2 environment_coords(const float3 &dir)
{
    const float u = atan2f(dir.z, dir.x) * (float)(0.5 / M_PI) + 0.5f;
    const float v = acosf(fmax(fminf(-dir.y, 1.0f), -1.0f)) * (float)(1.0 / M_PI);
    return make_float2(u, v);
}

// importance sample the environment
__device__ inline float3 environment_sample(
    float3 &dir,
    float  &pdf,
    const  float3 &xi,
    const  Kernel_params &params)
{
    // importance sample an envmap pixel using an alias map
    const unsigned int size = params.env_size.x * params.env_size.y;
    const unsigned int idx = min((unsigned int)(xi.x * (float)size), size - 1);
    unsigned int env_idx;
    float xi_y = xi.y;
    if (xi_y < params.env_accel[idx].q) {
        env_idx = idx ;
        xi_y /= params.env_accel[idx].q;
    } else {
        env_idx = params.env_accel[idx].alias;
        xi_y = (xi_y - params.env_accel[idx].q) / (1.0f - params.env_accel[idx].q);
    }

    const unsigned int py = env_idx / params.env_size.x;
    const unsigned int px = env_idx % params.env_size.x;
    pdf = params.env_accel[env_idx].pdf;

    // uniformly sample spherical area of pixel
    const float u = (float)(px + xi_y) / (float)params.env_size.x;
    const float phi = u * (float)(2.0 * M_PI) - (float)M_PI;
    float sin_phi, cos_phi;
    sincosf(phi, &sin_phi, &cos_phi);
    const float step_theta = (float)M_PI / (float)params.env_size.y;
    const float theta0 = (float)(py) * step_theta;
    const float cos_theta = cosf(theta0) * (1.0f - xi.z) + cosf(theta0 + step_theta) * xi.z;
    const float theta = acosf(cos_theta);
    const float sin_theta = sinf(theta);
    dir = make_float3(cos_phi * sin_theta, -cos_theta, sin_phi * sin_theta);

    // lookup filtered value
    const float v = theta * (float)(1.0 / M_PI);
    const float4 t = tex2D<float4>(params.env_tex, u, v);
    return make_float3(t.x, t.y, t.z) / pdf;
}

// evaluate the environment
__device__ inline float3 environment_eval(
    float &pdf,
    const float3 &dir,
    const Kernel_params &params)
{
    const float2 uv = environment_coords(dir);
    const unsigned int x =
        min((unsigned int)(uv.x * (float)params.env_size.x), params.env_size.x - 1);
    const unsigned int y =
        min((unsigned int)(uv.y * (float)params.env_size.y), params.env_size.y - 1);

    pdf = params.env_accel[y * params.env_size.x + x].pdf;
    const float4 t = tex2D<float4>(params.env_tex, uv.x, uv.y);
    return make_float3(t.x, t.y, t.z);
}


// Intersect a sphere with given radius located at the (0,0,0)
__device__ inline float intersect_sphere(
    const float3 &pos,
    const float3 &dir,
    const float radius)
{
    const float b = 2.0f * dot(dir, pos);
    const float c = dot(pos, pos) - radius * radius;

    float tmp = b * b - 4.0f * c;
    if (tmp < 0.0f)
        return -1.0f;

    tmp = sqrtf(tmp);
    const float t0 = (((b < 0.0f) ? -tmp : tmp) - b) * 0.5f;
    const float t1 = c / t0;

    const float m = fminf(t0, t1);
    return m > 0.0f ? m : fmaxf(t0, t1);
}

__device__ inline float3 render_sphere(
    Rand_state &rand_state,
    const Kernel_params &params,
    const float2 &screen_pos)
{
    // create ray from the pinhole camera
    const float r = (2.0f * screen_pos.x - 1.0f);
    const float u = (2.0f * screen_pos.y - 1.0f);
    const float aspect = (float)params.resolution.y / (float)params.resolution.x;

    const float3 dir = normalize(
        params.cam_dir * params.cam_focal + params.cam_right * r + params.cam_up * aspect * u);

    // intersect with geometry
    const float t = intersect_sphere(params.cam_pos, dir, 1.0f);
    if (t < 0.0f) {
        if (params.mdl_test_type != MDL_TEST_NO_ENV) {
            const float2 uv = environment_coords(dir);
            const float4 texval = tex2D<float4>(params.env_tex, uv.x, uv.y);
            return make_float3(texval.x, texval.y, texval.z);
        } else {
            return make_float3(0, 0, 0);
        }
    }

    // compute geometry state
    const float3 position = params.cam_pos + dir * t;
    const float3 normal = normalize(position);

    const float phi = atan2f(normal.x, normal.z);
    const float theta = acosf(normal.y);

    const float3 uvw = make_float3(
        (phi * (float)(0.5 / M_PI) + 0.5f) * 2.0f,
        1.0f - theta * (float)(1.0 / M_PI),
        0.0f);

    float sp, cp;
    sincosf(phi, &sp, &cp);
    float3 tangent_u = make_float3(cp, 0.0f, -sp);
    const float3 tangent_v = cross(normal, tangent_u);

    float4 texture_results[16];
    const unsigned int material_idx = params.current_material;
    const unsigned int tc_idx = mdl_df_target_code_indices[material_idx];
    const char *arg_block = params.arg_block_list[mdl_df_arg_block_indices[material_idx]];

    mi::mdl::Shading_state_material state = {
        normal,
        normal,
        position,
        0.0f,
        &uvw,
        &tangent_u,
        &tangent_v,
        texture_results,
        params.tc_data[tc_idx].ro_data_segment,
        id,
        id,
        0
    };

    Texture_handler tex_handler;
    tex_handler.vtable       = &tex_vtable;   // only required in 'vtable' mode, otherwise NULL
    tex_handler.num_textures = params.tc_data[tc_idx].num_textures;
    tex_handler.textures     = params.tc_data[tc_idx].textures;

    mi::mdl::Resource_data res_data = {NULL, &tex_handler};

    mdl_df_functions_init[material_idx](&state, &res_data, NULL, arg_block);

    float3 result = make_float3(0.0f, 0.0f, 0.0f);

    union {
        mi::mdl::Bsdf_sample_data   sample_data;
        mi::mdl::Bsdf_evaluate_data eval_data;
        mi::mdl::Bsdf_pdf_data      pdf_data;
    };

    // initialize shared fields
    sample_data.ior1 = make_float3(1.0f, 1.0f, 1.0f);
    sample_data.ior2.x = MDL_CORE_BSDF_USE_MATERIAL_IOR;
    sample_data.k1 = make_float3(-dir.x, -dir.y, -dir.z);


    // importance sample environment light
    if (params.mdl_test_type != MDL_TEST_SAMPLE && params.mdl_test_type != MDL_TEST_NO_ENV)
    {
        const float xi0 = curand_uniform(&rand_state);
        const float xi1 = curand_uniform(&rand_state);
        const float xi2 = curand_uniform(&rand_state);

        float3 light_dir;
        float pdf;
        const float3 f = environment_sample(light_dir, pdf, make_float3(xi0, xi1, xi2), params);

        const float cos_theta = dot(light_dir, normal);
        if (cos_theta > 0.0f && pdf > 0.0f) {
            eval_data.k2 = light_dir;

            mdl_df_functions_evaluate[material_idx](
                &eval_data, &state, &res_data, NULL, arg_block);

            const float mis_weight =
                (params.mdl_test_type == MDL_TEST_EVAL) ? 1.0f : pdf / (pdf + eval_data.pdf);
            result += f * eval_data.bsdf * mis_weight;
        }
    }

    // importance sample BSDF
    if (params.mdl_test_type != MDL_TEST_EVAL && params.mdl_test_type != MDL_TEST_NO_ENV)
    {
        sample_data.xi.x = curand_uniform(&rand_state);
        sample_data.xi.y = curand_uniform(&rand_state);
        sample_data.xi.z = curand_uniform(&rand_state);

        mdl_df_functions_sample[material_idx](
            &sample_data, &state, &res_data, NULL, arg_block);

        if (sample_data.event_type != mi::mdl::BSDF_EVENT_ABSORB)
        {
            float pdf;
            const float3 f = environment_eval(pdf, sample_data.k2, params);

            float bsdf_pdf;
            if (params.mdl_test_type == MDL_TEST_MIS_PDF) {
                const float3 k2 = sample_data.k2;
                pdf_data.k2 = k2;
                mdl_df_functions_pdf[material_idx](
                    &pdf_data, &state, &res_data, NULL, arg_block);
                bsdf_pdf = pdf_data.pdf;
            }
            else
                bsdf_pdf = sample_data.pdf;

            const bool is_specular =
                (sample_data.event_type & mi::mdl::BSDF_EVENT_SPECULAR) != 0;
            if (is_specular || bsdf_pdf > 0.0f) {
                const float mis_weight = is_specular ||
                    (params.mdl_test_type == MDL_TEST_SAMPLE) ? 1.0f : bsdf_pdf / (pdf + bsdf_pdf);
                result += f * sample_data.bsdf_over_pdf * mis_weight;
            }
        }
    }

    // compute direct lighting for point light
    if (params.light_intensity.x > 0.0f ||
        params.light_intensity.y > 0.0f ||
        params.light_intensity.z > 0.0f)
    {
        float3 to_light = params.light_pos - position;
        if (dot(to_light, normal) > 0.0f) {

            const float inv_squared_dist = 1.0f / squared_length(to_light);
            eval_data.k2 = to_light * sqrtf(inv_squared_dist);
            
            const float3 f = params.light_intensity * inv_squared_dist * (float)(0.25 / M_PI);

            mdl_df_functions_evaluate[material_idx](
                &eval_data, &state, &res_data, NULL, arg_block);

            result += f * eval_data.bsdf;
        }
    }

    return isfinite(result.x) && isfinite(result.y) && isfinite(result.z)
        ? result : make_float3(0.0f, 0.0f, 0.0f);
}

// exposure + simple Reinhard tonemapper + gamma
__device__ inline unsigned int display(float3 val, const float tonemap_scale)
{
    val *= tonemap_scale;
    const float burn_out = 0.1f;
    val.x *= (1.0f + val.x * burn_out) / (1.0f + val.x);
    val.y *= (1.0f + val.y * burn_out) / (1.0f + val.y);
    val.z *= (1.0f + val.z * burn_out) / (1.0f + val.z);
    const unsigned int r =
        (unsigned int)(255.0 * fminf(powf(fmaxf(val.x, 0.0f), (float)(1.0 / 2.2)), 1.0f));
    const unsigned int g =
        (unsigned int)(255.0 * fminf(powf(fmaxf(val.y, 0.0f), (float)(1.0 / 2.2)), 1.0f));
    const unsigned int b =
        (unsigned int)(255.0 * fminf(powf(fmaxf(val.z, 0.0f), (float)(1.0 / 2.2)), 1.0f));
    return 0xff000000 | (r << 16) | (g << 8) | b;
}

// CUDA kernel rendering simple geometry with IBL
extern "C" __global__ void render_sphere_kernel(
    const Kernel_params kernel_params)
{
    const unsigned int x = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= kernel_params.resolution.x || y >= kernel_params.resolution.y)
        return;

    const unsigned int idx = y * kernel_params.resolution.x + x;
    Rand_state rand_state;
    const unsigned int num_dim = 8; // 2 camera, 3 BSDF, 3 environment
    curand_init(idx, /*subsequence=*/0, kernel_params.iteration_start * num_dim, &rand_state);

    float3 value = make_float3(0.0f, 0.0f, 0.0f);
    for (unsigned int s = 0; s < kernel_params.iteration_num; ++s)
    {
        const float dx = curand_uniform(&rand_state);
        const float dy = curand_uniform(&rand_state);
        value += render_sphere(
            rand_state,
            kernel_params,
            make_float2(
                ((float)x + dx) / (float)kernel_params.resolution.x,
                ((float)y + dy) / (float)kernel_params.resolution.y));
    }
    value *= 1.0f / (float)kernel_params.iteration_num;


    // accumulate
    if (kernel_params.iteration_start == 0)
        kernel_params.accum_buffer[idx] = value;
    else {
        kernel_params.accum_buffer[idx] = kernel_params.accum_buffer[idx] +
            (value - kernel_params.accum_buffer[idx]) *
                ((float)kernel_params.iteration_num /
                    (float)(kernel_params.iteration_start + kernel_params.iteration_num));
    }

    // update display buffer
    if (kernel_params.display_buffer)
        kernel_params.display_buffer[idx] =
            display(kernel_params.accum_buffer[idx], kernel_params.exposure_scale);
}
