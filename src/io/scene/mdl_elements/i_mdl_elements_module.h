/***************************************************************************************************
 * Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
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
 **************************************************************************************************/

#ifndef IO_SCENE_MDL_ELEMENTS_I_MDL_ELEMENTS_MODULE_H
#define IO_SCENE_MDL_ELEMENTS_I_MDL_ELEMENTS_MODULE_H

#include <mi/base/enums.h>
#include <mi/base/handle.h>

#include <vector>
#include <base/data/db/i_db_access.h>
#include <base/data/db/i_db_tag.h>
#include <io/scene/scene/i_scene_scene_element.h>

#include "i_mdl_elements_material_instance.h"
#include "i_mdl_elements_utilities.h"

namespace mi {

namespace mdl {
class IAnnotation_block;
class IGenerated_code_dag;
class IMDL;
class IModule;
}

namespace neuraylib { class IReader; }

}

namespace MI {

namespace DB { class Transaction; }

namespace MDL {

class IAnnotation_block;
class IExpression_factory;
class IExpression_list;
class IType_factory;
class IValue_factory;
class IValue_bsdf_measurement;
class IValue_light_profile;
class IValue_texture;
class Symbol_importer;
class ICall;

/// Represents data to describe a parameter. Used by Material_data below.
class Parameter_data
{
public:
    /// The path that identifies the subexpression that becomes the default of a new parameter.
    std::string m_path;
    /// The name of the new parameter.
    std::string m_name;
    /// Indicates whether the parameter should be forced to be uniform.
    bool m_enforce_uniform;
    /// The annotations for the parameter. So far, only annotations with a single string argument
    /// are supported. The name of the annotation needs to be the fully-qualified MDL name (starting
    /// with a double colon, with signature). \n
    /// Note that the values in \p m_annotations are copied; passing an annotation block obtained
    /// from another MDL interface does not create a link between both instances. \n
    /// \c NULL is a valid value which is handled like an empty annotation block.
    mi::base::Handle<const IAnnotation_block> m_annotations;
};

/// Represents data needed to create a new material based on an existing material. Used by one of
/// the #create_module() overloads below.
class Material_data
{
public:
    /// The name of the new material (non-qualified, without module prefix). The DB name of the new
    /// material is created by prefixing this name with the DB name of the new module plus "::".
    std::string m_material_name;
    /// The tag of the prototype (material instance or function call to the material constructor)
    /// for the new material.
    DB::Tag m_prototype_tag;
    /// The parameters of the new material.
    std::vector<Parameter_data> m_parameters;
    /// The material does not inherit any annotations from the prototype. This member allows to
    /// specify annotations for the material, i.e., for the material declaration itself (but not for
    /// its arguments). So far, only annotations with a single string argument are supported.
    /// The name of the annotation needs to be the fully-qualified MDL name (starting with a double
    /// colon, with signature). \n
    /// Note that the values in \p m_annotations are copied; passing an annotation block obtained
    /// from another MDL interface does not create a link between both instances. \n
    /// \c NULL is a valid value which is handled like an empty annotation block.
    mi::base::Handle<const IAnnotation_block> m_annotations;
};

/// Represents data needed to create a variant. Used by one of the #create_module() overloads below.
class Variant_data
{
public:
    /// The name of the variant (non-qualified, without module prefix). The DB name of the variant
    /// is created by prefixing this name with the DB name of the new module plus "::".
    std::string m_variant_name;
    /// The tag of the prototype (material or function definition) for this variant.
    DB::Tag m_prototype_tag;
    /// The variant inherits the defaults from the prototype. This member allows to change the
    /// defaults and/or to add new defaults. The type of an expression in the expression list must
    /// match the type of the parameter of the same name of the prototype. \n
    /// Note that the expressions in \p m_defaults are copied; passing an expression list obtained
    /// from another MDL interface does not create a link between both instances. \n
    /// \c NULL is a valid value which is handled like an empty expression list.
    mi::base::Handle<const IExpression_list> m_defaults;
    /// The variant does not inherit any annotations from the prototype. This member allows to
    /// specify annotations for the variant, i.e., for the material declaration itself (but not for
    /// its arguments). So far, only annotations with a single string argument are supported.
    /// The name of the annotation needs to be the fully-qualified MDL name (starting with a double
    /// colon, with signature). \n
    /// Note that the values in \p m_annotations are copied; passing an annotation block obtained
    /// from another MDL interface does not create a link between both instances. \n
    /// \c NULL is a valid value which is handled like an empty annotation block.
    mi::base::Handle<const IAnnotation_block> m_annotations;
};

/// The class ID for the #Mdl_module class.
static const SERIAL::Class_id ID_MDL_MODULE = 0x5f4d6d6f; // '_Mmo'

class Mdl_module : public SCENE::Scene_element<Mdl_module, ID_MDL_MODULE>
{
public:

