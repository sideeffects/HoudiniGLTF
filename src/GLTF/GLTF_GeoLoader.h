/*
 * Copyright (c) 2021
 *	Side Effects Software Inc.  All rights reserved.
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

#ifndef __SOP_GLTFGEOLOADER_H__
#define __SOP_GLTFGEOLOADER_H__

#include "GLTF_API.h"

#include <GLTF/GLTF_Types.h>

#include <UT/UT_Array.h>
#include <UT/UT_Quaternion.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_Vector3.h>

// Forward declarations
class GU_Detail;
class UT_String;

namespace GLTF_NAMESPACE
{

class GLTF_Loader;

struct GLTF_API GLTF_MeshLoadingOptions
{
    bool loadCustomAttribs = true;
    bool promotePointAttribs = true;
    bool consolidatePoints = true;
    fpreal pointConsolidationDistance = 0.0001F;
    bool addPathAttribute = false;
    UT_StringHolder pathAttributeName;
    UT_StringHolder pathAttributeValue;
};

class GLTF_API GLTF_GeoLoader
{
public:
    GLTF_GeoLoader(const GLTF_Loader &loader, GLTF_Handle mesh_idx,
                   GLTF_Handle primitive_idx,
                   const GLTF_MeshLoadingOptions& options = {});

    bool loadIntoDetail(GU_Detail &detail);

    static bool load(const GLTF_Loader &loader, GLTF_Handle mesh_idx,
                     GLTF_Handle primitive_idx, GU_Detail &detail,
                     const GLTF_MeshLoadingOptions options = {});

private:
    bool
    LoadVerticesAndPoints(GU_Detail &detail,
                          const GLTF_MeshLoadingOptions &options,
                          const GLTF_Accessor &pos, const GLTF_Accessor &ind);

    bool
    LoadVerticesAndPointsNonIndexed(GU_Detail &detail, const GLTF_Accessor &pos);

    bool
    AddPointAttribute(GU_Detail &detail, const UT_StringHolder &attrib_name,
                      const GLTF_Accessor &accessor);

    // const uint32 myRootNode;
    const GLTF_Handle myMeshIdx;
    const GLTF_Handle myPrimIdx;
    const GLTF_Loader &myLoader;
    const GLTF_MeshLoadingOptions myOptions;
};

} // end GLTF_NAMESPACE

#endif
