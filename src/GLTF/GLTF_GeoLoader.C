/*
 * Copyright (c) COPYRIGHTYEAR
 *       Side Effects Software Inc.  All rights reserved.
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
 *
 *----------------------------------------------------------------------------
 */
#include "GLTF_GeoLoader.h"

#include "GLTF_Loader.h"
#include "GLTF_Types.h"
#include "GLTF_Util.h"

#include <GA/GA_Handle.h>
#include <GU/GU_Detail.h>
#include <GU/GU_Promote.h>
#include <UT/UT_String.h>

#include <OP/OP_Network.h>

using namespace GLTF_NAMESPACE;

//======================================================================

UT_StringHolder
GLTF_MapAttribName(const UT_String &name)
{
    // Convert UV's
    if (name == "TEXCOORD_0")
        return "uv";
    if (name.startsWith("TEXCOORD_"))
    {
        UT_String name_copy = name;
        name_copy.eraseHead(9);
        if (name_copy.isInteger())
        {
            int uv_idx = name_copy.toInt();
            UT_String gltf_uvname("uv");
            gltf_uvname.append(std::to_string(uv_idx + 1).c_str());
            return UT_StringHolder(gltf_uvname.c_str());
        }
        // Falls through here
    }
    if (name == "NORMAL")
        return "N";
    if (name == "TANGENT")
        return "tangentu";
    if (name == "COLOR_0")
        return "Cd";

    return name;
}

bool
GLTF_IsAttributeCustom(UT_String name)
{
    if (name.startsWith("_"))
        return true;
    return false;
}

//
// Fills the attrib with the given GLTF buffer, applying operation
// to convert types or perform other operations.
//
template <typename T, typename O>
static bool
FillAttrib(GA_Detail &detail, GA_AttributeOwner owner, const UT_StringRef &name,
           unsigned char *attrib_data, uint32 attrib_stride,
           std::function<O(T)> operation)
{
    GA_RWHandleT<O> accessor_handle(&detail, owner, name);
    if (!accessor_handle.isValid())
        return false;

    uint32 i = 0;
    for (GA_Iterator it(detail.getPointRange()); !it.atEnd(); ++it, ++i)
    {
        T elem =
            GLTF_Util::readInterleavedElement<T>(attrib_data, attrib_stride, i);

        GA_Offset offset = *it;
        accessor_handle.set(offset, operation(elem));
    }

    return true;
}

template <typename T>
static bool
FillAttrib(GA_Detail &detail, GA_AttributeOwner owner, const UT_StringRef &name,
           unsigned char *attrib_data, uint32 attrib_stride)
{
    GA_RWHandleT<T> accessor_handle(&detail, owner, name);
    if (!accessor_handle.isValid())
        return false;

    uint32 i = 0;
    for (GA_Iterator it(detail.getPointRange()); !it.atEnd(); ++it, ++i)
    {
        T elem =
            GLTF_Util::readInterleavedElement<T>(attrib_data, attrib_stride, i);

        GA_Offset offset = *it;
        accessor_handle.set(offset, elem);
    }

    return true;
}

//======================================================================

bool
GLTF_GeoLoader::load(const GLTF_Loader &loader, GLTF_Handle mesh_idx,
                     GLTF_Handle primitive_idx, GU_Detail &detail,
                     const GLTF_MeshLoadingOptions options)
{
    GLTF_GeoLoader geoload(loader, mesh_idx, primitive_idx, options);
    return geoload.loadIntoDetail(detail);
}


