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

#ifndef __SOP_GLTFUTIL_H__
#define __SOP_GLTFUTIL_H__

#include "GLTF_API.h"
#include "GLTF_Types.h"

namespace GLTF_NAMESPACE
{

class GLTF_API GLTF_Util
{
public:
    template <typename T>
    static T
    readInterleavedElement(unsigned char *data, uint32 stride, uint32 index)
    {
        return *reinterpret_cast<T *>(data + stride * index);
    }

    static const char *typeGetName(GLTF_Type type);
    static GLTF_Int componentTypeGetBytes(GLTF_ComponentType type);
    static GLTF_Int typeGetElements(GLTF_Type type);
    static GLTF_Int
    getDefaultStride(GLTF_Type type, GLTF_ComponentType component_type);

    ///
    /// Returns 0 if previous_stride == 0 and GetDefaultStride otherwise.
    ///
    static GLTF_Int getStride(uint32 previous_stride, GLTF_Type type,
                                       GLTF_ComponentType component_type);

    static GLTF_Type getTypeForTupleSize(uint32 tuplesize);

    ///
    /// Returns a list of the scene names in the given filename,
    /// where the index in the returned array corrosponds to the
    /// scene index, and the value corrosponds to the name if one
    /// exists, and "" othewise.
    ///
    static UT_Array<UT_String> getSceneList(const UT_String &filename);

    static bool
    DecomposeMatrixToTRS(const UT_Matrix4F &mat, UT_Vector3F &translation,
                         UT_Quaternion &rotation, UT_Vector3F scale);
};

} // end GLTF_NAMESPACE

#endif