    /// Factory (public, loads the module from file and creates the DB element if needed).
    ///
    /// Looks up the DB element for \p module_name. If it exists, the method returns 0. Otherwise,
    /// the method loads the MDL module from file, creates the DB element, and stores it in the DB
    /// (storing it is required since the method also creates DB elements for all contained
    /// definitions which need the tag of their module). If necessary, DB elements for imported
    /// modules are created recursively, too.
    ///
    /// \param transaction     The DB transaction to use.
    /// \param module_name     The fully-qualified MDL module name (including package names, starts
    ///                        with "::").
    /// \param[out] messages   Messages generated by the MDL compiler.
    /// \return
    ///           -  1: Success (module exists already, loading from file was skipped).
    ///           -  0: Success (module was actually loaded from file).
    ///           - -1: The module name \p module_name is invalid.
    ///           - -2: Failed to find or to compile the module \p module_name.
    ///           - -3: The DB name for an imported module is already in use but is not an MDL
    ///                 module, or the DB name for a definition in this module is already in use.
    ///           - -4: Initialization of an imported module failed.
    static mi::Sint32 create_module(
        DB::Transaction* transaction,
        const char* module_name,
        std::vector<Message>* messages = 0);

    /// Factory (public, loads the module from string and creates the DB element if needed).
    ///
    /// Looks up the DB element for \p module_name. If it exists, the method returns 0. Otherwise,
    /// the method loads the MDL module from string, creates the DB element, and stores it in the DB
    /// (storing it is required since the method also creates DB elements for all contained
    /// definitions which need the tag of their module). If necessary, DB elements for imported
    /// modules are created recursively, too.
    ///
    /// \param transaction     The DB transaction to use.
    /// \param module_name     The fully-qualified MDL module name (including package names, starts
    ///                        with "::").
    /// \param module_source   The source code of the MDL module.
    /// \param[out] messages   Messages generated by the MDL compiler.
    /// \return
    ///           -  1: Success (module exists already, creating from \p module_source was skipped).
    ///           -  0: Success (module was actually created from \p module_source).
    ///           - -1: The module name \p module_name is invalid.
    ///           - -2: Failed to find or to compile the module \p module_name.
    ///           - -3: The DB name for an imported module is already in use but is not an MDL
    ///                 module, or the DB name for a definition in this module is already in use.
    ///           - -4: Initialization of an imported module failed.
    static mi::Sint32 create_module(
        DB::Transaction* transaction,
        const char* module_name,
        mi::neuraylib::IReader* module_source,
        std::vector<Message>* messages = 0);

