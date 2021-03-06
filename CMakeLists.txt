#**************************************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#*************************************************************************************************/

cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)

# MDL SDK build directories
set(MDL_BASE_FOLDER ${CMAKE_SOURCE_DIR} CACHE FILEPATH "The folder that contains the targets to build and the cmake utility scripts.")
set(MDL_INCLUDE_FOLDER ${CMAKE_SOURCE_DIR}/include CACHE FILEPATH "The folder that interface headers, i.e., mi/base, mi/neuray, ... directories.")
set(MDL_SRC_FOLDER ${CMAKE_SOURCE_DIR}/src CACHE FILEPATH "The folder that contains the sources, i.e., the base, io, mdl, ... directories.")
set(MDL_EXAMPLES_FOLDER ${CMAKE_SOURCE_DIR}/examples CACHE FILEPATH "The folder that contains the mdl examples")

project(MDL LANGUAGES C CXX)

#--------------------------------------------------------------------------------------------------
# configuration options
include(${MDL_BASE_FOLDER}/cmake/build_types.cmake) # defines possible build types and sets the default one

option(MDL_BUILD_SDK_EXAMPLES "Adds MDL SDK examples to the build." ON)
option(MDL_BUILD_CORE_EXAMPLES "Adds MDL Core examples to the build." ON)
option(MDL_LOG_PLATFORM_INFOS "Prints some infos about the current build system (relevant for error reports)." ON)
option(MDL_LOG_DEPENDENCIES "Prints the list of dependencies during the generation step." ON)
option(MDL_LOG_FILE_DEPENDENCIES "Prints the list of files that is copied after a successful build." ON)
set(MDL_ADDITIONAL_COMPILER_OPTIONS "" CACHE STRING "Additional compile options that are passed to each of the projects")

# -------------------------------------------------------------------------------------------------
# general setup
include(${MDL_BASE_FOLDER}/cmake/toplevel_setup.cmake)

#--------------------------------------------------------------------------------------------------
# print system/build information for error reporting
include(${MDL_BASE_FOLDER}/cmake/log_platform_infos.cmake)
MESSAGE(STATUS "MDL_BUILD_SDK_EXAMPLES:             " ${MDL_BUILD_SDK_EXAMPLES})
MESSAGE(STATUS "MDL_BUILD_CORE_EXAMPLES:            " ${MDL_BUILD_CORE_EXAMPLES})
MESSAGE(STATUS "")

# -------------------------------------------------------------------------------------------------
# presets and utility functions used in all CMakeLists
include(${MDL_BASE_FOLDER}/cmake/utilities.cmake)

#--------------------------------------------------------------------------------------------------
# list of modules in defined order

# MDL SDK and MDL CORE
#--------------------------------------------------------------------------------------------------

# first thing to build
add_subdirectory(${MDL_SRC_FOLDER}/base/system/main)
add_subdirectory(${MDL_SRC_FOLDER}/base/system/version)

# third party
add_subdirectory(${MDL_SRC_FOLDER}/base/lib/libzip)
add_subdirectory(${MDL_SRC_FOLDER}/base/lib/zlib)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/compiler/coco)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/jit/llvm)

# libs
add_subdirectory(${MDL_SRC_FOLDER}/base/util/string_utils)
add_subdirectory(${MDL_SRC_FOLDER}/base/util/registry)
add_subdirectory(${MDL_SRC_FOLDER}/base/hal/disk)
add_subdirectory(${MDL_SRC_FOLDER}/base/hal/hal)
add_subdirectory(${MDL_SRC_FOLDER}/base/hal/link)
add_subdirectory(${MDL_SRC_FOLDER}/base/hal/time)
add_subdirectory(${MDL_SRC_FOLDER}/base/lib/config)
add_subdirectory(${MDL_SRC_FOLDER}/base/lib/mem)
add_subdirectory(${MDL_SRC_FOLDER}/base/lib/path)
add_subdirectory(${MDL_SRC_FOLDER}/base/lib/plug)
add_subdirectory(${MDL_SRC_FOLDER}/base/data/attr)
add_subdirectory(${MDL_SRC_FOLDER}/base/data/dblight)
add_subdirectory(${MDL_SRC_FOLDER}/base/data/serial)
add_subdirectory(${MDL_SRC_FOLDER}/io/image)
add_subdirectory(${MDL_SRC_FOLDER}/io/scene)
add_subdirectory(${MDL_SRC_FOLDER}/api/api/mdl)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/codegenerators/generator_code)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/codegenerators/generator_dag)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/compiler/compilercore)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/jit/libbsdf)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/jit/devlib)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/jit/generator_jit)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/no_glsl/generator_stub)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/no_jit/generator_stub)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/runtime)
add_subdirectory(${MDL_SRC_FOLDER}/mdl/integration/mdlnr)
add_subdirectory(${MDL_SRC_FOLDER}/render/mdl/backends)
add_subdirectory(${MDL_SRC_FOLDER}/render/mdl/runtime)

# libs/products
add_subdirectory(${MDL_SRC_FOLDER}/prod/bin/mdlc)
add_subdirectory(${MDL_SRC_FOLDER}/prod/lib/mdl_core)
add_subdirectory(${MDL_SRC_FOLDER}/prod/lib/mdl_sdk)

# PLUGINS
#--------------------------------------------------------------------------------------------------
add_subdirectory(${MDL_SRC_FOLDER}/shaders/plugin/dds)
add_subdirectory(${MDL_SRC_FOLDER}/shaders/plugin/freeimage)

# EXAMPLES
#--------------------------------------------------------------------------------------------------

# third party
if(MDL_ENABLE_OPENGL_EXAMPLES)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/thirdparty/imgui)
endif()
add_subdirectory(${MDL_EXAMPLES_FOLDER}/thirdparty/tinyxml2)

# MDL SDK
if(MDL_BUILD_SDK_EXAMPLES)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/shared)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/archives)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/calls)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/compilation)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/discovery)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/execution_native)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/instantiation)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/modules)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/start_shutdown)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/traversal)

    if(MDL_ENABLE_OPENGL_EXAMPLES AND MDL_ENABLE_CUDA_EXAMPLES)
        add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/df_cuda)
        add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/execution_cuda)
    endif()

    if(MDL_ENABLE_QT_EXAMPLES)
        add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_sdk/mdl_browser)
    endif()
endif()

# MDL CORE
if(MDL_BUILD_CORE_EXAMPLES)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_core/shared)
    add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_core/calls)
    if(MDL_ENABLE_OPENGL_EXAMPLES AND MDL_ENABLE_CUDA_EXAMPLES)
        add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_core/df_cuda)
        add_subdirectory(${MDL_EXAMPLES_FOLDER}/mdl_core/execution_cuda)
    endif()
endif()