bool
GLTF_GeoLoader::loadIntoDetail(GU_Detail &detail)
{
    const GLTF_Mesh *mesh = myLoader.getMesh(myMeshIdx);
    
    if (!mesh)
        return false;

    if (mesh->primitives.size() <= myPrimIdx)
        return false;

    const GLTF_Primitive &primitive = mesh->primitives[myPrimIdx];

    if (primitive.mode != GLTF_RENDERMODE_TRIANGLES)
        return false;

    // Load points & vertices attributes
    auto position_attribute = primitive.attributes.find("POSITION");
    if (position_attribute == primitive.attributes.end())
        return false;

    const GLTF_Accessor *position = myLoader.getAccessor(position_attribute->second);
    if (position == nullptr)
        return false;

    const GLTF_Accessor *indices = myLoader.getAccessor(primitive.indices);
    if (indices != nullptr)
    {
        if (!LoadVerticesAndPoints(detail, myOptions, *position, *indices))
            return false;
    }
    else if (!LoadVerticesAndPointsNonIndexed(detail, *position))
        return false;

    detail.bumpDataIdsForAddOrRemove(true, true, true);

    // Now for any other attribute, load it as a point attribute
    for (const auto &attrib : primitive.attributes)
    {
        UT_String attrib_name(attrib.first.c_str());

        // Position is treated specially (see LoadVerticesAndPoints)
        if (attrib.first == "POSITION")
            continue;

        bool custom_attrib = GLTF_IsAttributeCustom(attrib_name);
        if (myOptions.loadCustomAttribs || !custom_attrib)
        {
            // Erase the _
            if (custom_attrib)
                attrib_name.eraseHead(1);

            const GLTF_Accessor &attrib_acc =
                *myLoader.getAccessor(attrib.second);

            if (!AddPointAttribute(detail, attrib_name, attrib_acc))
                return false;
        }
    }

    // Handle the case when the exporter decides to use a single bufferview
    // for multiple submeshes (I've only seen this on the Unity exporter).
    // This could be handled more efficiently by only loading the points
    // referenced by the accessor.
    detail.destroyUnusedPoints();

    if (myOptions.promotePointAttribs)
    {
        // Promote all point attributes to vert attributes
        //
        // Note that we can't do this easily while iterating over
        // GA_AttributeDict, since GU_Promote will remove items from its
        // UT_ArrayStringMap while we are iterating over it through a proxy
        // iterator ... Using a temporary array is the simplest.
        UT_Array<GA_Attribute *> to_promote;

        for (auto attribute : detail.getAttributeDict(GA_ATTRIB_POINT))
        {
            // Skip Position attribute
            if (attribute->getName() == "P")
                continue;

            if (attribute->getScope() != GA_AttributeScope::GA_SCOPE_PUBLIC)
                continue;

            to_promote.append(attribute);
        }

        for (auto attribute : to_promote)
            GU_Promote::promote(detail, attribute, GA_ATTRIB_VERTEX);

        // Consolidate points
        detail.onlyConsolidatePoints(myOptions.pointConsolidationDistance, 
            nullptr, 0, true, GU_Detail::ONLYCONS_GRP_PROP_LEAST, true);
    }

    return true;
}