    /// Factory (public, creates a module with multiple variants and creates the DB element if
    /// needed).
    ///
    /// \param transaction     The DB transaction to use.
    /// \param module_name     The fully-qualified MDL module name (including package names, starts
    ///                        with "::").
    /// \param variant_data    An array with the data for each variant to be created. For details
    ///                        #see Variant_data.
    /// \param variant_count   The length of \p variant_data.
    /// \param[out] messages   Messages generated by the MDL compiler.
    /// \return
    ///           -   1: Success (module exists already, creating from \p variant_data was
    ///                  skipped).
    ///           -   0: Success (module was actually created with the variants as its only material
    ///                  and function definitions).
    ///           -  -1: The module name \p module_name is invalid.
    ///           -  -2: Failed to compile the module \p module_name.
    ///           -  -3: The DB name for an imported module is already in use but is not an MDL
    ///                  module, or the DB name for a definition in this module is already in use.
    ///           -  -5: The DB element of one of the prototypes has the wrong type.
    ///           -  -4: Initialization of an imported module failed.
    ///           -  -6: A default for a non-existing parameter was provided.
    ///           -  -7: The type of a default does not have the correct type.
    ///           -  -8: Unspecified error.
    ///           -  -9: One of the annotation arguments is wrong (wrong argument name, not a
    ///                  constant expression, or the argument type does not match the parameter
    ///                  type).
    ///           - -10: One of the annotations does not exist or it has a currently unsupported
    ///                  parameter type like deferred-sized arrays.
    static mi::Sint32 create_module(
        DB::Transaction* transaction,
        const char* module_name,
        const Variant_data* variant_data,
        mi::Size variant_count,
        std::vector<Message>* messages = 0);


    /// Creates a value referencing a texture identified by an MDL file path.
    ///
    /// \param transaction   The transaction to be used.
    /// \param file_path     The absolute MDL file path that identifies the texture. The MDL
    ///                      search paths are used to resolve the file path. See section 2.2 in
    ///                      [\ref MDLLS] for details.
    /// \param shape         The value that is returned by #IType_texture::get_shape() on the type
    ///                      corresponding to the return value.
    /// \param gamma         The value that is returned by #TEXTURE::Texture::get_gamma()
    ///                      on the DB element referenced by the return value.
    /// \param shared        Indicates whether you want to re-use the DB elements for that texture
    ///                      if it has already been loaded, or if you want to create new DB elements
    ///                      in all cases. Note that sharing is based on the location where the
    ///                      texture is finally located and includes sharing with instances that
    ///                      have not explicitly been loaded via this method, e.g., textures in
    ///                      defaults.
    /// \param errors        An optional pointer to an #mi::Sint32 to which an error code will be
    ///                      written. The error codes have the following meaning:
    ///                      -  0: Success.
    ///                      - -1: Invalid parameters (\c NULL pointer).
    ///                      - -2: The file path is not an absolute MDL file path.
    ///                      - -3: Failed to resolve the given file path, or no suitable image
    ///                            plugin available.
    /// \return              The value referencing the texture, or \c NULL in case of failure.
    static IValue_texture* create_texture(
        DB::Transaction* transaction,
        const char* file_path,
        IType_texture::Shape shape,
        mi::Float32 gamma,
        bool shared,
        mi::Sint32* errors = 0);

    /// Creates a value referencing a light profile identified by an MDL file path.
    ///
    /// \param transaction   The transaction to be used.
    /// \param file_path     The absolute MDL file path that identifies the light profile. The MDL
    ///                      search paths are used to resolve the file path. See section 2.2 in
    ///                      [\ref MDLLS] for details.
    /// \param shared        Indicates whether you want to re-use the DB element for that light
    ///                      profile if it has already been loaded, or if you want to create a new
    ///                      DB element in all cases. Note that sharing is based on the location
    ///                      where the light profile is finally located and includes sharing with
    ///                      instances that have not explicitly been loaded via this method, e.g.,
    ///                      light profiles in defaults.
    /// \param errors        An optional pointer to an #mi::Sint32 to which an error code will be
    ///                      written. The error codes have the following meaning:
    ///                      -  0: Success.
    ///                      - -1: Invalid parameters (\c NULL pointer).
    ///                      - -2: The file path is not an absolute MDL file path.
    ///                      - -3: Failed to resolve the given file path.
    /// \return              The value referencing the light profile, or \c NULL in case of failure.
    static IValue_light_profile* create_light_profile(
        DB::Transaction* transaction,
        const char* file_path,
        bool shared,
        mi::Sint32* errors = 0);

