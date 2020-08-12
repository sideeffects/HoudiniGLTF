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
#ifndef __SOP_GLTFTYPES_H__
#define __SOP_GLTFTYPES_H__

#include "GLTF_API.h"

#include <UT/UT_Matrix4.h>
#include <UT/UT_Optional.h>
#include <UT/UT_String.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_StringMap.h>
#include <UT/UT_Array.h>
#include <UT/UT_Vector3.h>
#include <UT/UT_Vector4.h>

namespace GLTF_NAMESPACE
{

#define GLTF_INVALID_IDX uint32(~0)

typedef uint32 GLTF_Int;
typedef uint32 GLTF_Offset;
typedef uint32 GLTF_Handle;

const uint32 GLB_BUFFER_IDX = 0;
const uint32 GLTF_GLB_MAGIC = 0x46546C67;
const uint32 GLTF_GLB_JSON = 0x4E4F534A;
const uint32 GLTF_GLB_BIN = 0x004E4942;

constexpr const char *GLTF_PROJECTION_NAME_ORTHOGRAPHIC = "ORTHOGRAPHIC";
constexpr const char *GLTF_PROJECTION_NAME_PERSPECTIVE = "PERSPECTIVE";
constexpr const char *GLTF_TYPE_NAME_SCALAR = "SCALAR";
constexpr const char *GLTF_TYPE_NAME_VEC2 = "VEC2";
constexpr const char *GLTF_TYPE_NAME_VEC3 = "VEC3";
constexpr const char *GLTF_TYPE_NAME_VEC4 = "VEC4";
constexpr const char *GLTF_TYPE_NAME_MAT2 = "MAT2";
constexpr const char *GLTF_TYPE_NAME_MAT3 = "MAT3";
constexpr const char *GLTF_TYPE_NAME_MAT4 = "MAT4";

enum GLTF_RenderMode
{
    GLTF_RENDERMODE_POINTS = 0,
    GLTF_RENDERMODE_LINES = 1,
    GLTF_RENDERMODE_LINE_LOOP = 2,
    GLTF_RENDERMODE_LINE_STRIP = 3,
    GLTF_RENDERMODE_TRIANGLES = 4,
    GLTF_RENDERMODE_TRIANGLE_STRIP = 5,
    GLTF_RENDERMODE_TRIANGLE_FAN = 6,
    GLTF_RENDERMODE_INVALID
};

enum GLTF_ComponentType
{
    GLTF_COMPONENT_INVALID = 0,
    GLTF_COMPONENT_BYTE = 5120,
    GLTF_COMPONENT_UNSIGNED_BYTE = 5121,
    GLTF_COMPONENT_SHORT = 5122,
    GLTF_COMPONENT_UNSIGNED_SHORT = 5123,
    GLTF_COMPONENT_UNSIGNED_INT = 5125,
    GLTF_COMPONENT_FLOAT = 5126
};

enum GLTF_BufferViewTarget
{
    GLTF_BUFFER_INVALID = 0,
    GLTF_BUFFER_ARRAY = 34962,
    GLTF_BUFFER_ELEMENT = 34963
};

enum GLTF_TexFilter
{
    GLTF_TEXFILTER_INVALID = 0,
    GLTF_TEXFILTER_NEAREST = 9728,
    GLTF_TEXFILTER_LINEAR = 9729,
    GLTF_TEXFILTER_NEAREST_MIPMAP_NEAREST = 9984,
    GLTF_TEXFILTER_LINEAR_MIPMAP_NEAREST = 9985,
    GLTF_TEXFILTER_NEAREST_MIPMAP_LINEAR = 9986,
    GLTF_TEXFILTER_LINEAR_MIPMAP_LINEAR = 9987,
};

enum GLTF_TexWrap
{
    GLTF_TEXWRAP_INVALID = 0,
    GLTF_TEXWRAP_CLAMP_TO_EDGE = 33071,
    GLTF_TEXWRAP_MIRRORED_REPEAT = 33648,
    GLTF_TEXWRAP_REPEAT = 10497,
};

//=================================================

// Represented as a string in the GLTF file
enum GLTF_Type
{
    GLTF_TYPE_INVALID = 0,
    GLTF_TYPE_SCALAR,
    GLTF_TYPE_VEC2,
    GLTF_TYPE_VEC3,
    GLTF_TYPE_VEC4,
    GLTF_TYPE_MAT2,
    GLTF_TYPE_MAT3,
    GLTF_TYPE_MAT4
};

enum GLTF_TRANSFORM_TYPE
{
    GLTF_TRANSFORM_NONE = 0,
    GLTF_TRANSFORM_MAT4,
    GLTF_TRANSFORM_TRS
};

enum GLTF_TextureTypes
{
    TEXTURE_NONE = 0,
    TEXTURE_NORMAL,
    TEXTURE_OCCLUSION,
    TEXTURE_EMISSIVE
};

//=========================================================================

// GLTF types - unimplemented fields are currently displayed as comments in the
// structure declarations

struct GLTF_API GLTF_TextureInfo
{
    GLTF_Handle index = GLTF_INVALID_IDX;
    GLTF_Int texCoord = 0;
    // extensions
    // extras
};

struct GLTF_API GLTF_NormalTextureInfo : public GLTF_TextureInfo
{
    fpreal32 scale = 1.0;
    // extensions
    // extras
};

struct GLTF_API GLTF_Accessor
{
    GLTF_Handle bufferView = GLTF_INVALID_IDX;
    GLTF_Int byteOffset = 0;
    GLTF_ComponentType componentType = GLTF_COMPONENT_INVALID; // Required
    bool normalized = false;
    GLTF_Int count = 0;
    GLTF_Type type = GLTF_TYPE_INVALID;

