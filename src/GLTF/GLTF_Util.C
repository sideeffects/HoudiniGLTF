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

#include "GLTF_Cache.h"
#include "GLTF_Loader.h"
#include "GLTF_Util.h"

#include <UT/UT_Matrix3.h>
#include <UT/UT_Quaternion.h>

using namespace GLTF_NAMESPACE;

const char GLTF_API *
GLTF_Util::typeGetName(GLTF_Type type)
{
    switch (type)
    {
    case GLTF_Type::GLTF_TYPE_SCALAR:
        return "SCALAR";
    case GLTF_Type::GLTF_TYPE_MAT2:
        return "MAT2";
    case GLTF_Type::GLTF_TYPE_MAT3:
        return "MAT3";
    case GLTF_Type::GLTF_TYPE_MAT4:
        return "MAT4";
    case GLTF_Type::GLTF_TYPE_VEC2:
        return "VEC2";
    case GLTF_Type::GLTF_TYPE_VEC3:
        return "VEC3";
    case GLTF_Type::GLTF_TYPE_VEC4:
        return "VEC4";
    case GLTF_Type::GLTF_TYPE_INVALID:
        return nullptr;
    }

    return "";
}

GLTF_Int
GLTF_Util::componentTypeGetBytes(GLTF_ComponentType type)
{
    switch (type)
    {
    case GLTF_ComponentType::GLTF_COMPONENT_BYTE:
    case GLTF_ComponentType::GLTF_COMPONENT_UNSIGNED_BYTE:
        return 1;
    case GLTF_ComponentType::GLTF_COMPONENT_UNSIGNED_SHORT:
    case GLTF_ComponentType::GLTF_COMPONENT_SHORT:
        return 2;
    case GLTF_ComponentType::GLTF_COMPONENT_UNSIGNED_INT:
    case GLTF_ComponentType::GLTF_COMPONENT_FLOAT:
        return 4;
    case GLTF_ComponentType::GLTF_COMPONENT_INVALID:
        return 0;
    default:
	return 0;
    }
}

GLTF_Int
GLTF_Util::typeGetElements(GLTF_Type type)
{
    switch (type)
    {
    case GLTF_Type::GLTF_TYPE_SCALAR:
        return 1;
    case GLTF_Type::GLTF_TYPE_VEC2:
        return 2;
    case GLTF_Type::GLTF_TYPE_VEC3:
        return 3;
    case GLTF_Type::GLTF_TYPE_VEC4:
    case GLTF_Type::GLTF_TYPE_MAT2:
        return 4;
    case GLTF_Type::GLTF_TYPE_MAT3:
        return 9;
    case GLTF_Type::GLTF_TYPE_MAT4:
        return 16;
    default: 
	return 0;
    }
}

GLTF_Int
GLTF_Util::getDefaultStride(GLTF_Type type,
                                 GLTF_ComponentType component_type)
{
    return componentTypeGetBytes(component_type) *
           typeGetElements(type);
}

GLTF_Int
GLTF_Util::getStride(uint32 previous_stride, GLTF_Type type,
                          GLTF_ComponentType component_type)
{
    if (previous_stride != 0)
    {
        return previous_stride;
    }
    return componentTypeGetBytes(component_type) *
           typeGetElements(type);
}

GLTF_Type
GLTF_Util::getTypeForTupleSize(uint32 tuplesize)
{
    switch (tuplesize)
    {
    case 1:
        return GLTF_Type::GLTF_TYPE_SCALAR;
    case 2:
        return GLTF_Type::GLTF_TYPE_VEC2;
    case 3:
        return GLTF_Type::GLTF_TYPE_VEC3;
    case 4:
        return GLTF_Type::GLTF_TYPE_VEC4;
    default:
        return GLTF_Type::GLTF_TYPE_INVALID;
    }
}

UT_Array<UT_String>
GLTF_Util::getSceneList(const UT_String &filename)
{
    UT_Array<UT_String> object_paths;

    auto loader = GLTF_Cache::GetInstance().LoadLoader(filename);
    if (!loader)
        return object_paths;

    for (const GLTF_Scene *scene : loader->getScenes())
    {
        object_paths.append(UT_String(UT_String::ALWAYS_DEEP, scene->name));
    }

    return object_paths;
}

bool
GLTF_Util::DecomposeMatrixToTRS(const UT_Matrix4F &mat,
                                UT_Vector3F &translation,
                                UT_Quaternion &rotation, UT_Vector3F scale)
{
    if (mat.determinant() != 0.0)
    {
        return false;
    }

    UT_Vector3D translationD;
    UT_Vector3D scaleD; 
    UT_Vector3D euler_rotation;
    UT_Vector3D shears;
    UT_XformOrder rotorder;

    mat.explode(UT_XformOrder::TRS, euler_rotation, scaleD, translationD,
                shears);

    const fpreal64 EPSILON = 0.000001;
    if (shears.length() > EPSILON)
    {
        return false;
    }

    translation = translationD;
    rotation.updateFromEuler(euler_rotation, rotorder);
    scale = scaleD;

    return true;
}