    /// Creates a value referencing a BSDF measurement identified by an MDL file path.
    ///
    /// \param transaction   The transaction to be used.
    /// \param file_path     The absolute MDL file path that identifies the BSDF measurement. The
    ///                      MDL search paths are used to resolve the file path. See section 2.2 in
    ///                      [\ref MDLLS] for details.
    /// \param shared        Indicates whether you want to re-use the DB element for that BSDF
    ///                      measurement if it has already been loaded, or if you want to create a
    ///                      new DB element in all cases. Note that sharing is based on the location
    ///                      where the BSDF measurement is finally located and includes sharing with
    ///                      instances that have not explicitly been loaded via this method, e.g.,
    ///                      BSDF measurements in defaults.
    /// \param errors        An optional pointer to an #mi::Sint32 to which an error code will be
    ///                      written. The error codes have the following meaning:
    ///                      -  0: Success.
    ///                      - -1: Invalid parameters (\c NULL pointer).
    ///                      - -2: The file path is not an absolute MDL file path.
    ///                      - -3: Failed to resolve the given file path.
    /// \return              The value referencing the BSDF measurement, or \c NULL in case of
    ///                      failure.
    static IValue_bsdf_measurement* create_bsdf_measurement(
        DB::Transaction* transaction,
        const char* file_path,
        bool shared,
        mi::Sint32* errors = 0);

    /// Default constructor.
    ///
    /// Does not create a valid instance, to be used by the deserializer only. Use one of the
    /// factories above instead.
    Mdl_module();

    /// Copy constructor.
    ///
    /// The copy constructor does the same as the default one. It is just explicit to avoid pulling
    /// in mi/mdl/mdl_generated_dag.h, mi/mdl/mdl_mdl.h, and mi/mdl/mdl_modules.h for Visual Studio.
    Mdl_module( const Mdl_module& other);

    // methods corresponding to mi::neuraylib::IModule

    const char* get_filename() const;

    const char* get_api_filename() const;

    const char* get_mdl_name() const;

    mi::Size get_import_count() const;

    DB::Tag get_import( mi::Size index) const;

    const IType_list* get_types() const;

    const IValue_list* get_constants() const;

    mi::Size get_function_count() const;

    DB::Tag get_function( DB::Transaction* transaction, mi::Size index) const;

    const char* get_function_name( mi::Size index) const;

    mi::Size get_material_count() const;

    DB::Tag get_material( DB::Transaction* transaction, mi::Size index) const;

    const char* get_material_name( mi::Size index) const;

    const IAnnotation_block* get_annotations() const;

    bool is_standard_module() const;

    const std::vector<std::string> get_function_overloads(
        DB::Transaction* transaction,
        const char* name,
        const IExpression_list* arguments = 0) const;

    const std::vector<std::string> get_function_overloads_by_signature(
        DB::Transaction* transaction,
        const char* name,
        const char* param_sig) const;

    mi::Size get_resources_count() const;

    const char* get_resource_mdl_file_path(mi::Size index) const;

    DB::Tag get_resource_tag(mi::Size index) const;

    const IType_resource* get_resource_type(mi::Size index) const;

    // internal methods

    /// Returns the underlying MDL module.
    ///
    /// Never returns NULL.
    const mi::mdl::IModule* get_mdl_module() const;

    /// Returns the DAG representation of this module.
    const mi::mdl::IGenerated_code_dag* get_code_dag() const;

    /// Indicates whether \p name is a valid module name.
    ///
    /// \param name  the module name to check
    /// \param mdl   the MDL interface
    static bool is_valid_module_name( const char* name, const mi::mdl::IMDL* mdl);

