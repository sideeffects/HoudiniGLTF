/*
 * Copyright (c) 2020
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

#include "GLTF_Loader.h"
#include "GLTF_Util.h"

#include <UT/UT_DirUtil.h>
#include <UT/UT_FileStat.h>
#include <UT/UT_IStream.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONValue.h>
#include <UT/UT_JSONValueArray.h>
#include <UT/UT_JSONValueMap.h>
#include <UT/UT_StringMap.h>
#include <UT/UT_WorkBuffer.h>
#include <UT/UT_Endian.h>

#include <UT/UT_Base64.h>

using namespace GLTF_NAMESPACE;

//===================================================
// Parsing convenience functions

bool
ParseAsString(const UT_JSONValue *val, const bool required, const char **uri)
{
    if (!val && !required)
        return true;
    
    if (!val || val->getType() != UT_JSONValue::JSON_STRING)
        return false;

    *uri = val->getS();
    return true;
}

bool
ParseAsString(const UT_JSONValue *val, const bool required, UT_String *uri)
{
    if (!val && !required)
        return true;

    if (!val || val->getType() != UT_JSONValue::JSON_STRING)
        return false;

    *uri = UT_String(val->getS());
    uri->harden();
    return true;
}

bool
ParseAsFloat(const UT_JSONValue *val, const bool required, fpreal32 *f)
{
    if (!val && !required)
        return true;

    if (!val || val->getType() != UT_JSONValue::JSON_REAL)
        return false;

    *f = static_cast<fpreal32>(val->getF());
    return true;
}

bool
ParseAsInteger(const UT_JSONValue *val, const bool required, uint32 *i)
{
    if (!val && !required)
        return true;

    if (!val || val->getType() != UT_JSONValue::JSON_INT)
        return false;

    *i = val->getI();
    return true;
}

bool
ParseAsBool(const UT_JSONValue *val, const bool required, bool *b)
{
    if (!val && !required)
        return true;

    else if (val && val->getType() == UT_JSONValue::JSON_BOOL)
    {
        *b = val->getB();
        return true;
    }

    return false;
}

template <typename Vectype, uint32 length>
bool
ParseAsFloatVec(const UT_JSONValue *val, const bool required, Vectype &v)
{
    SYS_STATIC_ASSERT(length > 0);

    if (!val && !required)
        return true;

    if (!val || val->getType() != UT_JSONValue::JSON_ARRAY)
        return false;

    const UT_JSONValueArray &valarray = *val->getArray();

    if (valarray.size() != length)
        return false;

    for (uint32 i = 0; i < valarray.size(); i++)
    {
        if (valarray[i]->getType() != UT_JSONValue::JSON_REAL &&
            valarray[i]->getType() != UT_JSONValue::JSON_INT)
        {
            return false;
        }
        v[i] = valarray[i]->getF();
    }

    return true;
}

template <typename Mattype, uint32 length>
bool
ParseAsFloatMat(const UT_JSONValue *val, const bool required, Mattype &v)
{
    SYS_STATIC_ASSERT(length > 0);

    if (!val && !required)
        return true;

    if (!val || val->getType() != UT_JSONValue::JSON_ARRAY)
        return false;

    const UT_JSONValueArray &valarray = *val->getArray();

    if (valarray.size() != length * length)
        return false;

    for (uint32 r = 0; r < length; r++)
    {
        for (uint32 c = 0; c < length; c++)
        {
            const uint32 idx = c + length * r;
            const UT_JSONValue *elem = valarray[idx];
            if (elem->getType() != UT_JSONValue::JSON_REAL &&
                elem->getType() != UT_JSONValue::JSON_INT)
            {
                return false;
            }
            v[r][c] = valarray[idx]->getF();
        }
    }

    return true;
}

GLTF_ComponentType
ConvertToComponentType(uint32 component_type)
{
    if (component_type >= GLTF_COMPONENT_BYTE &&
	component_type <= GLTF_COMPONENT_FLOAT &&
	component_type != 5124)
    {
        return static_cast<GLTF_ComponentType>(component_type);
    }
    return GLTF_COMPONENT_INVALID;
}

GLTF_Type
ConvertStringTogltf_type(const char *str)
{
    std::string typestring(str);
    if (typestring == GLTF_TYPE_NAME_SCALAR)
        return GLTF_TYPE_SCALAR;
    else if (typestring == GLTF_TYPE_NAME_VEC2)
        return GLTF_TYPE_VEC2;
    else if (typestring == GLTF_TYPE_NAME_VEC3)
        return GLTF_TYPE_VEC3;
    else if (typestring == GLTF_TYPE_NAME_VEC4)
        return GLTF_TYPE_VEC4;
    else if (typestring == GLTF_TYPE_NAME_MAT2)
        return GLTF_TYPE_MAT2;
    else if (typestring == GLTF_TYPE_NAME_MAT3)
        return GLTF_TYPE_MAT3;
    else if (typestring == GLTF_TYPE_NAME_MAT4)
        return GLTF_TYPE_MAT4;

    return GLTF_Type::GLTF_TYPE_INVALID;
}

GLTF_RenderMode
ConvertToRenderMode(uint32 rendermode)
{
    if (rendermode <= GLTF_RENDERMODE_TRIANGLE_FAN)
        return static_cast<GLTF_RenderMode>(rendermode);

    return GLTF_RENDERMODE_INVALID;
}

//==========================================================================================

GLTF_Loader::GLTF_Loader() {}

GLTF_Loader::GLTF_Loader(UT_String filename)
    : myFilename(filename), myIsLoaded(false)
{
    myFilename.harden();

    // Immediately discarded - splitPath requires 2 args
    UT_String tmp;
    myFilename.splitPath(myBasePath, tmp);
}

GLTF_Loader::~GLTF_Loader()
{
    for (unsigned char *buffer : myBufferCache)
    {
        if (buffer)
            free(buffer);
    }
}

bool
GLTF_Loader::Load()
{
    // First check whether we are dealing with a .glb or .gltf file
    UT_String extension = UT_String(myFilename.fileExtension());
    extension.toLower();
    // todo
    if (extension == ".gltf")
    {
        if (!ReadGLTF())
            return false;
    }
    else if (extension == ".glb")
    {
        if (!ReadGLB())
            return false;
    }
    else
    {
        // Unknown file type
        return false;
    }

    myBufferCache.setSize(myBuffers.size());
    myBufferCache.appendMultiple(nullptr, myBuffers.size());
    myIsLoaded = true;
    return true;
}

bool
GLTF_Loader::LoadAccessorData(const GLTF_Accessor &accessor,
                              unsigned char *&data) const
{
    UT_AutoLock accessorLock(myAccessorLock);

    const GLTF_BufferView &bv = *myBufferViews[accessor.bufferView];

    unsigned char *buffer_data;
    if (!LoadBuffer(bv.buffer, buffer_data))
        return false;

    data = buffer_data + bv.byteOffset + accessor.byteOffset;

    return true;
}

bool
GLTF_Loader::ReadJSON(const UT_JSONValueMap &root_json)
{
    if (root_json["asset"] &&
        root_json["asset"]->getType() == UT_JSONValue::JSON_MAP)
    {
        ReadAsset(*root_json["asset"]->getMap());
    }
    else
    {
        return false;
    }

    if (!ReadArrayOfMaps(root_json["accessors"], false,
                         [&](const UT_JSONValueMap &map, const exint idx) -> bool {
                             return ReadAccessor(map, idx);
                         }))
    {
        return false;
    }
    if (!ReadArrayOfMaps(root_json["buffers"], false,
                         [&](const UT_JSONValueMap &map, const exint idx) -> bool {
                             return ReadBuffer(map, idx);
                         }))
    {
        return false;
    }
    if (!ReadArrayOfMaps(root_json["bufferViews"], false,
                         [&](const UT_JSONValueMap &map, const exint idx) -> bool {
                             return ReadBufferView(map, idx);
                         }))
    {
        return false;
    }
    if (!ReadArrayOfMaps(
            root_json["meshes"], false,
            [&](const UT_JSONValueMap &map, const exint idx) -> bool { return ReadMesh(map, idx); }))
    {
        return false;
    }
    if (!ReadArrayOfMaps(
            root_json["nodes"], false,
            [&](const UT_JSONValueMap &map, const exint idx) -> bool { return ReadNode(map, idx); }))
    {
        return false;
    }
    if (!ReadArrayOfMaps(root_json["textures"], false,
                         [&](const UT_JSONValueMap &map, const exint idx) -> bool {
                             return ReadTexture(map, idx);
                         }))
    {
        return false;
    }
    if (!ReadArrayOfMaps(root_json["samplers"], false,
                         [&](const UT_JSONValueMap &map, const exint idx) -> bool {
                             return ReadSampler(map, idx);
                         }))
    {
        return false;
    }
    if (!ReadArrayOfMaps(
            root_json["images"], false,
            [&](const UT_JSONValueMap &map, const exint idx) -> bool { return ReadImage(map, idx); }))
    {
        return false;
    }

    if (!ReadArrayOfMaps(
            root_json["scenes"], false,
            [&](const UT_JSONValueMap &map, const exint idx) -> bool { return ReadScene(map, idx); }))
    {
        return false;
    }

    if (!ReadArrayOfMaps(
            root_json["materials"], false,
            [&](const UT_JSONValueMap &map, const exint idx) -> bool { return ReadMaterial(map, idx); }))
    {
        return false;
    }

    if (!ParseAsInteger(root_json["scene"], false, &myScene))
    {
        return false;
    }

    return true;
}

bool
GLTF_Loader::ReadGLTF()
{
    UT_IFStream is;
    UT_JSONValue keymap;

    if (!is.open(myFilename, UT_ISTREAM_ASCII))
        return false;

    UT_AutoJSONParser jsonParser(is);

    if (!keymap.parseValue(jsonParser))
        return false;

    if (keymap.getType() != UT_JSONValue::JSON_MAP)
        return false;

    if (!ReadJSON(*keymap.getMap()))
        return false;

    return true;
}

bool
GLTF_Loader::ReadGLB()
{
    UT_IFStream is;
    UT_JSONValue keymap;

    if (!is.open(myFilename, UT_ISTREAM_ASCII))
        return false;

    unsigned char header[12];
    if (is.bread(header, 12) != 12)
        return false;

    unsigned char json_header[8];
    if (is.bread(json_header, 8) != 8)
        return false;

    uint32 json_size = *reinterpret_cast<uint32 *>(&json_header);

    // Support big endian systems
    UTtovax(json_size);

    char *json_data = static_cast<char *>(malloc(json_size));

    // RAII for json_copy
    std::unique_ptr<char, decltype(std::free) *> 
	json_copy{strdup(json_data), std::free};

    if (is.bread(json_data, json_size) != json_size)
        return false;

    unsigned char bin_header[8];
    if (is.bread(bin_header, 8) != 8)
        return false;

    uint32 bin_size = *reinterpret_cast<uint32 *>(&bin_header);
    UTtovax(json_size);

    char *bin_data = static_cast<char *>(malloc(bin_size));
    
    // RAII for bindata
    std::unique_ptr<char, decltype(std::free) *> 
	bin_copy{strdup(bin_data), std::free};

    if (is.bread(bin_data, bin_size) != bin_size)
        return false;

    UT_AutoJSONParser json_parser(json_data, json_size);

    if (!keymap.parseValue(json_parser))
        return false;

    if (keymap.getType() != UT_JSONValue::JSON_MAP)
        return false;

    if (!ReadJSON(*keymap.getMap()))
        return false;

    UT_ASSERT(myBufferCache.size() == 0);

    myBufferCache.append(reinterpret_cast<unsigned char *>(bin_data));

    // Ownership is passed off to myBufferCache
    bin_copy.release();

    return true;
}

bool
GLTF_Loader::ReadAsset(const UT_JSONValueMap &asset_json)
{
    GLTF_Asset &asset = myAsset;
    if (!ParseAsString(asset_json["copyright"], false, &asset.copyright))
        return false;
    if (!ParseAsString(asset_json["generator"], false, &asset.minversion))
        return false;
    if (!ParseAsString(asset_json["version"], false, &asset.version))
        return false;
    if (!ParseAsString(asset_json["minversion"], false, &asset.minversion))
        return false;

    return true;
}

bool
GLTF_Loader::ReadNode(const UT_JSONValueMap &node_json, const exint idx)
{
    auto node = UTmakeUnique<GLTF_Node>();
    const UT_JSONValue *children = node_json["children"];

    if (children)
    {
        if (children->getType() != UT_JSONValue::JSON_ARRAY)
            return false;

        const UT_JSONValueArray &children_array = *children->getArray();
        for (uint32 i = 0; i < children_array.size(); i++)
        {
            uint32 val;
            ParseAsInteger(children_array[i], true, &val);
            node->children.append(val);
        }
    }

    if (!ParseAsInteger(node_json["mesh"], false, &node->mesh))
        return false;
    if (!ParseAsFloatMat<UT_Matrix4F, 4>(node_json["matrix"], false,
                                         node->matrix))
        return false;
    if (!ParseAsFloatVec<UT_Vector4F, 4>(node_json["rotation"], false,
                                         node->rotation))
        return false;
    if (!ParseAsFloatVec<UT_Vector3F, 3>(node_json["scale"], false,
                                         node->scale))
        return false;
    if (!ParseAsFloatVec<UT_Vector3F, 3>(node_json["translation"], false,
                                         node->translation))
        return false;
    if (!ParseAsString(node_json["name"], false, &node->name))
        return false;

    myNodes.append(node.get());
    node.release();

    return true;
}

bool
GLTF_Loader::ReadBuffer(const UT_JSONValueMap &buffer_json, const exint idx)
{
    auto buffer = UTmakeUnique<GLTF_Buffer>();

    if (!ParseAsString(buffer_json["uri"], false, &buffer->myURI))
        return false;
    if (!ParseAsInteger(buffer_json["byteLength"], true, &buffer->myByteLength))
        return false;
    if (!ParseAsString(buffer_json["name"], false, &buffer->name))
        return false;

    myBuffers.append(buffer.get());
    buffer.release();

    return true;
}

bool
GLTF_Loader::ReadBufferView(const UT_JSONValueMap &bufferview_json, const exint idx)
{
    auto bufferview = UTmakeUnique<GLTF_BufferView>();
    uint32 target = 0;

    if (!ParseAsInteger(bufferview_json["buffer"], true, &bufferview->buffer))
        return false;
    if (!ParseAsInteger(bufferview_json["byteOffset"], false,
                        &bufferview->byteOffset))
        return false;
    if (!ParseAsInteger(bufferview_json["byteLength"], true,
                        &bufferview->byteLength))
        return false;
    if (!ParseAsInteger(bufferview_json["byteStride"], false,
                        &bufferview->byteStride))
        return false;
    if (!ParseAsInteger(bufferview_json["target"], false, &target))
        return false;
    if (!ParseAsString(bufferview_json["name"], false, &bufferview->name))
        return false;

    if (target == 0)
    {
        target = GLTF_BufferViewTarget::GLTF_BUFFER_ARRAY;
    }
    bufferview->target = static_cast<GLTF_BufferViewTarget>(target);

    if (bufferview->buffer >= myBuffers.size())
        return false;

    myBufferViews.append(bufferview.release());

    return true;
}

bool
GLTF_Loader::ReadAccessor(const UT_JSONValueMap &accessor_json, const exint idx)
{
    auto accessor = UTmakeUnique<GLTF_Accessor>();
    uint32 componentType;
    const char *type;
    GLTF_Type gltf_type;

    if (!ParseAsInteger(accessor_json["bufferView"], false,
                        &accessor->bufferView))
        return false;
    if (!ParseAsInteger(accessor_json["byteOffset"], false,
                        &accessor->byteOffset))
        return false;
    if (!ParseAsInteger(accessor_json["componentType"], true, &componentType))
        return false;
    if (!ParseAsBool(accessor_json["normalized"], false, &accessor->normalized))
        return false;
    if (!ParseAsInteger(accessor_json["count"], true, &accessor->count))
        return false;
    if (!ParseAsString(accessor_json["type"], true, &type))
        return false;
    if (!ParseAsString(accessor_json["name"], false, &accessor->name))
        return false;

    gltf_type = ConvertStringTogltf_type(type);
    accessor->type = gltf_type;

    accessor->componentType = ConvertToComponentType(componentType);

    if (accessor->componentType == GLTF_ComponentType::GLTF_COMPONENT_INVALID)
        return false;

    myAccesors.append(accessor.get());
    accessor.release();
    return true;
}

bool
GLTF_Loader::ReadPrimitive(const UT_JSONValue *primitive_json,
                           GLTF_Primitive *primitive)
{
    UT_StringMap<uint32> attributes_map;
    uint32 prim_mode = GLTF_RenderMode::GLTF_RENDERMODE_TRIANGLES;

    if (primitive_json->getType() != UT_JSONValue::JSON_MAP)
        return false;

    const UT_JSONValueMap &prim_json = *(primitive_json->getMap());

    // Parse attributes property
    const UT_JSONValue *attributes = prim_json["attributes"];
    if (!attributes)
        return false;

    const UT_JSONValueMap &attributes_json_map = *attributes->getMap();
    UT_StringArray keys;
    attributes_json_map.getKeyReferences(keys);
    for (const UT_StringHolder &key : keys)
    {
        const UT_JSONValue *attribute = attributes_json_map[key];
        uint32 ind;
        if (!ParseAsInteger(attribute, true, &ind))
            return false;

        attributes_map.insert({key, ind});
    }

    if (!ParseAsInteger(prim_json["indices"], false, &primitive->indices))
        return false;
    if (!ParseAsInteger(prim_json["material"], false, &primitive->material))
        return false;
    if (prim_json["mode"])
    {
        if (!ParseAsInteger(prim_json["mode"], false, &prim_mode))
            return false;
    }

    primitive->mode = ConvertToRenderMode(prim_mode);
    if (primitive->mode == GLTF_RenderMode::GLTF_RENDERMODE_INVALID)
        return false;

    primitive->attributes = attributes_map;
    return true;
}

bool
GLTF_Loader::ReadMesh(const UT_JSONValueMap &mesh_json, const exint idx)
{
    auto mesh = UTmakeUnique<GLTF_Mesh>();

    // Parse the primitives in the mesh
    const UT_JSONValue *primitives = mesh_json["primitives"];
    if (!primitives || primitives->getType() != UT_JSONValue::JSON_ARRAY)
    {
        return false;
    }

    const UT_JSONValueArray &primitives_array = *primitives->getArray();
    for (uint32 i = 0; i < primitives_array.size(); i++)
    {
        GLTF_Primitive prim;
        if (!ReadPrimitive(primitives_array[i], &prim))
            return false;
        mesh->primitives.append(std::move(prim));
    }

    if (!ParseAsString(mesh_json["name"], false, &mesh->name))
        return false;

    myMeshes.append(mesh.get());
    mesh.release();

    return true;
}

bool
GLTF_Loader::ReadTexture(const UT_JSONValueMap &texture_json, const exint idx)
{
    auto texture = UTmakeUnique<GLTF_Texture>();

    if (!ParseAsInteger(texture_json["sampler"], false, &texture->sampler))
        return false;
    if (!ParseAsInteger(texture_json["source"], false, &texture->source))
        return false;
    if (!ParseAsString(texture_json["name"], false, &texture->name))
        return false;

    myTextures.append(texture.get());
    texture.release();

    return true;
}

bool
GLTF_Loader::ReadSampler(const UT_JSONValueMap &sampler_json, const exint idx)
{
    auto sampler = UTmakeUnique<GLTF_Sampler>();

    uint32 magFilter = GLTF_TexFilter::GLTF_TEXFILTER_INVALID;
    uint32 minFilter = GLTF_TexFilter::GLTF_TEXFILTER_INVALID;
    uint32 wrapS = GLTF_TexWrap::GLTF_TEXWRAP_INVALID;
    uint32 wrapT = GLTF_TexWrap::GLTF_TEXWRAP_INVALID;

    if (sampler_json["magFilter"])
    {
        if (!ParseAsInteger(sampler_json["magFilter"], true, &magFilter))
            return false;
    }
    if (sampler_json["minFilter"])
    {
        if (!ParseAsInteger(sampler_json["minFilter"], true, &minFilter))
            return false;
    }
    if (sampler_json["wrapS"])
    {
        if (!ParseAsInteger(sampler_json["wrapS"], true, &wrapS))
            return false;
    }
    if (sampler_json["wrapS"])
    {
        if (!ParseAsInteger(sampler_json["wrapT"], true, &wrapT))
            return false;
    }

    // todo:  Check the types!
    sampler->magfilter = static_cast<GLTF_TexFilter>(magFilter);
    sampler->minFilter = static_cast<GLTF_TexFilter>(magFilter);
    sampler->wrapS = static_cast<GLTF_TexWrap>(magFilter);
    sampler->wrapT = static_cast<GLTF_TexWrap>(magFilter);

    mySamplers.append(sampler.get());
    sampler.release();

    return true;
}

bool
GLTF_Loader::ReadImage(const UT_JSONValueMap &image_json, const exint idx)
{
    auto image = UTmakeUnique<GLTF_Image>();

    if (!ParseAsString(image_json["uri"], false, &image->uri))
        return false;
    if (!ParseAsString(image_json["mimeType"], false, &image->mimeType))
        return false;
    if (!ParseAsInteger(image_json["bufferView"], false, &image->bufferView))
        return false;
    if (!ParseAsString(image_json["name"], false, &image->name))
        return false;

    myImages.append(image.get());
    image.release();

    return true;
}

bool
GLTF_Loader::ReadMaterial(const UT_JSONValueMap &material_json, const exint idx)
{
    // Just read the name in...
    // todo:  we can read the rest in if we ever need to
    auto material = UTmakeUnique<GLTF_Material>();

	if (!material_json["name"])
	{
		// If the glTF JSON material has no name, use the default 'principledshader'
		// name with index starting at 1. This is to match what the python script in 
		// the gltf_hierarchy loader creates (a principledshader node with default name).
		material->name = UT_String("principledshader" + std::to_string(idx + 1));
	}
    else if (!ParseAsString(material_json["name"], false, &material->name))
        return false;

    myMaterials.append(material.get());
    material.release();
    
    return true;
}

bool
GLTF_Loader::ReadScene(const UT_JSONValueMap &scene_json, const exint idx)
{
    auto scene = UTmakeUnique<GLTF_Scene>();

    const UT_JSONValue *nodes = scene_json["nodes"];
    if (nodes)
    {
        if (nodes->getType() != UT_JSONValue::JSON_ARRAY)
            return false;

        const UT_JSONValueArray &nodes_array = *nodes->getArray();
        for (uint32 i = 0; i < nodes_array.size(); i++)
        {
            uint32 val;
            ParseAsInteger(nodes_array[i], true, &val);
            scene->nodes.append(val);
        }
    }

    if (!ParseAsString(scene_json["name"], false, &scene->name))
        return false;

    myScenes.append(scene.get());
    scene.release();

    return true;
}

bool
GLTF_Loader::ReadArrayOfMaps(const UT_JSONValue *arr, bool required,
                             std::function<bool(const UT_JSONValueMap &, const exint idx)> func)
{
    if (required && !arr)
        return false;

    // Read in nodes
    if (arr)
    {
        if (arr->getType() != UT_JSONValue::JSON_ARRAY)
            return false;

        const UT_JSONValueArray &elem_json = *arr->getArray();
        for (exint i = 0; i < elem_json.size(); i++)
        {
            const UT_JSONValue *elem = elem_json[i];
            if (elem->getType() != UT_JSONValue::JSON_MAP)
                return false; // Invalid
            if (!func(*elem->getMap(), i))
                return false;
        }
    }

    return true;
}

bool
GLTF_Loader::LoadBuffer(uint32 idx, unsigned char *&buffer_data) const
{
    if (myBufferCache[idx] != nullptr)
    {
        buffer_data = myBufferCache[idx];
        return true;
    }

    // Handle buffer data stored in external .bin file
    const GLTF_Buffer &buffer = *myBuffers[idx];
    const uint32 buffer_size = buffer.myByteLength;
    UT_IFStream is;

    constexpr const char *header = "data:application/octet-stream;base64,";

    // Handle base64 encoded buffer data
    if (buffer.myURI.startsWith(header))
    {
        UT_String b64str;
        buffer.myURI.substr(b64str, strlen(header));

        UT_WorkBuffer dst;
        UT_String uri_tag;

        if (UT_Base64::decode(b64str.c_str(), dst))
        {
            unsigned char *data;
            const exint decoded_length = dst.end() - dst.begin();

            if (decoded_length != buffer_size)
                return false;

            data = static_cast<unsigned char *>(malloc(buffer_size));

            buffer_data = data;

            // Copy data from the workbuffer to the cache
            memcpy(data, dst.begin(), decoded_length);
            myBufferCache[idx] = data;

            return true;
        }

        return false;
    }

    // Decoded normal, bin stored data
    unsigned char *data;
    UT_String absolute_path = buffer.myURI;
    exint actual_size;

    UTmakeAbsoluteFilePath(absolute_path, myBasePath.c_str());

    if (!is.open(absolute_path, UT_ISTREAM_BINARY))
        return false;

    data = static_cast<unsigned char *>(malloc(buffer_size));

    // Read in the data
    actual_size = is.bread(data, buffer_size);
    if (actual_size != buffer_size)
    {
        free(data);
        return false;
    }

    is.close();
    buffer_data = data;
    myBufferCache[idx] = data;

    return true;
}

GLTF_Accessor const *
GLTF_Loader::getAccessor(GLTF_Handle idx) const
{
    if (getNumAccessors() < idx)
        return nullptr;
    return myAccesors[idx];
}

GLTF_Animation const *
GLTF_Loader::getAnimation(GLTF_Handle idx) const
{
    if (idx >= getNumAnimations())
        return nullptr;
    return myAnimations[idx];
}

GLTF_Asset
GLTF_Loader::getAsset() const
{
    return myAsset;
}

GLTF_Buffer const *
GLTF_Loader::getBuffer(GLTF_Handle idx) const
{
    if (idx >= getNumBuffers())
        return nullptr;
    return myBuffers[idx];
}

GLTF_BufferView const *
GLTF_Loader::getBufferView(GLTF_Handle idx) const
{
    if (idx >= getNumBufferViews())
        return nullptr;
    return myBufferViews[idx];
}

GLTF_Camera const *
GLTF_Loader::getCamera(GLTF_Handle idx) const
{
    if (idx >= getNumCameras())
        return nullptr;
    return myCameras[idx];
}

GLTF_Image const *
GLTF_Loader::getImage(GLTF_Handle idx) const
{
    if (idx >= getNumImages())
        return nullptr;
    return myImages[idx];
}

GLTF_Material const *
GLTF_Loader::getMaterial(GLTF_Handle idx) const
{
    if (idx >= getNumMaterials())
        return nullptr;
    return myMaterials[idx];
}

GLTF_Mesh const *
GLTF_Loader::getMesh(GLTF_Handle idx) const
{
    if (idx >= getNumMeshes())
        return nullptr;
    return myMeshes[idx];
}

GLTF_Node const *
GLTF_Loader::getNode(GLTF_Handle idx) const
{
    if (idx >= getNumNodes())
        return nullptr;
    return myNodes[idx];
}

GLTF_Sampler const *
GLTF_Loader::getSampler(GLTF_Handle idx) const
{
    if (idx >= getNumSamplers())
        return nullptr;
    return mySamplers[idx];
}

GLTF_Handle
GLTF_Loader::getDefaultScene() const
{
    return myScene;
}

GLTF_Scene const *
GLTF_Loader::getScene(GLTF_Handle idx) const
{
    if (idx >= getNumScenes())
        return nullptr;
    return myScenes[idx];
}

GLTF_Skin const *
GLTF_Loader::getSkin(GLTF_Handle idx) const
{
    if (idx >= getNumSkins())
        return nullptr;
    return mySkins[idx];
}

GLTF_Texture const *
GLTF_Loader::getTexture(GLTF_Handle idx) const
{
    if (idx >= getNumTextures())
        return nullptr;
    return myTextures[idx];
}

GLTF_Accessor *
GLTF_Loader::getAccessor(GLTF_Handle idx)
{
    return const_cast<GLTF_Accessor *>(
        static_cast<const GLTF_Loader *>(this)->getAccessor(idx));
}

GLTF_Animation *
GLTF_Loader::getAnimation(GLTF_Handle idx)
{
    return const_cast<GLTF_Animation *>(
        static_cast<const GLTF_Loader *>(this)->getAnimation(idx));
}

GLTF_Buffer *
GLTF_Loader::getBuffer(GLTF_Handle idx)
{
    return const_cast<GLTF_Buffer *>(
        static_cast<const GLTF_Loader *>(this)->getBuffer(idx));
}

GLTF_BufferView *
GLTF_Loader::getBufferView(GLTF_Handle idx)
{
    return const_cast<GLTF_BufferView *>(
        static_cast<const GLTF_Loader *>(this)->getBufferView(idx));
}

GLTF_Camera *
GLTF_Loader::getCamera(GLTF_Handle idx)
{
    return const_cast<GLTF_Camera *>(
        static_cast<const GLTF_Loader *>(this)->getCamera(idx));
}

GLTF_Image *
GLTF_Loader::getImage(GLTF_Handle idx)
{
    return const_cast<GLTF_Image *>(
        static_cast<const GLTF_Loader *>(this)->getImage(idx));
}

GLTF_Material *
GLTF_Loader::getMaterial(GLTF_Handle idx)
{
    return const_cast<GLTF_Material *>(
        static_cast<const GLTF_Loader *>(this)->getMaterial(idx));
}

GLTF_Mesh *
GLTF_Loader::getMesh(GLTF_Handle idx)
{
    return const_cast<GLTF_Mesh *>(
        static_cast<const GLTF_Loader *>(this)->getMesh(idx));
}

GLTF_Node *
GLTF_Loader::getNode(GLTF_Handle idx)
{
    return const_cast<GLTF_Node *>(
        static_cast<const GLTF_Loader *>(this)->getNode(idx));
}

GLTF_Sampler *
GLTF_Loader::getSampler(GLTF_Handle idx)
{
    return const_cast<GLTF_Sampler *>(
        static_cast<const GLTF_Loader *>(this)->getSampler(idx));
}

GLTF_Scene *
GLTF_Loader::getScene(GLTF_Handle idx)
{
    return const_cast<GLTF_Scene *>(
        static_cast<const GLTF_Loader *>(this)->getScene(idx));
}

GLTF_Skin *
GLTF_Loader::getSkin(GLTF_Handle idx)
{
    return const_cast<GLTF_Skin *>(
        static_cast<const GLTF_Loader *>(this)->getSkin(idx));
}

GLTF_Texture *
GLTF_Loader::getTexture(GLTF_Handle idx)
{
    return const_cast<GLTF_Texture *>(
        static_cast<const GLTF_Loader *>(this)->getTexture(idx));
}

void
GLTF_Loader::setDefaultScene(const GLTF_Handle &idx)
{
    myScene = idx;
}

void
GLTF_Loader::setAsset(const GLTF_Asset &asset)
{
    myAsset = asset;
}

exint
GLTF_Loader::getNumAccessors() const
{
    return myAccesors.size();
}
exint
GLTF_Loader::getNumAnimations() const
{
    return myAnimations.size();
}
exint
GLTF_Loader::getNumBuffers() const
{
    return myBuffers.size();
}
exint
GLTF_Loader::getNumBufferViews() const
{
    return myBufferViews.size();
}
exint
GLTF_Loader::getNumCameras() const
{
    return myCameras.size();
}
exint
GLTF_Loader::getNumImages() const
{
    return myImages.size();
}
exint
GLTF_Loader::getNumMaterials() const
{
    return myMaterials.size();
}
exint
GLTF_Loader::getNumMeshes() const
{
    return myMeshes.size();
}
exint
GLTF_Loader::getNumNodes() const
{
    return myNodes.size();
}
exint
GLTF_Loader::getNumSamplers() const
{
    return mySamplers.size();
}
exint
GLTF_Loader::getNumScenes() const
{
    return myScenes.size();
}
exint
GLTF_Loader::getNumSkins() const
{
    return mySkins.size();
}
exint
GLTF_Loader::getNumTextures() const
{
    return myTextures.size();
}

const UT_Array<GLTF_Accessor *> &
GLTF_Loader::getAccessors() const
{
    return myAccesors;
}

const UT_Array<GLTF_Animation *> &
GLTF_Loader::getAnimations() const
{
    return myAnimations;
}

const UT_Array<GLTF_Buffer *> &
GLTF_Loader::getBuffers() const
{
    return myBuffers;
}

const UT_Array<GLTF_BufferView *> &
GLTF_Loader::getBufferViews() const
{
    return myBufferViews;
}

const UT_Array<GLTF_Camera *> &
GLTF_Loader::getCameras() const
{
    return myCameras;
}

const UT_Array<GLTF_Image *> &
GLTF_Loader::getImages() const
{
    return myImages;
}

const UT_Array<GLTF_Material *> &
GLTF_Loader::getMaterials() const
{
    return myMaterials;
}

const UT_Array<GLTF_Mesh *> &
GLTF_Loader::getMeshes() const
{
    return myMeshes;
}

const UT_Array<GLTF_Node *> &
GLTF_Loader::getNodes() const
{
    return myNodes;
}

const UT_Array<GLTF_Sampler *> &
GLTF_Loader::getSamplers() const
{
    return mySamplers;
}

const UT_Array<GLTF_Scene *> &
GLTF_Loader::getScenes() const
{
    return myScenes;
}

const UT_Array<GLTF_Skin *> &
GLTF_Loader::getSkins() const
{
    return mySkins;
}

const UT_Array<GLTF_Texture *> &
GLTF_Loader::getTextures() const
{
    return myTextures;
}

void
GLTF_Loader::removeBuffer(GLTF_Handle idx)
{
    myBuffers.removeIndex(idx);
}

void
GLTF_Loader::removeNode(GLTF_Handle idx)
{
    myNodes.removeIndex(idx);
}

// Helper function for the GLTF_Loader::Create* functions
// Use template argument deduction to save code
template <typename T>
static T *
createElem(UT_Array<T *> &arr, GLTF_Handle &idx)
{
    idx = arr.size();
    arr.append(new T());
    return arr[idx];
}

GLTF_Accessor *
GLTF_Loader::createAccessor(GLTF_Handle &idx)
{
    return createElem(myAccesors, idx);
}

GLTF_Animation *
GLTF_Loader::createAnimation(GLTF_Handle &idx)
{
    return createElem(myAnimations, idx);
}

GLTF_Buffer *
GLTF_Loader::createBuffer(GLTF_Handle &idx)
{
    return createElem(myBuffers, idx);
}

GLTF_BufferView *
GLTF_Loader::createBufferView(GLTF_Handle &idx)
{
    return createElem(myBufferViews, idx);
}

GLTF_Camera *
GLTF_Loader::createCamera(GLTF_Handle &idx)
{
    return createElem(myCameras, idx);
}

GLTF_Image *
GLTF_Loader::createImage(GLTF_Handle &idx)
{
    return createElem(myImages, idx);
}

GLTF_Material *
GLTF_Loader::createMaterial(GLTF_Handle &idx)
{
    return createElem(myMaterials, idx);
}

GLTF_Mesh *
GLTF_Loader::createMesh(GLTF_Handle &idx)
{
    return createElem(myMeshes, idx);
}

GLTF_Node *
GLTF_Loader::createNode(GLTF_Handle &idx)
{
    return createElem(myNodes, idx);
}

GLTF_Sampler *
GLTF_Loader::createSampler(GLTF_Handle &idx)
{
    return createElem(mySamplers, idx);
}

GLTF_Scene *
GLTF_Loader::createScene(GLTF_Handle &idx)
{
    return createElem(myScenes, idx);
}

GLTF_Skin *
GLTF_Loader::createSkin(GLTF_Handle &idx)
{
    return createElem(mySkins, idx);
}

GLTF_Texture *
GLTF_Loader::createTexture(GLTF_Handle &idx)
{
    return createElem(myTextures, idx);
}
