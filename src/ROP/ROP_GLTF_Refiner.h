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

#ifndef __ROP_GLTF_REFINER_h__
#define __ROP_GLTF_REFINER_h__

#include <GA/GA_Types.h>
#include <GLTF/GLTF_Types.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_Refine.h>
#include <GT/GT_Types.h>

#include <functional>

typedef GLTF_NAMESPACE::GLTF_Primitive           GLTF_Primitive;
typedef GLTF_NAMESPACE::GLTF_Mesh                GLTF_Mesh;
typedef GLTF_NAMESPACE::GLTF_ComponentType       GLTF_ComponentType;
typedef GLTF_NAMESPACE::GLTF_BufferViewTarget    GLTF_BufferViewTarget;

class GT_PrimPolygonMesh;
class GT_PrimInstance;
class GU_Detail;
class ROP_GLTF_BaseErrorManager;
class GT_GEOPrimPacked;

class ROP_GLTF_Refiner : public GT_Refine
{
public:
    struct Refine_Options
    {
        bool output_custom_attribs = false;
    };

    ROP_GLTF_Refiner(ROP_GLTF_ExportRoot &root, GLTF_Node *node,
                     const UT_StringHolder &obj_material,
                     std::function<GLTF_Handle(UT_StringHolder)> create_material,
                     Refine_Options options);

    ~ROP_GLTF_Refiner() override;
    void addPrimitive(const GT_PrimitiveHandle &prim) override;

    // GLTF buffer allocation is currently unprotected
    bool allowThreading() const override { return false; }

    ///
    /// A convenience function.  Refines the detail, and adds meshes 
    /// (or potentially submeshes if instancing is used) to the GLTF_Node
    /// that is passed in.
    ///
    static void
    refine(const GU_Detail *src, ROP_GLTF_ExportRoot &root, GLTF_Node &node,
           const UT_StringHolder &obj_material,
           std::function<GLTF_Handle(UT_StringHolder)> create_material,
           Refine_Options options);


private:
    void processPrimPolygon(GT_PrimPolygonMesh *prim, UT_Matrix4D trans,
                            GLTF_Mesh &mesh);

    bool processInstance(const GT_PrimInstance *instance);

    GLTF_Handle appendMeshIfNotEmpty(GLTF_Mesh &mesh);

    void
    addMesh(const GT_PrimPolygonMesh &prim, UT_Matrix4D trans, GLTF_Mesh &mesh);

    // Creates a GLTF_Primitive based on the given attributes and
    // indices then returns a reference to it.
    GLTF_Primitive &
    addPoints(const GT_AttributeListHandle &attributes,
              const GT_DataArrayHandle &indices, GLTF_Mesh &mesh);

    // Translates a Houdini component type to the 'closest' GLTF
    // component type
    GLTF_ComponentType GetComponentTypeFromStorage(GT_Storage storage);

    struct Attrib_CopyResult
    {
        uint32 size;
        uint32 offset;
        UT_Array<fpreal64> elem_min;
        UT_Array<fpreal64> elem_max;
        uint32 entries;
    };

    template <typename T, typename FUNC_CAST>
    Attrib_CopyResult
    CopyAttribData(uint32 bid, const T *arr, GT_Size entries,
                   GT_Size old_tuple_size, GT_Size new_tuple_size,
                   std::function<void(FUNC_CAST *)> func, uint32 stride);

    //
    // Allocates data from the GLTF buffer 'bid' and moves attribute data
    // to handle, converting type if needed.
    // If old_tuple_size > new_tuple_size, then the size of the tuple will
    // be truncated (this is mainly used for UVs).
    //
    template <typename T = void>
    uint32 AddAttrib(const GT_DataArrayHandle &handle,
                     GLTF_ComponentType target_type, GT_Size new_tuple_size,
                     uint32 bid, GLTF_BufferViewTarget buffer_type,
                     std::function<void(T *)> func = {}, uint32 stride = 1);

    bool ExportAttribute(const UT_StringRef &attrib_name,
                         const GT_DataArrayHandle &attrib_data,
                         GLTF_Primitive &prim);

    ROP_GLTF_ExportRoot &myRoot;
    GLTF_Node *myNode;
    const UT_StringHolder &myObjectMaterial;
    std::function<GLTF_Handle(UT_StringHolder)> myCreateMaterial;
    const Refine_Options myOptions;
};

class ROP_GLTF_PointSplit
{
public:
    static void
    Split(const GT_PrimPolygonMesh &polymesh, fpreal64 tol,
          GT_AttributeListHandle &new_points, GT_DataArrayHandle &new_vertices);

private:
    ROP_GLTF_PointSplit(const GT_PrimPolygonMesh &prim, fpreal64 tol);

    // Refines detail and prim attributes down to vertex attributes
    // (which will later be refined into point attributes)
    GT_AttributeListHandle refineDetailPrims();

    // Returns true if the points can be merged
    template <typename T>
    bool compareAttribs(GT_Offset pt_1, GT_Offset pt_2, T *attr_arr,
                        GT_Size tuple_size);

    template <typename T>
    GT_DataArrayHandle
    splitAttribute(GT_Int32Array *new_verts,
                   UT_Array<UT_Array<GT_Offset>> &vertexes_using_point,
                   GT_Int32Array *new_pts_indirect, T *attr_arr,
                   GT_Size tuple_size);

    void
    splitAttrib(GT_AttributeListHandle &new_points, GT_DataArrayHandle &new_vertice,
          const GT_AttributeListHandle &vertex_attribs, exint idx);

    const GT_PrimPolygonMesh &myPrim;
    const fpreal myTol;
};

#endif