    /// Improved version of SERIAL::Serializable::dump().
    ///
    /// \param transaction   The DB transaction (for name lookups and tag versions). Can be \c NULL.
    void dump( DB::Transaction* transaction) const;

    // methods of SERIAL::Serializable

    const SERIAL::Serializable* serialize( SERIAL::Serializer* serializer) const;

    SERIAL::Serializable* deserialize( SERIAL::Deserializer* deserializer);

    void dump() const  { dump( /*transaction*/ 0); }

    // methods of DB::Element_base

    size_t get_size() const;

    DB::Journal_type get_journal_flags() const;

    Uint bundle( DB::Tag* results, Uint size) const;

    // methods of SCENE::Scene_element_base

    void get_scene_element_references( DB::Tag_set* result) const;

private:

    /// Adds a variant to an MDL module.
    ///
    /// \note Does not work for arbitrary MDL modules. The only supported workflow is as follows:
    ///       Create an empty module, add several variants via this method, analyze the module, and
    ///       do not modify it any more.
    ///
    /// \param symbol_importer The importer for new symbols found in this variant.
    /// \param transaction     The DB transaction to use.
    /// \param module          The MDL module to which the variant is to be added.
    /// \param prototype_tag   The prototype for the variant.
    /// \param variant_name    The name of the variant (non-qualified, without module prefix). The
    ///                        DB name of the variant is created by prefixing this name with the DB
    ///                        name of the new module plus "::".
    /// \param defaults        The variant inherits the defaults from the prototype. This parameter
    ///                        allows to change the defaults and/or to add new defaults. The type of
    ///                        an attribute in the attribute set must match the type of the
    ///                        parameter of the same name of the prototype. \n
    ///                        Note that the values in \p defaults are copied; passing an attribute
    ///                        set obtained from another MDL interface does not create a link
    ///                        between both instances. \n
    ///                        \c NULL is a valid argument which is handled like an empty attribute
    ///                        set.
    /// \param annotations     The variant does not inherit any annotations from the prototype. This
    ///                        parameter allows to specify annotations for the variant, i.e., for
    ///                        the material declaration itself (but not for its arguments). So far,
    ///                        only annotations with a single string argument are supported.
    ///                        Therefore, the type of an attribute in the attribute set must be a
    ///                        struct with a single string member. The name of that member does not
    ///                        matter. The name of the attribute identifies the annotation
    ///                        (fully-qualified MDL name starting with a double colon, without
    ///                        signature). \n Note that the values in \p annotations are copied;
    ///                        passing an attribute set obtained from another MDL interface does not
    ///                        create a link between both instances. \n \c NULL is a valid value
    ///                        which is handled like an empty attribute set.
    /// \param[out] messages   Messages generated by the MDL compiler.
    /// \return
    ///           -   1: Success (module exists already, creating from \p variant_data was
    ///                  skipped).
    ///           -   0: Success (module was actually created with the variants as its only material
    ///                  definitions).
    ///           -  -1: The module name \p module_name is invalid.
    ///           -  -2: Failed to compile the module \p module_name.
    ///           -  -3: The DB name for an imported module is already in use but is not an MDL
    ///                  module, or the DB name for a definition in this module is already in use.
    ///           -  -4: Initialization of an imported module failed.
    ///           -  -6: A default for a non-existing parameter was provided.
    ///           -  -7: The type of a default does not have the correct type.
    ///           -  -8: Unspecified error.
    ///           -  -9: One of the annotation arguments is wrong (wrong argument name, not a
    ///                  constant expression, or the argument type does not match the parameter
    ///                  type).
    ///           - -10: One of the annotations does not exist or it has a currently unsupported
    ///                  parameter type like deferred-sized arrays.
    template <class T>
    static mi::Sint32 add_variant(
        Symbol_importer& symbol_importer,
        DB::Transaction* transaction,
        mi::mdl::IModule* module,
        DB::Access<T> prototype,
        const char* variant_name,
        const IExpression_list* defaults,
        const IAnnotation_block* annotations,
        std::vector<Message>* messages);