bool
GLTF_GeoLoader::AddPointAttribute(GU_Detail &detail,
                                  const UT_StringHolder& attrib_name,
                                  const GLTF_Accessor &accessor)
{
    unsigned char *attrib_data;
    GA_RWHandleT<UT_Vector3F> accessor_handle;

    if (!myLoader.LoadAccessorData(accessor, attrib_data))
        return false;

    const GLTF_BufferView &bufferview = *myLoader.getBufferView(accessor.bufferView);

    uint32 attrib_stride = GLTF_Util::getStride(
        bufferview.byteStride, accessor.type, accessor.componentType);

    const UT_StringHolder houdini_attrib_name =
        GLTF_MapAttribName(attrib_name.c_str());

    const uint32 num_elements = GLTF_Util::typeGetElements(accessor.type);

    // TODO:  We are typecasting uint32 to int32
    if (accessor.componentType == GLTF_COMPONENT_FLOAT)
    {
        if (num_elements == 1)
        {
            detail.addFloatTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC,
                                 houdini_attrib_name, 1);

            FillAttrib<fpreal32>(detail, GA_ATTRIB_POINT, houdini_attrib_name,
                                 attrib_data, attrib_stride);
        }
        else if (num_elements == 2)
        {
            if (houdini_attrib_name == "uv" || houdini_attrib_name == "uv2")
            {
                const auto flip_uvs = [](UT_Vector2F tex_coord) {
                    return UT_Vector3F(tex_coord.x(), 1.f - tex_coord.y(), 0);
                };

                detail.addFloatTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC,
                                     houdini_attrib_name, 3);

                FillAttrib<UT_Vector2F, UT_Vector3F>(
                    detail, GA_ATTRIB_POINT, houdini_attrib_name, attrib_data,
                    attrib_stride, flip_uvs);
            }
            else
            {
                detail.addFloatTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC,
                                     houdini_attrib_name, 2);

                FillAttrib<UT_Vector2F>(detail, GA_ATTRIB_POINT,
                                        houdini_attrib_name, attrib_data,
                                        attrib_stride);
            }
        }
        else if (num_elements == 3)
        {
	    if (houdini_attrib_name == "N")
	    {
		detail.addNormalAttribute(GA_ATTRIB_POINT, GA_STORE_REAL32);
	    }
	    else
	    {
		detail.addFloatTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC,
                                 houdini_attrib_name, 3);
	    }
            
            FillAttrib<UT_Vector3F>(detail, GA_ATTRIB_POINT,
                                    houdini_attrib_name, attrib_data,
                                    attrib_stride);
        }
        else if (num_elements == 4)
        {
            detail.addFloatTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC,
                                 houdini_attrib_name, 4);
            FillAttrib<UT_Vector4F>(detail, GA_ATTRIB_POINT,
                                    houdini_attrib_name, attrib_data,
                                    attrib_stride);
        }
        else
        {
            UT_ASSERT(false);
        }
    }
    else if (accessor.componentType == GLTF_COMPONENT_UNSIGNED_BYTE ||
             accessor.componentType == GLTF_COMPONENT_UNSIGNED_SHORT ||
             accessor.componentType == GLTF_COMPONENT_UNSIGNED_INT)
    {
        detail.addIntTuple(GA_ATTRIB_POINT, GA_SCOPE_PUBLIC,
                           houdini_attrib_name, num_elements);

        if (num_elements == 1)
        {
            FillAttrib<int32>(detail, GA_ATTRIB_POINT, houdini_attrib_name,
                              attrib_data, attrib_stride);
        }
        else if (num_elements == 2)
        {
            FillAttrib<UT_Vector2I>(detail, GA_ATTRIB_POINT,
                                    houdini_attrib_name, attrib_data,
                                    attrib_stride);
        }
        else if (num_elements == 3)
        {
            FillAttrib<UT_Vector3I>(detail, GA_ATTRIB_POINT,
                                    houdini_attrib_name, attrib_data,
                                    attrib_stride);
        }
        else if (num_elements == 4)
        {
            FillAttrib<UT_Vector4I>(detail, GA_ATTRIB_POINT,
                                    houdini_attrib_name, attrib_data,
                                    attrib_stride);
        }
        else
        {
            UT_ASSERT(false);
        }
    }

    return true;
}

bool
GLTF_GeoLoader::LoadVerticesAndPoints(GU_Detail &detail,
                                      const GLTF_MeshLoadingOptions &options,
                                      const GLTF_Accessor &pos,
                                      const GLTF_Accessor &ind)
{

    // We convert everything to indexed triangle meshes, so the number
    // of indices must divisible by 3
    if (ind.count % 3 != 0)
        return false;

    GLTF_BufferView pos_bv = *myLoader.getBufferView(pos.bufferView);

    // Load the vertex data from the binary into a buffer
    unsigned char *position_data;
    if (!myLoader.LoadAccessorData(pos, position_data))
        return false;

    uint32 pos_stride = GLTF_Util::getStride(pos_bv.byteStride, pos.type,
                                                  pos.componentType);

    // Read in the vertices from the (potentially) interleaved array
    GA_Offset start_pt_off = detail.appendPointBlock(pos.count);
    for (uint32 i = 0; i < pos.count; i++)
    {
        UT_Vector3F vec = GLTF_Util::readInterleavedElement<UT_Vector3F>(
            position_data, pos_stride, i);
        detail.setPos3(start_pt_off + i, vec);
    }

    // Wire up indices
    unsigned char *indice_data;
    const GLTF_BufferView &indBV = *myLoader.getBufferView(ind.bufferView);
    if (!myLoader.LoadAccessorData(ind, indice_data))
        return false;

    uint32 ind_stride = GLTF_Util::getStride(indBV.byteStride, ind.type,
                                                  ind.componentType);
    const uint32 num_tris = ind.count / 3;

    GA_Offset start_vtxoff;
    detail.appendPrimitivesAndVertices(GA_PRIMPOLY, ind.count / 3, 3,
                                       start_vtxoff, true);

    uint32 tri_vert_indexes[3];
    for (uint32 tri_idx = 0; tri_idx < num_tris; tri_idx++)
    {
        for (uint32 tri_vert_num = 0; tri_vert_num < 3; tri_vert_num++)
        {
            const uint32 indice = tri_idx * 3 + tri_vert_num;

            uint32 elem = 0;
            if (ind.componentType ==
                GLTF_ComponentType::GLTF_COMPONENT_UNSIGNED_BYTE)
            {
                elem = GLTF_Util::readInterleavedElement<unsigned char>(
                    indice_data, ind_stride, indice);
            }
            else if (ind.componentType ==
                     GLTF_ComponentType::GLTF_COMPONENT_UNSIGNED_SHORT)
            {
                elem = GLTF_Util::readInterleavedElement<unsigned short>(
                    indice_data, ind_stride, indice);
            }
            else if (ind.componentType ==
                     GLTF_ComponentType::GLTF_COMPONENT_UNSIGNED_INT)
            {
                elem = GLTF_Util::readInterleavedElement<uint32>(
                    indice_data, ind_stride, indice);
            }
            else
            {
                return false;
            }

            tri_vert_indexes[tri_vert_num] = start_vtxoff + elem;
        }

        const uint32 cur_tri_off = start_pt_off + tri_idx * 3;

        detail.getTopology().wireVertexPoint(
            static_cast<GA_Offset>(cur_tri_off + 0),
            static_cast<GA_Offset>(tri_vert_indexes[0]));
        // Swap second and third vertex indexes to reverse tri winding order
        detail.getTopology().wireVertexPoint(
            static_cast<GA_Offset>(cur_tri_off + 2),
            static_cast<GA_Offset>(tri_vert_indexes[1]));
        detail.getTopology().wireVertexPoint(
            static_cast<GA_Offset>(cur_tri_off + 1),
            static_cast<GA_Offset>(tri_vert_indexes[2]));
    }

    return true;
}