    // A double has a 53 bit mantissa, we can therefore cast to either float
    // or int32 without loss of precision
    UT_Array<fpreal64> max;
    UT_Array<fpreal64> min;

    // sparse
    UT_String name = "";
    // extensions
    // extras
};

struct GLTF_API GLTF_Animation
{
    // channels
    UT_Array<GLTF_Accessor *> samplers;
    UT_String name = "";
    // extensions
    // extras
};

struct GLTF_API GLTF_Asset
{
    UT_String copyright = "";
    UT_String generator = "";
    UT_String version;
    UT_String minversion = "";
    // extensions
    // extras
};

struct GLTF_API GLTF_Buffer
{
    UT_String myURI = "";
    GLTF_Int myByteLength;
    UT_String name = "";
    // extensions
    // extras
};

struct GLTF_API GLTF_BufferView
{
    GLTF_Handle buffer = 0; // Required
    GLTF_Int byteOffset = 0;
    GLTF_Int byteLength = 0; // Required
    GLTF_Int byteStride = 0;
    GLTF_BufferViewTarget target = GLTF_BUFFER_INVALID;
    UT_String name = "";
    // extensions
    // extras
};

struct GLTF_API GLTF_Orthographic
{
    fpreal32 xmag;
    fpreal32 ymag;
    fpreal32 zmag;
    fpreal32 znear;
    // extensions
    //
};

struct GLTF_API GLTF_Perspsective
{
    UT_Optional<fpreal32> aspectRatio;
    fpreal32 yfov;
    UT_Optional<fpreal32> zmag;
    fpreal32 znear;
    // extensions
    // extras
};

struct GLTF_API GLTF_Camera
{
    UT_Optional<GLTF_Orthographic> orthographic;
    UT_Optional<GLTF_Perspsective> perspective;
    UT_String type = "";
    UT_String name = "";
    // extensions
    // extras
};

struct GLTF_API GLTF_Channel
{
    GLTF_Handle sampler;
    GLTF_Handle target;
    // extensions
    // extras
};

struct GLTF_API GLTF_Images
{
    UT_String uri;
    UT_String mimeType;
    UT_String name;
    // extensions
    // extras
};

struct GLTF_API GLTF_Indices
{
    GLTF_Handle bufferView;
    GLTF_Int byteOffset;
    GLTF_ComponentType componentType;
    // extensions
    // extras
};

struct GLTF_API GLTF_PBRMetallicRoughness
{
    UT_Vector4 baseColorFactor = {0.0f, 0.0f, 0.0f, 1.0f};
    UT_Optional<GLTF_TextureInfo> baseColorTexture;
    fpreal32 metallicFactor = 1.0f;
    fpreal32 roughnessFactor = 1.0f;
    UT_Optional<GLTF_TextureInfo> metallicRoughnessTexture;
};

struct GLTF_API GLTF_Material
{
    UT_String name = "";
    // extensions
    // extras
    UT_Optional<GLTF_PBRMetallicRoughness> metallicRoughness;
    UT_Optional<GLTF_NormalTextureInfo> normalTexture;
    UT_Optional<GLTF_TextureInfo> occlusionTexture;
    UT_Optional<GLTF_TextureInfo> emissiveTexture;
    UT_Vector3F emissiveFactor = {0.0f, 0.0f, 0.0f};
    UT_String alphaMode;
    fpreal32 alphaCutoff;
    bool doubleSided;
};

struct GLTF_API GLTF_Primitive
{
    UT_StringMap<uint32> attributes;
    GLTF_Handle indices = GLTF_INVALID_IDX;
    GLTF_Handle material = GLTF_INVALID_IDX;
    GLTF_RenderMode mode = GLTF_RENDERMODE_TRIANGLES;
    // targets
    // extensions
    // extras
};

struct GLTF_API GLTF_Mesh
{
    UT_Array<GLTF_Primitive> primitives;
    // weights
    UT_String name;
    // extensions
    // extras
};

struct GLTF_API GLTF_Sampler
{
    GLTF_TexFilter magfilter;
    GLTF_TexFilter minFilter;
    GLTF_TexWrap wrapS;
    GLTF_TexWrap wrapT;
    UT_String name;
    // extensions
    // extras
};

struct GLTF_API GLTF_Image
{
    UT_String uri = "";
    UT_String mimeType = "";
    GLTF_Handle bufferView = GLTF_INVALID_IDX;
    UT_String name = "";
    // extensions
    // extras
};

struct GLTF_API GLTF_Node
{
    GLTF_Handle camera = GLTF_INVALID_IDX;
    UT_Array<uint32> children;
    GLTF_Handle skin = GLTF_INVALID_IDX;
    UT_Matrix4 matrix = UT_Matrix4::getIdentityMatrix();
    GLTF_Handle mesh = GLTF_INVALID_IDX;
    UT_Vector4 rotation = {0, 0, 0, 1};
    UT_Vector3 scale = {1, 1, 1};
    UT_Vector3 translation = {0, 0, 0};
    // weights
    UT_String name = "";
    // Extensions
    // Extras

    // GLTF standard specifies that only a Matrix OR TRS transform
    // is stored.
    GLTF_TRANSFORM_TYPE getTransformType() const;
    void getTransformAsMatrix(UT_Matrix4F &mat) const;
};

struct GLTF_API GLTF_Scene
{
    UT_Array<uint32> nodes;
    UT_String name = "";
    // extensions
    // extras
};

struct GLTF_API GLTF_Skin
{
    // inverse bind matrices
    // skeleton
    // joins
    // extensions
    // extras
};

struct GLTF_API GLTF_Sparse
{
    // count
    // indices
    // values
    // extensions
    // extras
};

struct GLTF_API GLTF_Target
{
    GLTF_Node *node;
    UT_String path;
    // extensions
    // extras
};

struct GLTF_API GLTF_Texture
{
    uint32 sampler = GLTF_INVALID_IDX;
    uint32 source = GLTF_INVALID_IDX;
    UT_String name = "";
    // extensions
    // extras
};

} // end GLTF_NAMESPACE

#endif