    /// Check if it is possible to enforce the uniform property if the new parameter is uniform
    ///
    /// \param[in]  transaction      current transaction
    /// \param[in]  args             argument list
    /// \param[in]  param_types      parameter type list
    /// \param[in]  path             path to the newly parameter
    /// \param[in]  expr             the expression that will be turned into parameter
    /// \param[out] must_be_uniform  \c true iff the newly created parameter must be uniform to
    ///                              enforce the uniform property
    ///
    /// \return true if possible, false if not possible
    static bool can_enforce_uniform(
        DB::Transaction* transaction,
        mi::base::Handle<const IExpression_list> const &args,
        mi::base::Handle<const IType_list> const &param_types,
        std::string const &path,
        mi::base::Handle<const IExpression> const &expr,
        bool &must_be_uniform);

    /// \return
    ///           -   1: Success (module exists already, creating from \p variant_data was
    ///                  skipped).
    ///           -   0: Success (module was actually created with the variants as its only material
    ///                  definitions).
    ///           -  -9: One of the annotation arguments is wrong (wrong argument name, not a
    ///                  constant expression, or the argument type does not match the parameter
    ///                  type).
    ///           - -10: One of the annotations does not exist or it has a currently unsupported
    ///                  parameter type like deferred-sized arrays.
    ///           - -13: A provided parameter path does not exist.
    ///           - -15: A provided argument is not uniform (but this is required by the parameter)
    static mi::Sint32 add_material(
        Symbol_importer& symbol_importer,
        DB::Transaction* transaction,
        mi::mdl::IModule* module,
        const ICall& callee,
        const Material_data& md,
        std::vector<Message>* messages);

    /// Creates an mi::mdl annotation block from an MI::MDL annotation block.
    ///
    /// \param transaction       The DB transaction to use.
    /// \param module            The MDL module to which the annotation is to be added.
    /// \param annotation_block  The annotation block which is to be converted.
    /// \param symbol_importer   The importer for new symbols found in this annotation.
    /// \param[out] mdl_annotation_block    The resulting annotation block, or \c NULL if
    ///                                     \p annotation_block is \c NULL.
    /// \return
    ///                          -   0: Success.
    ///                          -  -9: One of the annotation arguments is wrong (wrong argument
    ///                                 name, not a constant expression, or the argument type does
    ///                                 not match the parameter type).
    ///                          - -10: One of the annotations does not exist or it has a currently
    ///                                 unsupported parameter type like deferred-sized arrays.
    static mi::Sint32 create_annotations(
        DB::Transaction* transaction,
        mi::mdl::IModule* module,
        const IAnnotation_block* annotation_block,
        Symbol_importer* symbol_importer,
        mi::mdl::IAnnotation_block* &mdl_annotation_block);

    /// Adds an annotation to an annotation block.
    ///
    /// \param transaction       The DB transaction to use.
    /// \param module            The MDL module to which the annotation is to be added.
    /// \param annotation_block  The annotation block to which the annotation is to be added.
    /// \param annotation_name   The fully qualified name of the annotation (The name of the
    ///                          starting with a double colon, including signature).
    /// \param annotation_args   The arguments of the annotation.
    /// \return
    ///                          -   0: Success.
    ///                          -  -9: One of the annotation arguments is wrong (wrong argument
    ///                                 name, not a constant expression, or the argument type does
    ///                                 not match the parameter type).
    ///                          - -10: One of the annotations does not exist or it has a currently
    ///                                 unsupported parameter type like deferred-sized arrays.
    static mi::Sint32 add_annotation(
        DB::Transaction* transaction,
        mi::mdl::IModule* module,
        mi::mdl::IAnnotation_block* annotation_block,
        const char* annotation_name,
        const IExpression_list* annotation_args);

