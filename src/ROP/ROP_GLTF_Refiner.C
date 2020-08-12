/*
 * Copyright (c) 2018
 *      Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ROP_GLTF.h"
#include "ROP_GLTF_Refiner.h"
#include <GA/GA_Names.h>
#include <GLTF/GLTF_Types.h>
#include <GLTF/GLTF_Util.h>
#include <GT/GT_AttributeMap.h>
#include <GT/GT_DAIndirect.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimCurveMesh.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_Util.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_Promote.h>
#include <ROP/ROP_Error.h>
#include <UT/UT_Interrupt.h>

#include "ROP_GLTF_ExportRoot.h"

using namespace GLTF_NAMESPACE;

#define DEBUG_GT_POINTSPLIT 0

// Returns -1 if the uv string is invalid
static int
theGetUVLayer(UT_String uv)
{
    if (!uv.startsWith("uv"))
        return -1;

    uv.eraseHead(2);

    if (uv == "")
        return 0;

    if (!uv.isInteger())
        return false;

    return uv.toInt() - 1;
}

////////////////////////////////////

ROP_GLTF_Refiner::ROP_GLTF_Refiner(
    ROP_GLTF_ExportRoot &root,
    GLTF_Node *node,
    const UT_StringHolder &obj_material,
    std::function<GLTF_Handle(UT_StringHolder)> create_material,
    Refine_Options refine_options)
    : myRoot(root),
      myNode(node),
      myObjectMaterial(obj_material),
      myCreateMaterial(create_material),
      myOptions(refine_options)
{
}

ROP_GLTF_Refiner::~ROP_GLTF_Refiner() {}

void
ROP_GLTF_Refiner::refine(const GU_Detail *src, ROP_GLTF_ExportRoot &root,
                         GLTF_Node &node, const UT_StringHolder &obj_material,
                         std::function<GLTF_Handle(UT_StringHolder)> create_material,
                         Refine_Options refine_options)
{
    // Copy detail
    GU_Detail *mdtl = new GU_Detail;
    {
        GU_DetailHandle gdh;
        gdh.allocateAndSet(SYSconst_cast(src), false);
        GU_DetailHandleAutoReadLock rlock(gdh);
        const GU_Detail *src = rlock.getGdp();
        mdtl->duplicate(*src, 0, GA_DATA_ID_CLONE);
    }

    // Refine
    GU_DetailHandle gdh;
    GT_PrimitiveHandle gt_prim;
    GT_RefineParms rparms;

    ROP_GLTF_Refiner refiner(root, &node, obj_material, create_material,
                             refine_options);

    gdh.allocateAndSet(mdtl, false);
    gt_prim = GT_GEODetail::makeDetail(gdh);
    gt_prim->refine(refiner, &rparms);

    gdh.deleteGdp();
}

void
ROP_GLTF_Refiner::addPrimitive(const GT_PrimitiveHandle &prim)
{
    UT_AutoInterrupt progress("Refining Geometry");
    if (progress.wasInterrupted())
        return;

    if (!prim)
        return;

    const GT_PrimitiveType type =
        static_cast<GT_PrimitiveType>(prim->getPrimitiveType());

    GT_Owner owner;
    auto shop_attrib =
        prim->findAttribute(GA_Names::shop_materialpath, owner, 0);

    if (type == GT_PRIM_POLYGON_MESH)
    {
        GLTF_Mesh mesh;
        processPrimPolygon(static_cast<GT_PrimPolygonMesh *>(prim.get()),
                           UT_Matrix4D(1), mesh);
        GLTF_Handle mesh_idx = appendMeshIfNotEmpty(mesh);
        if (mesh_idx != GLTF_INVALID_IDX)
        {
            myNode->mesh = mesh_idx;
        }
    }
    else if (type == GT_PRIM_POLYGON_MESH)
    {
        const GT_PrimCurveMesh *pr =
            static_cast<const GT_PrimCurveMesh *>(prim.get());

        pr->refineToLinear();
    }
    else if (type == GT_GEO_PACKED)
    {
    }
    else if (type == GT_PRIM_INSTANCE)
    {
        processInstance(static_cast<const GT_PrimInstance *>(prim.get()));
    }
    else
    {
        prim->refine(*this);
    }
}

void
ROP_GLTF_Refiner::processPrimPolygon(GT_PrimPolygonMesh *prim,
                                     UT_Matrix4D trans, GLTF_Mesh &mesh)
{
    GT_PrimitiveHandle handle;

    // Attempt to triangulate the mesh
    handle = prim->convex();
    if (handle)
    {
        addMesh(*static_cast<GT_PrimPolygonMesh *>(handle.get()), trans, mesh);
    }
}

bool
ROP_GLTF_Refiner::processInstance(const GT_PrimInstance *instance)
{
    GT_PrimitiveHandle geo = instance->geometry();
    int type = geo->getPrimitiveType();

    if (type == GT_PRIM_POLYGON_MESH)
    {
        // The only attribute this instances currently is materials.

        // Handling for material assignments:
        // If there only instanced material assignments, remove material
        // assigments from the mesh, process the mesh, then create additional
        // GLTF primitives with the new material assignments.

        // Handling for other assignments:
        // Instancing is currently not handled and it just flattens the
        // geometry.
        // Note that other instanced materials can technically be done
        // by using non-interleaved arrays and resusing accessors.

        UT_Matrix4D prim_xform;
        instance->getPrimitiveTransform()->getMatrix(prim_xform, 0);

        GT_TransformArrayHandle xforms = instance->transforms();

        // If there's a primitive attribute on the instance, then add it to
        // the instance as a detail attribute
        auto *polygon_mesh = static_cast<GT_PrimPolygonMesh *>(geo.get());
        GT_PrimPolygonMesh *polygon_mesh_copy = nullptr;

        bool can_reuse_mesh = true;

        GT_Owner owner;
        const auto &instance_shop =
            instance->findAttribute(GA_Names::shop_materialpath, owner, 0);

        if (instance_shop && instance_shop->getStorage() == GT_STORE_STRING)
        {
            can_reuse_mesh = false;

            // Recreate the Prim Poly mesh without materialpath attribute
            const auto &geo_points = polygon_mesh->getPointAttributes();
            const auto &geo_vertices = polygon_mesh->getVertexAttributes();
            const auto &geo_prims = polygon_mesh->getVertexAttributes();
            const auto &geo_details = polygon_mesh->getDetailAttributes();
            GT_AttributeListHandle new_points(nullptr);
            GT_AttributeListHandle new_vertices(nullptr);
            GT_AttributeListHandle new_prims(nullptr);
            GT_AttributeListHandle new_details(nullptr);

            if (geo_points)
            {
                geo_points->removeAttribute(GA_Names::shop_materialpath);
            }
            if (geo_vertices)
            {
                geo_vertices->removeAttribute(GA_Names::shop_materialpath);
            }
            if (geo_prims)
            {
                geo_prims->removeAttribute(GA_Names::shop_materialpath);
            }
            if (geo_details)
            {
                geo_details->removeAttribute(GA_Names::shop_materialpath);
            }

            polygon_mesh_copy =
                new GT_PrimPolygonMesh(*polygon_mesh, geo_points, geo_vertices,
                                       geo_prims, geo_details);

            GLTF_Mesh mesh;
            UT_Matrix4D identity(1);
            processPrimPolygon(polygon_mesh_copy, identity, mesh);

            for (exint in_idx = 0; in_idx < xforms->entries(); ++in_idx)
            {
                // Multiply primitve and instance transform
                GT_TransformHandle xform = xforms->get(in_idx);
                UT_ASSERT(xform->getSegments() == 1);
                UT_Matrix4D m;
                xform->getMatrix(m, 0);
                m = m * prim_xform;

                GLTF_Handle node_idx;
                GLTF_Node &node = myRoot.CreateNode(node_idx);
                myNode->children.append(node_idx);

                // Copy mesh via copy constructor
                GLTF_Mesh instanced_mesh(mesh);

                for (GLTF_Primitive &prim : instanced_mesh.primitives)
                {
                    GLTF_Handle material =
                        myCreateMaterial(instance_shop->getS(in_idx));
                    if (material != GLTF_INVALID_IDX)
                    {
                        prim.material = material;
                    }
                }

                GLTF_Handle mesh_idx = appendMeshIfNotEmpty(instanced_mesh);
                if (mesh_idx != GLTF_INVALID_IDX)
                {
                    node.mesh = mesh_idx;
                }

                node.matrix = m;
            }
        }

        // A straight instance with no attributes other than transforms.
        // We can simply reuse the GLTF_Mesh in this case
        if (can_reuse_mesh)
        {
            GLTF_Mesh mesh;
            UT_Matrix4D identity(1);
            processPrimPolygon(polygon_mesh, identity, mesh);
            GLTF_Handle mesh_idx = appendMeshIfNotEmpty(mesh);

            for (exint in_idx = 0; in_idx < xforms->entries(); ++in_idx)
            {
                // Multiply primitve and instance transform
                GT_TransformHandle xform = xforms->get(in_idx);
                UT_ASSERT(xform->getSegments() == 1);
                UT_Matrix4D m;
                xform->getMatrix(m, 0);
                m = m * prim_xform;

                GLTF_Handle node_idx;
                GLTF_Node &node = myRoot.CreateNode(node_idx);
                myNode->children.append(node_idx);

                // Copy mesh via copy constructor
                if (mesh_idx != GLTF_INVALID_IDX)
                {
                    node.mesh = mesh_idx;
                }

                node.matrix = m;
            }
        }
    }
    else if (type == GT_GEO_PACKED)
    {
        // Extract name from instance attribute
        GT_Owner name_owner;
        auto name_attr = instance->findAttribute("name", name_owner, 0);

        UT_StringHolder name = "";
        if (name_attr && name_attr->entries() > 0)
        {
            name = name_attr->getS(0);
        }

        // Setup transforms
        const GT_GEOPrimPacked *packed =
            static_cast<const GT_GEOPrimPacked *>(geo.get());

        // Create empty xforms with the same # of items

        UT_Matrix4D packed_xform;
        packed->getPrimitiveTransform()->getMatrix(packed_xform);

        // Zero out the PP transform
        const UT_Matrix4D identity(1);
        GT_PrimitiveHandle geo_copy = geo->doSoftCopy();
        geo_copy->setPrimitiveTransform(new GT_Transform(&identity, 1));

	GT_PrimInstance new_inst(geo_copy, instance->transforms(),
				    instance->packedPrimOffsets(),
				    instance->uniform(), instance->detail(),
				    instance->sourceGeometry());

	// Clone the refiner and change the root node - this is a bit
	// of a hack for recursively creating GLTF subnodes.
	ROP_GLTF_Refiner refiner_copy(*this);
	GLTF_Handle new_node_idx;
	GLTF_Node &new_node = myRoot.CreateNode(new_node_idx);

	new_node.matrix = packed_xform;
	new_node.name = name;
	myNode->children.append(new_node_idx);
	refiner_copy.myNode = &new_node;

	new_inst.refine(refiner_copy);
    }
    else
    {
        instance->refine(*this);
    }

    return true;
}

GLTF_Handle
ROP_GLTF_Refiner::appendMeshIfNotEmpty(GLTF_Mesh &mesh)
{
    if (mesh.primitives.size() > 0)
    {
        uint32 idx;
        GLTF_Mesh &new_mesh = myRoot.CreateMesh(idx);
        // copy via copy constructor
        new_mesh = mesh;
        return idx;
    }
    return GLTF_INVALID_IDX;
}

void
ROP_GLTF_Refiner::addMesh(const GT_PrimPolygonMesh &prim, UT_Matrix4D trans,
                          GLTF_Mesh &mesh)
{
    if (prim.getVertexList()->entries() == 0)
        return;

    if (prim.getPointAttributes()->entries() == 0)
        return;

    const GT_AttributeListHandle face_attributes =
        prim.getAttributeList(GT_OWNER_UNIFORM);

    // Split the mesh into submeshes by material assignments
    GT_DataArrayHandle material_attribute;
    if (face_attributes)
    {
        material_attribute = face_attributes->get(GA_Names::shop_materialpath);
    }

    GT_DataArrayHandle work_handle;

    const GT_Size num_faces = prim.getFaceCount();

    UT_StringArray strings;
    UT_IntArray indices;

    if (material_attribute)
    {
        material_attribute->getIndexedStrings(strings, indices);
    }

    UT_Array<UT_IntArray> material_faces_map(indices.size() + 1,
                                             indices.size() + 1);
    GT_Offset no_material_offset = indices.size();

    if (material_attribute)
    {
        // Sort the face indexes into arrays based on material assignment
        for (GT_Size idx = 0; idx < num_faces; idx++)
        {
            GT_Offset string_indice = material_attribute->getStringIndex(idx);

            if (string_indice == -1)
            {
                material_faces_map[no_material_offset].append(idx);
            }
            else
            {
                material_faces_map[string_indice].append(idx);
            }
        }
    }
    else
    {
        // Handle the case where there is no primitive materials - assign
        // every mesh to the 'nomesh' offset
        for (GT_Size idx = 0; idx < num_faces; idx++)
        {
            material_faces_map[no_material_offset].append(idx);
        }
    }

    GT_DataArrayHandle new_verts;
    GT_AttributeListHandle new_pt_attribs;
    ROP_GLTF_PointSplit::Split(prim, 0.f, new_pt_attribs, new_verts);
    const GT_Size num_vertices = new_verts->entries();
    const int32 *new_pts_arr = new_verts->getI32Array(work_handle);

    // A mapping from the vertex in the main mesh, to the
    // vertex in the submesh
    UT_IntArray vertex_to_submesh(num_vertices, num_vertices);
    std::fill_n(vertex_to_submesh.begin(), num_vertices, -1);
    for (int64 sm_idx = 0; sm_idx < material_faces_map.size(); sm_idx++)
    {
        const UT_IntArray &submesh_map = material_faces_map[sm_idx];

        // The final indice list for the submesh
        GT_Int32Array *submesh_indices = new GT_Int32Array(0, 1);
        GT_DataArrayHandle submesh_indices_handle(submesh_indices);

        // Indirect mapping from vertex-indices to submesh-indices
        GT_Int32Array *indirect_mapping = new GT_Int32Array(0, 1);
        GT_DataArrayHandle indirect_mapping_handle(indirect_mapping);

        // For each face in the submesh
        for (int64 idx = 0; idx < submesh_map.size(); idx++)
        {
            const int64 face = submesh_map[idx];

            // Get the location of the face in the main data array
            const GT_Offset first_vertex_offset = prim.getVertexOffset(face);
            UT_ASSERT(prim.getVertexCount(face) == 3);

            // For each vertex, if it's not in the submesh yet - then add it
            for (int64 i = 0; i < 3; i++)
            {
                const int64 vertex_offset =
                    new_pts_arr[first_vertex_offset + i];
                if (vertex_to_submesh[vertex_offset] == -1)
                {
                    indirect_mapping->append(vertex_offset);
                    vertex_to_submesh[vertex_offset] =
                        indirect_mapping->entries() - 1;
                }
                int64 vertex_in_submesh_idx = vertex_to_submesh[vertex_offset];
                submesh_indices->append(vertex_in_submesh_idx);
            }
        }

        // Only create the submesh if the group actually has points (GLTF
        // defines that empty meshes are not allowed)
        if (submesh_indices->entries() > 0 && submesh_map.entries() > 0)
        {
            UT_Matrix4D m(1);
            const GT_TransformHandle &x = prim.getPrimitiveTransform();
            if (x)
            {
                prim.getPrimitiveTransform()->getMatrix(m);
            }
            m = m * trans;

            if (new_pt_attribs && !m.isIdentity())
            {
                GT_TransformHandle tfh(new GT_Transform(new UT_Matrix4D(m), 1));
                new_pt_attribs = new_pt_attribs->transform(tfh);
            }

            GLTF_Primitive &prim = addPoints(
                new_pt_attribs->createIndirect(indirect_mapping_handle),
                submesh_indices_handle, mesh);

            GLTF_Handle material_handle = GLTF_INVALID_IDX;

            // Next is the primitive override material
            if (sm_idx != no_material_offset)
            {
                UT_StringHolder override_mat(strings[indices[sm_idx]]);
                material_handle = myCreateMaterial(override_mat);
            }
            // Finally, the object level material
            else if (myObjectMaterial != "")
            {
                material_handle = myCreateMaterial(myObjectMaterial);
            }

            if (material_handle != GLTF_INVALID_IDX)
            {
                prim.material = material_handle;
            }
        }

        std::fill_n(vertex_to_submesh.begin(), num_vertices, -1);
    }
}

template <typename T>
std::function<void(T *)>
getReverseWinding()
{
    return [](T *data) -> void { std::swap(data[1], data[2]); };
}

GLTF_Primitive &
ROP_GLTF_Refiner::addPoints(const GT_AttributeListHandle &point_attribs,
                            const GT_DataArrayHandle &indices, GLTF_Mesh &mesh)
{
    GLTF_Primitive gltf_primitive;

    // Translate  indices
    // Use the shortest possible indices type
    uint32 accessor;

    // TODO1:  use specializations of AddAttrib, get rid of target_type
    // because the conversions are done in GT_DataArrayHandle
    // TODO2:  create methods to get unsigned data from GT_DataArray
    // (notice the 1 << 7 and 1 << 15, we are losing a bit of space
    // in those special cases)
    if (indices->entries() < (1 << 7))
    {
        accessor = AddAttrib<uint8>(indices, GLTF_COMPONENT_UNSIGNED_BYTE, 1, 0,
                                    GLTF_BUFFER_ELEMENT,
                                    getReverseWinding<uint8>(), 3);
    }
    else if (indices->entries() < (1 << 15))
    {
        accessor = AddAttrib<uint16>(indices, GLTF_COMPONENT_UNSIGNED_SHORT, 1,
                                     0, GLTF_BUFFER_ELEMENT,
                                     getReverseWinding<uint16>(), 3);
    }
    else
    {
        accessor = AddAttrib<uint32>(indices, GLTF_COMPONENT_UNSIGNED_INT, 1, 0,
                                     GLTF_BUFFER_ELEMENT,
                                     getReverseWinding<uint32>(), 3);
    }

    gltf_primitive.indices = accessor;

    // Extract point attributes
    for (int idx = 0; idx < point_attribs->entries(); idx++)
    {
        const UT_StringRef &attrib_name = point_attribs->getName(idx);
        const GT_DataArrayHandle &attrib_data = point_attribs->get(idx, 0);

        ExportAttribute(attrib_name, attrib_data, gltf_primitive);
    }

    mesh.primitives.append(gltf_primitive);
    return mesh.primitives[mesh.primitives.size() - 1];
}

GLTF_ComponentType
ROP_GLTF_Refiner::GetComponentTypeFromStorage(GT_Storage storage)
{
    switch (storage)
    {
    case GT_Storage::GT_STORE_FPREAL16:
    case GT_Storage::GT_STORE_FPREAL32:
    case GT_Storage::GT_STORE_FPREAL64:
        return GLTF_COMPONENT_FLOAT;
    case GT_Storage::GT_STORE_INT8:
        return GLTF_COMPONENT_BYTE;
    case GT_Storage::GT_STORE_INT16:
        return GLTF_COMPONENT_SHORT;
    case GT_Storage::GT_STORE_INT32:
    case GT_Storage::GT_STORE_INT64:
        return GLTF_COMPONENT_UNSIGNED_INT;
    case GT_Storage::GT_STORE_UINT8:
        return GLTF_COMPONENT_UNSIGNED_BYTE;
    default:
        break;
    }
    return GLTF_COMPONENT_INVALID;
}

bool
ROP_GLTF_Refiner::ExportAttribute(const UT_StringRef &attrib_name,
                                  const GT_DataArrayHandle &attrib_data,
                                  GLTF_Primitive &prim)
{
    // Handle default item names - other names are exported as-is
    int uv_layer = theGetUVLayer(attrib_name.c_str());
    if (uv_layer != -1)
    {
        auto flip_uvs = [](fpreal32 *uv) { uv[1] = 1.f - uv[1]; };

        uint32 vertex_colors =
            AddAttrib<fpreal32>(attrib_data, GLTF_COMPONENT_FLOAT, 2, 0,
                                GLTF_BUFFER_ARRAY, flip_uvs);

        UT_String texcoord_str("TEXCOORD_");
        texcoord_str.append(std::to_string(uv_layer).c_str());

        prim.attributes.insert({texcoord_str.c_str(), vertex_colors});
    }
    else if (attrib_name == GA_Names::P)
    {
        uint32 position = AddAttrib(attrib_data, GLTF_COMPONENT_FLOAT, 3, 0,
                                    GLTF_BUFFER_ARRAY);

        prim.attributes.insert({"POSITION", position});
    }
    else if (attrib_name == GA_Names::N)
    {
        auto normalize_normals = [](fpreal32 *normal) {
            UT_Vector3F nv(normal[0], normal[1], normal[2]);
            nv.normalize();
            normal[0] = nv.x();
            normal[1] = nv.y();
            normal[2] = nv.z();
        };

        uint32 normals =
            AddAttrib<fpreal32>(attrib_data, GLTF_COMPONENT_FLOAT, 3, 0,
                                GLTF_BUFFER_ARRAY, normalize_normals);

        prim.attributes.insert({"NORMAL", normals});
    }
    else if (attrib_name == GA_Names::Cd)
    {
        uint32 vertex_colors = AddAttrib(attrib_data, GLTF_COMPONENT_FLOAT, 3,
                                         0, GLTF_BUFFER_ARRAY);

        prim.attributes.insert({"COLOR_0", vertex_colors});
    }
    else if (attrib_name == "tangentu")
    {
        auto assign_tangent_handedness = [](fpreal32 *tangent) {
            tangent[3] = 1.f;
        };

        uint32 vertex_colors =
            AddAttrib<fpreal32>(attrib_data, GLTF_COMPONENT_FLOAT, 4, 0,
                                GLTF_BUFFER_ARRAY, assign_tangent_handedness);

        prim.attributes.insert({"TANGENT", vertex_colors});
    }
    else if (attrib_name == "tangentv")
    {
        // Note:  tangentv is not exported because the bitangent is
        // automatically calculated by the GLTF application based
        // on the normal and the tangent
    }
    else if (myOptions.output_custom_attribs)
    {
        // Translate custom data
        GT_Storage storage = attrib_data->getStorage();
        GLTF_ComponentType component_type =
            GetComponentTypeFromStorage(storage);
        // Skip string attributes as the importer doesn't support them
        if (component_type != GLTF_COMPONENT_INVALID)
        {
            uint32 new_attrib = AddAttrib(
                    attrib_data, component_type, attrib_data->getTupleSize(), 0,
                    GLTF_BUFFER_ARRAY);

            // Per the GLTF spec, custom attribs are required to start with _
            UT_String new_name = "_";
            new_name += attrib_name;
            prim.attributes.insert({std::move(new_name), new_attrib});
        }
    }

    return true;
}

// Helper function for AddAttrib
template <typename T, typename FUNC_CAST>
ROP_GLTF_Refiner::Attrib_CopyResult
ROP_GLTF_Refiner::CopyAttribData(uint32 bid, const T *arr, GT_Size entries,
                                 GT_Size old_tuple_size, GT_Size new_tuple_size,
                                 std::function<void(FUNC_CAST *)> func,
                                 uint32 stride)
{
    Attrib_CopyResult result;
    uint32 offset;
    GT_Size new_buff_len;

    new_buff_len = entries;

    const GT_Size new_buff_size = new_buff_len * new_tuple_size * sizeof(T);

    T *new_buffer_data = static_cast<T *>(
        myRoot.BufferAlloc(bid, new_buff_size, sizeof(T), offset));

    for (GA_Size idx = 0; idx < new_buff_len; idx++)
    {
        for (GA_Size off = 0; off < new_tuple_size; off++)
        {
            new_buffer_data[new_tuple_size * idx + off] =
                arr[old_tuple_size * idx + off];
        }
    }

    // Find min/max for the accessor
    UT_Array<T> min;
    UT_Array<T> max;

    for (exint i = 0; i < new_tuple_size; i++)
    {
        min.append(std::numeric_limits<T>::max());
        max.append(std::numeric_limits<T>::lowest());
    }
    for (exint idx = 0; idx < new_buff_len; idx++)
    {
        if (func && (idx % stride == 0))
        {
            FUNC_CAST *data = reinterpret_cast<FUNC_CAST *>(
                &new_buffer_data[new_tuple_size * idx]);
            func(data);
        }

        for (exint off = 0; off < new_tuple_size; off++)
        {
            min[off] =
                std::min(new_buffer_data[new_tuple_size * idx + off], min[off]);

            max[off] =
                std::max(new_buffer_data[new_tuple_size * idx + off], max[off]);
        }
    }

    result.elem_min = UT_Array<fpreal64>(new_tuple_size);
    result.elem_max = UT_Array<fpreal64>(new_tuple_size);
    for (exint i = 0; i < new_tuple_size; i++)
    {
        result.elem_min.append(static_cast<fpreal64>(min[i]));
        result.elem_max.append(static_cast<fpreal64>(max[i]));
    }

    result.entries = new_buff_len;
    result.size = new_buff_size;
    result.offset = offset;
    return result;
}

template <typename T>
uint32
ROP_GLTF_Refiner::AddAttrib(const GT_DataArrayHandle &handle,
                            GLTF_ComponentType target_type,
                            GT_Size new_tuple_size, uint32 bid,
                            GLTF_BufferViewTarget buffer_type,
                            std::function<void(T *)> func, uint32 stride)
{
    Attrib_CopyResult attrib_data;
    const GT_Size old_tuple_size = handle->getTupleSize();
    const GLTF_Type type = GLTF_Util::getTypeForTupleSize(new_tuple_size);

    GT_DataArrayHandle buffer;
    if (target_type == GLTF_COMPONENT_BYTE ||
        target_type == GLTF_COMPONENT_UNSIGNED_BYTE)
    {
        attrib_data =
            CopyAttribData(bid, handle->getI8Array(buffer), handle->entries(),
                           old_tuple_size, new_tuple_size, func, stride);
    }
    else if (target_type == GLTF_COMPONENT_FLOAT)
    {
        attrib_data =
            CopyAttribData(bid, handle->getF32Array(buffer), handle->entries(),
                           old_tuple_size, new_tuple_size, func, stride);
    }
    else if (target_type == GLTF_COMPONENT_SHORT ||
             target_type == GLTF_COMPONENT_UNSIGNED_SHORT)
    {
        attrib_data =
            CopyAttribData(bid, handle->getI16Array(buffer), handle->entries(),
                           old_tuple_size, new_tuple_size, func, stride);
    }
    else if (target_type == GLTF_COMPONENT_UNSIGNED_INT)
    {
        attrib_data =
            CopyAttribData(bid, handle->getI32Array(buffer), handle->entries(),
                           old_tuple_size, new_tuple_size, func, stride);
    }

    uint32 accessor_idx;
    uint32 bufferview_idx;

    GLTF_BufferView &bufferview = myRoot.CreateBufferview(bufferview_idx);
    bufferview.buffer = bid;
    bufferview.byteLength = attrib_data.size;
    bufferview.target = buffer_type;
    bufferview.byteOffset = attrib_data.offset;

    GLTF_Accessor &accessor = myRoot.CreateAccessor(accessor_idx);
    accessor.bufferView = bufferview_idx;
    accessor.componentType = target_type;
    accessor.count = attrib_data.entries;
    accessor.type = type;
    accessor.min = attrib_data.elem_min;
    accessor.max = attrib_data.elem_max;

    return accessor_idx;
}

ROP_GLTF_PointSplit::ROP_GLTF_PointSplit(const GT_PrimPolygonMesh &prim,
                                         fpreal64 tol)
    : myPrim(prim), myTol(tol)
{
}

void
ROP_GLTF_PointSplit::Split(const GT_PrimPolygonMesh &polymesh, fpreal64 tol,
                           GT_AttributeListHandle &new_points,
                           GT_DataArrayHandle &new_vertices)
{
    ROP_GLTF_PointSplit splitter(polymesh, tol);

    GT_AttributeListHandle pt_attribs = polymesh.getPointAttributes();
    GT_AttributeListHandle vtx_attribs = splitter.refineDetailPrims();

    new_vertices = polymesh.getVertexList();
    new_points = GT_AttributeListHandle(new GT_AttributeList(*pt_attribs));
    for (exint attr_idx = 0; attr_idx < vtx_attribs->entries(); ++attr_idx)
    {
        splitter.splitAttrib(new_points, new_vertices, vtx_attribs, attr_idx);
    }

    // new_points->dumpList("POINTS", false);
}

GT_AttributeListHandle
ROP_GLTF_PointSplit::refineDetailPrims()
{
    GT_AttributeListHandle new_vx_attr_list(myPrim.getVertexAttributes());

    // If we don't have any vertex attributes
    if (!new_vx_attr_list)
    {
        new_vx_attr_list = GT_AttributeListHandle(
            new GT_AttributeList(new GT_AttributeMap(), 1));
    }

    // Promote prim attribute
    auto prim_attribs = myPrim.getUniformAttributes();
    if (prim_attribs)
    {
        auto new_attribs = prim_attribs->createIndirect(
            myPrim.getFaceCountArray().buildRepeatList());

        new_vx_attr_list = new_vx_attr_list->mergeNewAttributes(new_attribs);
    }

    // Promote detail attribute
    auto detail_attribs = myPrim.getDetailAttributes();
    if (detail_attribs)
    {
        const GT_Size num_verts = myPrim.getVertexList()->entries();
        GT_Int32Array *new_verts = new GT_Int32Array(num_verts, 1);
        GT_DataArrayHandle new_verts_handle(new_verts);
        for (exint i = 0; i < num_verts; i++)
            new_verts->set(0, i);

        auto new_attribs = detail_attribs->createIndirect(new_verts_handle);
        new_vx_attr_list = new_vx_attr_list->mergeNewAttributes(new_attribs);
    }
    return new_vx_attr_list;
}

template <typename T>
GT_DataArrayHandle
ROP_GLTF_PointSplit::splitAttribute(
    GT_Int32Array *new_verts,
    UT_Array<UT_Array<GT_Offset>> &vertexes_using_point,
    GT_Int32Array *new_pts_indirect, T *attr_arr, GT_Size tuple_size)
{
    // For each vertex, if the vertex does not match some other
    // vertex in vertexes_using_point, then split the vertex
    for (GT_Offset p_idx = 0; p_idx < new_pts_indirect->entries(); p_idx++)
    {
        if (vertexes_using_point[p_idx].size() < 2)
            continue;

        const GT_Offset v_idx = vertexes_using_point[p_idx][0];

        // The vertices thare are to be split of this point into a new point
        UT_Array<GT_Offset> v_to_split;

        for (exint idx = vertexes_using_point[p_idx].size() - 1; idx > 0; idx--)
        {
            const GT_Offset v = vertexes_using_point[p_idx][idx];

            if (v != v_idx && compareAttribs(v_idx, v, attr_arr, tuple_size))
            {
                v_to_split.append(v);

                // Update the vertexes->points array
                vertexes_using_point[p_idx].removeIndex(idx);
            }
        }

        if (v_to_split.size() == 0)
            continue;

        // Finally, perform the splitting
        UT_ASSERT(new_pts_indirect->entries() ==
                  vertexes_using_point.entries());

        // new_pts_indirect has at most 1 layer of indirection, so
        // dereferencing the added point will cause it to always point to
        // the original array
        new_pts_indirect->append(new_pts_indirect->getI32(p_idx));
        vertexes_using_point.append(UT_Array<exint>());

        uint32 new_p_idx = new_pts_indirect->entries() - 1;

        for (int32 vert : v_to_split)
        {
            new_verts->set(new_p_idx, vert);
            vertexes_using_point[new_p_idx].append(vert);
        }
    }

    UT_ASSERT(new_pts_indirect->entries() == vertexes_using_point.entries());

    // Promote the attribute to a point attribute.  We create an indirect
    // array from points to the vertex attributes (we use an arbitrary
    // vertex attrib value which is mapped to the point)
    GT_Int32Array *prmtd_pt_attrb =
        new GT_Int32Array(new_pts_indirect->entries(), 1);
    GT_DataArrayHandle prmtd_pt_attrb_handle(prmtd_pt_attrb);

    for (exint idx = 0; idx < new_pts_indirect->entries(); ++idx)
    {
        auto &&tmp = vertexes_using_point[idx];
        if (!tmp.isEmpty())
        {
            prmtd_pt_attrb->set(*tmp.begin(), idx);
        }
    }

    return prmtd_pt_attrb_handle;
}

template <typename T>
bool
ROP_GLTF_PointSplit::compareAttribs(GT_Offset pt_1, GT_Offset pt_2, T *attr_arr,
                                    GT_Size tuple_size)
{
    for (exint idx = 0; idx < tuple_size; idx++)
    {
        GT_Offset of1 = pt_1 * tuple_size + idx;
        GT_Offset of2 = pt_2 * tuple_size + idx;
        if (SYSabs(attr_arr[of1] - attr_arr[of2]) >= myTol)
        {
            return true;
        }
    }

    return false;
}

void
ROP_GLTF_PointSplit::splitAttrib(GT_AttributeListHandle &new_points,
                                 GT_DataArrayHandle &new_vertice,
                                 const GT_AttributeListHandle &vertex_attribs,
                                 exint attr_idx)
{
    GT_DataArrayHandle tmp;
    const int32 *vertex_list = new_vertice->getI32Array(tmp);

    // Copy the existing vertex array
    GT_Int32Array *new_verts = new GT_Int32Array(0, 1);
    GT_DataArrayHandle new_verts_handle(new_verts);

    // and fill it out naievely
    for (exint idx = 0; idx < new_vertice->entries(); idx++)
        new_verts->append(vertex_list[idx]);

    // An indirect mapping to the point array from the extended
    // point array to the initial point array.
    // Assumes that all point attributes have the same # of elems
    const int32 initial_points = new_points->get(0)->entries();
    GT_Int32Array *new_pts_indirect = new GT_Int32Array(initial_points, 1);
    const GT_DataArrayHandle new_pts_da(new_pts_indirect);

    for (GT_Offset idx = 0; idx < new_pts_indirect->entries(); ++idx)
    {
        new_pts_indirect->set(idx, idx);
    }

    // An array where the ith element contains a list of the
    // vertexes which use the ith point
    UT_Array<UT_Array<GT_Offset>> vertexes_using_point(initial_points,
                                                       initial_points);
    for (GT_Offset idx = 0; idx < new_vertice->entries(); ++idx)
    {
        const int32 point = vertex_list[idx];
        vertexes_using_point[point].emplace_back(idx);
    }

    const auto attr = vertex_attribs->get(attr_idx);
    const auto attr_name = vertex_attribs->getName(attr_idx);

    // We do not export non-numerical attributes
    if (attr->getStorage() < GT_STORE_UINT8 ||
        attr->getStorage() > GT_STORE_REAL64)
    {
        return;
    }

    // If the attribute is private then skip
    if (attr_name.startsWith("__"))
    {
        return;
    }

    GT_DataArrayHandle prmtd_pt_attrb;
    GT_DataArrayHandle buffer;
    if (attr->getStorage() < GT_STORE_FPREAL16)
    {
        prmtd_pt_attrb =
            splitAttribute(new_verts, vertexes_using_point, new_pts_indirect,
                           attr->getI32Array(buffer), attr->getTupleSize());
    }
    else
    {
        prmtd_pt_attrb =
            splitAttribute(new_verts, vertexes_using_point, new_pts_indirect,
                           attr->getF32Array(buffer), attr->getTupleSize());
    }

    auto new_attrib_data =
        GT_DataArrayHandle(new GT_DAIndirect(prmtd_pt_attrb, attr));

    new_points = new_points->createIndirect(new_pts_da);
    new_points = new_points->addAttribute(attr_name, new_attrib_data, false);
    new_vertice = new_verts;

#if DEBUG_GT_POINTSPLIT
    new_pts_indirect->dumpValues("Indirect Pt Map");
#endif

#if DEBUG_GT_POINTSPLIT
    new_points->dumpList("NEW POINTS ATTRIB");
#endif
}
