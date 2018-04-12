/*
 * File: lib-engine/asset-resolver.hpp
 * This file provies a resolution mechanism for asset handles to be associated and loaded with their
 * underlying resource, from either memory or disk. Handles are serialized by a variety of containers,
 * including `poly_scene`, `material_library`, and `shader_library`. During deserialization, these
 * handles are not assocated with any actual resource. This class compares handles in the containers
 * to assigned assets in the `asset_handle<T>` table. If an unassigned resource is found, the asset
 * handle identifier is used as a key to recursively search an asset folder for a matching filename
 * where the asset is loaded.
 *
 * (todo) Presently we assume that all handle identifiers refer to unique assets, however this is a weak
 * assumption and is likely untrue in practice and should be fixed.
 *
 * (todo) The asset_resolver is single-threaded and called on the main thread because it may also
 * touch GPU resources. This must be changed to load asynchronously. 
 */

#pragma once

#ifndef polymer_asset_resolver_hpp
#define polymer_asset_resolver_hpp

#include "asset-handle.hpp"
#include "asset-handle-utils.hpp"
#include "string_utils.hpp"
#include "scene.hpp"

namespace polymer
{
    template <typename T>
    void remove_duplicates(std::vector<T> & vec)
    {
        std::sort(vec.begin(), vec.end());
        vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
    }

    // The purpose of an asset resolver is to match an asset_handle to an asset on disk. This is done
    // for scene objects (meshes, geometry) and materials (shaders, textures). 
    class asset_resolver
    {
        // Unresolved asset names
        std::vector<std::string> mesh_names;
        std::vector<std::string> geometry_names;
        std::vector<std::string> shader_names;
        std::vector<std::string> material_names;
        std::vector<std::string> texture_names;

        // What to do if we find multiples? 
        void walk_directory(path root)
        {
            scoped_timer t("load + resolve");

            for (auto & entry : recursive_directory_iterator(root))
            {
                const size_t root_len = root.string().length(), ext_len = entry.path().extension().string().length();
                auto path = entry.path().string(), name = path.substr(root_len + 1, path.size() - root_len - ext_len - 1);
                for (auto & chr : path) if (chr == '\\') chr = '/';

                const auto ext = entry.path().extension(); // also includes the dot

                if (ext == ".png" || ext == ".PNG" || ext == ".tga" || ext == ".jpg" || ext == ".jpeg")
                {
                    const auto filename_no_extension = get_filename_without_extension(path);
                    for (auto & name : texture_names)
                    {
                        if (name == filename_no_extension)
                        {
                            create_handle_for_asset(name.c_str(), load_image(path, false));
                            Logger::get_instance()->assetLog->info("resolved {} ({})", name, typeid(GlTexture2D).name());
                        }
                    }
                }

                if (ext == ".obj" || ".OBJ")
                {
                    // todo - .mesh, .fbx, .gltf
                    // all meshes are currently intrinsics, handled separately (for now)
                }
            }
        }

    public:

        void resolve(const std::string & asset_dir, poly_scene * scene, material_library * library)
        {
            assert(scene != nullptr && library != nullptr && asset_dir.size() > 1);

            for (auto & obj : scene->objects)
            {
                if (auto * mesh = dynamic_cast<StaticMesh*>(obj.get()))
                {
                    material_names.push_back(mesh->mat.name);
                    mesh_names.push_back(mesh->mesh.name);
                    geometry_names.push_back(mesh->geom.name);
                }
            }

            remove_duplicates(material_names);
            remove_duplicates(mesh_names);
            remove_duplicates(geometry_names);

            for (auto & mat : library->instances)
            {
                if (auto * pbr = dynamic_cast<polymer_pbr_standard*>(mat.second.get()))
                {
                    shader_names.push_back(pbr->shader.name);
                    texture_names.push_back(pbr->albedo.name);
                    texture_names.push_back(pbr->normal.name);
                    texture_names.push_back(pbr->metallic.name);
                    texture_names.push_back(pbr->roughness.name);
                    texture_names.push_back(pbr->emissive.name);
                    texture_names.push_back(pbr->height.name);
                    texture_names.push_back(pbr->occlusion.name);
                }
            }

            remove_duplicates(shader_names);
            remove_duplicates(texture_names);

            walk_directory(asset_dir);

            // todo - shader_names and material_names need to be resolved somewhat differently, since shaders have includes
            // and materials come from the library.
        }
    };

} // end namespace polymer

/*
// Find missing Geometry and GlMesh asset_handles first
std::unordered_map<std::string, uint32_t> missingGeometryAssets;
std::unordered_map<std::string, uint32_t> missingMeshAssets;

for (auto & obj : scene->objects)
{
if (auto * mesh = dynamic_cast<StaticMesh*>(obj.get()))
{
bool foundGeom = false;
bool foundMesh = false;

for (auto & h : asset_handle<Geometry>::list())
{
if (h.name == mesh->geom.name) foundGeom = true;
}

for (auto & h : asset_handle<GlMesh>::list())
{
if (h.name == mesh->mesh.name) foundMesh = true;
}

if (!foundGeom) missingGeometryAssets[mesh->geom.name] += 1;
if (!foundMesh) missingMeshAssets[mesh->mesh.name] += 1;

}
}

for (auto & e : missingGeometryAssets) std::cout << "Asset table does not have " << e.first << " geometry required by " << e.second << " game object instances" << std::endl;
for (auto & e : missingMeshAssets) std::cout << "Asset table does not have " << e.first << " mesh required by " << e.second << " game object instances" << std::endl;
*/

#endif // end polymer_asset_resolver_hpp