    /// Factory (private, takes an mi::mdl::IModule and creates the DB element if needed).
    ///
    /// Looks up the DB element for \p module. If it exists, the method stores its tag in
    /// \p module_tag and returns 1. Otherwise, the method creates the DB element, and stores it in
    /// the DB (storing it is required since the method also creates DB elements for all contained
    /// definitions which need the tag of their module). If necessary, DB elements for imported
    /// modules are created recursively, too.
    ///
    /// \param transaction       The DB transaction to use.
    /// \param mdl               The IMDL instance.
    /// \param module            The corresponding MDL module.
    /// \param[out] messages     Messages generated by the MDL compiler.
    /// \param[out] module_tag   The tag of the already existing or just created DB element (only
    ///                          valid if the return value is 0).
    /// \return
    ///
    ///           -  1: Success (module exists already, was not created from \p module).
    ///           -  0: Success (module was actually created from \p module).
    ///           - -2: \p module is an invalid module.
    ///           - -3: The DB name for an imported module is already in use but is not an MDL
    ///                 module, or the DB name for a definition in this module is already in use.
    ///           - -4: Initialization of an imported module failed.
    static mi::Sint32 create_module_internal(
        DB::Transaction* transaction,
        mi::mdl::IMDL* mdl,
        const mi::mdl::IModule* module,
        std::vector<Message>* messages,
        DB::Tag* module_tag = 0);

    /// Constructor.
    ///
    /// This constructor is used by the factory #create_module_internal(). The parameters are used
    /// to initialize the parameters in the obvious way (m_name and m_filename are taken from
    /// module). The annotations are taken from m_code_dag and converted using the transaction.
    Mdl_module(
        DB::Transaction* transaction,
        mi::mdl::IMDL* mdl,
        const mi::mdl::IModule* module,
        mi::mdl::IGenerated_code_dag* code_dag,
        const std::vector<DB::Tag>& imports,
        const std::vector<std::string>& functions,
        const std::vector<std::string>& materials);

    /// The main MDL interface.
    mi::base::Handle<mi::mdl::IMDL> m_mdl;
    /// The underlying MDL module.
    mi::base::Handle<const mi::mdl::IModule> m_module;
    /// The DAG representation of this module.
    mi::base::Handle<const mi::mdl::IGenerated_code_dag> m_code_dag;

    mi::base::Handle<IType_factory> m_tf;        ///< The type factory.
    mi::base::Handle<IValue_factory> m_vf;       ///< The value factory.
    mi::base::Handle<IExpression_factory> m_ef;  ///< The expression factory.

    std::string m_name;                          ///< The MDL name.

    /// The filename of the module (might be empty). Contains archive and member names in case of
    /// archives.
    std::string m_file_name;

    /// The filename of the module (might be empty). Contains only the archive name in case of
    /// archives.
    std::string m_api_file_name;

    std::vector<DB::Tag> m_imports;              ///< The imported modules.
    mi::base::Handle<IType_list> m_types;        ///< The types.
    mi::base::Handle<IValue_list> m_constants;   ///< The constants.
    mi::base::Handle<IAnnotation_block> m_annotations; ///< Module annotations.

    // The next two vectors use strings instead of tags to avoid cyclic dependencies (the
    // definitions already reference the module). The named definition might no longer exist in the
    // DB and m_module and m_code_dag may contain data for definitions that do no longer exist as
    // DB element.
    std::vector<std::string> m_functions;        ///< Names of the contained function definitions.
    std::vector<std::string> m_materials;        ///< Names of the contained material definitions.

    // Resource tags
    // This vector has en entry for each item in mi::mdl::IModules' resource table
    // and contains a list of tags corresponding to the resource (a texture used with different
    // gamma modes is stored per gamma mode in the DB, so one file can refer to more than one
    // db element). It may contain invalid tags for non-existing resources.
    std::vector<std::vector<DB::Tag> > m_resource_reference_tags;
};

} // namespace MDL

} // namespace MI

#endif // IO_SCENE_MDL_ELEMENTS_I_MDL_ELEMENTS_MODULE_H