bool
GLTF_GeoLoader::LoadVerticesAndPointsNonIndexed(GU_Detail &detail, const GLTF_Accessor &pos)
{
    // We convert everything to triangle meshes, so the number
    // of vertices must divisible by 3
    if (pos.count % 3 != 0)
        return false;

    const uint32 num_tris = pos.count / 3;

    GLTF_BufferView pos_bv = *myLoader.getBufferView(pos.bufferView);

    // Load the vertex data from the binary into a buffer
    unsigned char *position_data;
    if (!myLoader.LoadAccessorData(pos, position_data))
        return false;

    uint32 pos_stride = GLTF_Util::getStride(pos_bv.byteStride, pos.type,
                                                  pos.componentType);

    // Read in the vertices from the (potentially) interleaved array
    GA_Offset start_pt_off = detail.appendPointBlock(pos.count);
    for (uint32 i = 0; i < pos.count; i++)
    {
        UT_Vector3F vec = GLTF_Util::readInterleavedElement<UT_Vector3F>(
            position_data, pos_stride, i);
        detail.setPos3(start_pt_off + i, vec);
    }    

    GA_Offset start_vtxoff;
    detail.appendPrimitivesAndVertices(GA_PRIMPOLY, num_tris, 3,
                                       start_vtxoff, true);

    for (uint32 tri_idx = 0; tri_idx < num_tris; tri_idx++)
    {
        const uint32 cur_tri_off = start_pt_off + (tri_idx * 3);
	const uint32 vtx_tri_off = start_vtxoff + (tri_idx * 3);

        detail.getTopology().wireVertexPoint(
            static_cast<GA_Offset>(cur_tri_off + 0),
            static_cast<GA_Offset>(vtx_tri_off + 0));
        // Swap second and third vertex indexes to reverse tri winding order
        detail.getTopology().wireVertexPoint(
            static_cast<GA_Offset>(cur_tri_off + 2),
            static_cast<GA_Offset>(vtx_tri_off + 1));
        detail.getTopology().wireVertexPoint(
            static_cast<GA_Offset>(cur_tri_off + 1),
            static_cast<GA_Offset>(vtx_tri_off + 2));
    }

    return true;
}

GLTF_GeoLoader::GLTF_GeoLoader(const GLTF_Loader &loader, GLTF_Handle mesh_idx,
                               GLTF_Handle primitive_idx,
                               const GLTF_MeshLoadingOptions& options)
    : myMeshIdx(mesh_idx),
      myPrimIdx(primitive_idx),
      myLoader(loader),
      myOptions(options)
{
}
