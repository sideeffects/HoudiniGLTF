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

#include "GLTF_Types.h"

#include <UT/UT_Quaternion.h>

using namespace GLTF_NAMESPACE;

GLTF_TRANSFORM_TYPE 
GLTF_Node::getTransformType() const
{
    if (matrix.isIdentity())
    {
        return GLTF_TRANSFORM_TRS;
    }
    return GLTF_TRANSFORM_MAT4;
}

void 
GLTF_Node::getTransformAsMatrix(UT_Matrix4F &mat) const
{
    UT_Matrix4F transform = matrix;
    if (transform.isIdentity())
    {
        UT_Matrix4F rotation_transform;
        UT_Quaternion(rotation).getTransformMatrix(rotation_transform);
        UT_Matrix4F trs_matrix(1);

        trs_matrix.scale(scale);
        trs_matrix = rotation_transform * trs_matrix;
        trs_matrix.translate(translation);

        transform = trs_matrix;
    }
    mat = transform;
}