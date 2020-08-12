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

#include "ROP_GLTF_ExportRoot.h"
#include <GLTF/GLTF_Util.h>
#include <UT/UT_Endian.h>
#include <UT/UT_FileUtil.h>
#include <UT/UT_JSONWriter.h>
#include <UT/UT_OFStream.h>
#include <UT/UT_OStream.h>

using namespace GLTF_NAMESPACE;

///////////////////////////////////////////////////////////////////////////////
// Convenience functions for outputting JSON

static void
Output(UT_JSONWriter &writer, const char *string, fpreal32 value)
{
    writer.jsonKey(string);
    writer.jsonReal(value);
}

static void
Output(UT_JSONWriter &writer, const char *string, int64 value)
{
    writer.jsonKey(string);
    writer.jsonInt(value);
}

static void
Output(UT_JSONWriter &writer, const char *string, uint32 value)
{
    writer.jsonKey(string);
    writer.jsonInt(static_cast<int64>(value));
}

static void
Output(UT_JSONWriter &writer, const char *string, bool value)
{
    writer.jsonKey(string);
    writer.jsonBool(value);
}

static void
Output(UT_JSONWriter &writer, const char *string, const char *value)
{
    writer.jsonKey(string);
    if (value)
	writer.jsonString(value);
    else
	writer.jsonString("");
}

static void
Output(UT_JSONWriter &writer, const char *string, UT_Vector3F value)
{
    writer.jsonKey(string);
    writer.jsonBeginArray();
    for (exint i = 0; i < 3; i++)
        writer.jsonReal(value[i]);
    writer.jsonEndArray();
}

static void
OutputDefault(UT_JSONWriter &writer, const char *string, const char *value)
{
    if (!value)
        return;
    if (strcasecmp(value, "") == 0)
        return;

    writer.jsonKey(string);
    writer.jsonString(value);
}

static void
OutputDefault(UT_JSONWriter &writer, const char *string, UT_Vector3F value,
              UT_Vector3F default_value = {0.f, 0.f, 0.f})
{
    if (value == default_value)
        return;
    Output(writer, string, value);
}

static void
OutputDefault(UT_JSONWriter &writer, const char *string, int64 value,
              int64 default_value)
{
    if (value == default_value)
        return;
    Output(writer, string, value);
}

static void
OutputDefault(UT_JSONWriter &writer, const char *string, uint32 value,
              uint32 default_value)
{
    if (value == default_value)
        return;
    Output(writer, string, value);
}

static void
OutputDefault(UT_JSONWriter &writer, const char *string, bool value,
              bool default_value)
{
    if (value == default_value)
        return;
    Output(writer, string, value);
}

static void
OutputDefault(UT_JSONWriter &writer, const char *string, fpreal32 value,
              fpreal32 default_value)
{
    if (value == default_value)
        return;
    Output(writer, string, value);
}

///////////////////////////////////////////////////////////////////////////////

ROP_GLTF_ExportRoot::ROP_GLTF_ExportRoot(ExportSettings s) : mySettings(s) {}

ROP_GLTF_ExportRoot::~ROP_GLTF_ExportRoot() {}

void
ROP_GLTF_ExportRoot::OutputName(UT_JSONWriter &writer, const char *string,
                                const char *value)
{
    if (mySettings.exportNames)
    {
        OutputDefault(writer, "name", value);
    }
}

// TODO:  UT_Array does not have <, so we use std::vector which compares
// lexiographically.
static std::vector<ROP_GLTF_ChannelMapping>
GetSortedChannels(const UT_Array<ROP_GLTF_ChannelMapping> &mapping)
{
    std::vector<ROP_GLTF_ChannelMapping> arr;
    for (auto item : mapping)
        arr.push_back(item);

    std::sort(arr.begin(), arr.end(),
              [&](const ROP_GLTF_ChannelMapping &a,
                  const ROP_GLTF_ChannelMapping &b) {
                  if (a.path < b.path || a.from_channel < b.from_channel ||
                      a.to_channel < b.from_channel)
                  {
                      return true;
                  }
                  return false;
              });

    return arr;
}

bool
ROP_GLTF_ExportRoot::HasCachedChannelImage(
    const UT_Array<ROP_GLTF_ChannelMapping> &mapping) const
{
    std::vector<ROP_GLTF_ChannelMapping> sorted_map =
        GetSortedChannels(mapping);
    return myChannelImageMap.find(sorted_map) != myChannelImageMap.end();
}

GLTF_Handle
ROP_GLTF_ExportRoot::GetCachedChannelImage(
    const UT_Array<ROP_GLTF_ChannelMapping> &mapping)
{
    std::vector<ROP_GLTF_ChannelMapping> sorted_map =
        GetSortedChannels(mapping);
    return myChannelImageMap.find(sorted_map)->second;
}

void
ROP_GLTF_ExportRoot::InsertCachedChannelImage(
    const UT_Array<ROP_GLTF_ChannelMapping> &mapping, GLTF_Handle idx)
{
    std::vector<ROP_GLTF_ChannelMapping> sorted_map =
        GetSortedChannels(mapping);
    myChannelImageMap.insert({sorted_map, idx});
}

UT_Map<UT_StringHolder, uint32> &
ROP_GLTF_ExportRoot::GetImageCache()
{
    return myImageMap;
}

UT_Map<const OP_Node *, uint32> &
ROP_GLTF_ExportRoot::GetMaterialCache()
{
    return myMaterialMap;
}

UT_Map<UT_StringHolder, uint32> &
ROP_GLTF_ExportRoot::GetNameUsagesMap()
{
    return myNameUsagesMap;
}

void *
ROP_GLTF_ExportRoot::BufferAlloc(GLTF_Handle bid, GLTF_Offset bytes,
                                 GLTF_Offset alignment, GLTF_Offset &offset)
{
    UT_ASSERT(myLoader.getNumBuffers() >= bid);

    // Ensure alignment
    const uint32 current_offset = myBufferData[bid].size();
    const uint32 alignment_amount = current_offset % alignment;
    if (alignment_amount != 0)
    {
        uint32 unused;
        BufferAlloc(bid, alignment - alignment_amount, 1, unused);
    }

    UT_ASSERT(myBufferData[bid].size() % alignment == 0);

    uint32 old_size = myBufferData[bid].size();
    myBufferData[bid].setSize(myBufferData[bid].size() + bytes);
    offset = old_size;

    return &myBufferData[bid][old_size];
}

GLTF_NAMESPACE::GLTF_Loader &
ROP_GLTF_ExportRoot::loader()
{
    return myLoader;
}

bool
ROP_GLTF_ExportRoot::OpenFileStreamAtPath(const UT_String &path, UT_OFStream &os)
{
    os.open(path);
    if (os.fail())
    {
	if (!UTcreateDirectoryForFile(path.c_str()))
	    return false;

	os.clear();
	os.open(path);
	if (os.fail())
	    return false;
    }

    return true;
}

bool
ROP_GLTF_ExportRoot::ExportGLTF(const UT_String &path)
{
    UT_OFStream os;
    UT_String dir;
    UT_String filename;

    if (!OpenFileStreamAtPath(path, os))
    {
	return false;
    }

    UT_AutoJSONWriter w(os, false);
    UT_JSONWriter &writer(w.writer());

    // Output buffers to disk
    path.splitPath(dir, filename);

    for (uint32 idx = 0; idx < myLoader.getNumBuffers(); idx++)
    {
        UT_ASSERT(myLoader.getBuffer(idx)->myURI != "");
        OutputBuffer(dir, idx);
    }

    // Preprocess structure
    ResolveBufferLengths();
    RemoveEmptyBuffers();
    ConvertAbsolutePaths(dir);

    // Finally, output the actual JSON
    SerializeJSON(writer);

    os.close();
    return true;
}

bool
ROP_GLTF_ExportRoot::ExportAsGLB(const UT_String &path)
{
    UT_OFStream os;
    if (!OpenFileStreamAtPath(path, os))
    {
	return false;
    }

    UT_AutoJSONWriter w(os, false);
    UT_JSONWriter &writer(w.writer());

    UT_String dir;
    UT_String filename;
    path.splitPath(dir, filename);

    // Output buffers which are not in the .bin chunk (index > 0)
    for (uint32 idx = 1; idx < myLoader.getNumBuffers(); idx++)
    {
        UT_ASSERT(myLoader.getBuffer(idx)->myURI != "");
        OutputBuffer(dir, idx);
    }

    ResolveBufferLengths();
    ConvertAbsolutePaths(dir);

    // Header
    const char header[] = {// magic
                           'g', 'l', 'T', 'F',
                           // version
                           0x02, 0x00, 0x00, 0x00,
                           // file length
                           0x00, 0x00, 0x00, 0x00};
    os.write(header, 12);

    // Write JSON header
    const char json_header[] = {// chunk length
                                0x00, 0x00, 0x00, 0x00,
                                // chunk type
                                'J', 'S', 'O', 'N'};
    os.write(json_header, 8);

    const uint32 json_chunk_start = os.tellp();

    // Serialize JSON
    SerializeJSON(writer);

    // Pad JSON payload with spaces to ensure 4-byte alignment
    while (os.tellp() % 4 != 0)
        os.put(' ');

    const uint32 json_chunk_size =
        static_cast<uint32>(os.tellp()) - json_chunk_start;

    // Write DATA header
    const char data_header[] = {// chunk length
                                0x00, 0x00, 0x00, 0x00,
                                // magic
                                'B', 'I', 'N', 0x00};
    os.write(data_header, 8);

    const uint32 data_chunk_start = os.tellp();

    // Write DATA buffer
    OutputGLBBuffer(os);

    // Pad data to satisfy 4-byte alignment requirement
    while (os.tellp() % 4 != 0)
        os.put(0x00);

    const uint32 data_chunk_size =
        static_cast<uint32>(os.tellp()) - data_chunk_start;

    const uint32 total_size = os.tellp();

    // Go back and write in buffer length

    // Output in little endian
    const uint32 total_size_o = total_size;
    UTtovax(total_size_o);

    os.seekp(8);
    os.write(reinterpret_cast<const char *>(&total_size_o), sizeof(total_size_o));

    // Write in JSON chunk length
    // 12 - header size; - 0 offset of chunklength into JSON header
    const uint32 json_chunk_size_o = json_chunk_size;
    UTtovax(total_size_o);

    os.seekp(12 + 0);
    os.write(reinterpret_cast<const char *>(&json_chunk_size_o),
             sizeof(json_chunk_size_o));

    // Write in BIN chunk length
    // 12 - header size;  8 - json header size; 0 offset of chunklength into BIN
    // header
    const uint32 data_chunk_size_o = data_chunk_size;
    UTtovax(data_chunk_size_o);

    os.seekp(12 + 8 + json_chunk_size + 0);
    os.write(reinterpret_cast<const char *>(&data_chunk_size_o),
             sizeof(data_chunk_size_o));

    return true;
}

void
ROP_GLTF_ExportRoot::SerializeJSON(UT_JSONWriter &writer)
{
    writer.jsonBeginMap();

    SerializeAsset(writer);
    SerializeAccessors(writer);
    SerializeBuffers(writer);
    SerializeBufferViews(writer);
    SerializeNodes(writer);
    SerializeMeshes(writer);
    SerializeMaterials(writer);
    SerializeScenes(writer);
    SerializeTextures(writer);
    SerializeImages(writer);

    OutputDefault(writer, "scene", myLoader.getDefaultScene(),
                  GLTF_INVALID_IDX);
    writer.jsonEndMap();
}

void
ROP_GLTF_ExportRoot::ResolveBufferLengths()
{
    for (uint32 idx = 0; idx < myBufferData.size(); idx++)
    {
        GLTF_Buffer &buffer = *myLoader.getBuffer(idx);
        buffer.myByteLength = myBufferData[idx].size();
    }
}

void
ROP_GLTF_ExportRoot::RemoveEmptyBuffers()
{
    uint32 num_buffers = myLoader.getNumBuffers();

    // Create mapping of old buffer offsets to new buffer offsets
    UT_Array<uint32> buffer_map(num_buffers, num_buffers);
    std::fill(buffer_map.begin(), buffer_map.end(), GLTF_INVALID_IDX);

    uint32 cur_buff_idx = 0;
    for (exint idx = 0; idx < myLoader.getNumBuffers(); idx++)
    {
        GLTF_Buffer *buffer = myLoader.getBuffer(idx);
        if (buffer->myByteLength != 0)
        {
            buffer_map[idx] = cur_buff_idx;
            cur_buff_idx++;
        }
    }

    // Reverse and delete the buffers
    for (exint idx = myLoader.getNumBuffers() - 1; idx >= 0; idx--)
    {
        GLTF_Buffer *buffer = myLoader.getBuffer(idx);
        if (buffer->myByteLength == 0)
        {
            myLoader.removeBuffer(idx);
        }
    }

    for (GLTF_BufferView *bv : myLoader.getBufferViews())
    {
        // There should be no zero length bufferviews
        UT_ASSERT(buffer_map[bv->buffer] != GLTF_INVALID_IDX);
        bv->buffer = buffer_map[bv->buffer];
    }
}

void
ROP_GLTF_ExportRoot::ConvertAbsolutePaths(const UT_String &base_path)
{
    for (GLTF_Image *image : myLoader.getImages())
    {
        UT_String image_dir;
        UT_String image_filename;
        image->uri.splitPath(image_dir, image_filename);

        UT_String new_file(base_path);
        new_file += "/";
        new_file += image_filename;
        UT_FileUtil::copyFile(image->uri, new_file);

        image->uri = image_filename;
        image->uri.harden();
    }
}

bool
ROP_GLTF_ExportRoot::OutputBuffer(const char *folder,
                                  const GLTF_Handle idx) const
{
    const GLTF_Buffer &buffer = *myLoader.getBuffer(idx);
    const UT_Array<char> &buffer_data = myBufferData[idx];
    UT_OFStream os;

    if (os.fail())
        return false;

    UT_String absFolder(folder);
    UT_ASSERT(buffer.myURI != "");

    // TODO:  verify that buffer name is valid, this is technically not needed
    // now as buffer names are based off the (valid) file name
    absFolder.append("/");
    absFolder.append(buffer.myURI);
    os.open(absFolder);

    os.write(buffer_data.data(), buffer_data.size());
    os.close();

    return true;
}

void
ROP_GLTF_ExportRoot::OutputGLBBuffer(UT_OFStream &os) const
{
    uint32 idx = 0;
    const UT_Array<char> &buffer_data = myBufferData[idx];

    // GLB buffer is always stored in first slot
    UT_ASSERT(myLoader.getNumBuffers() > 0);
    UT_ASSERT(myLoader.getBuffer(idx)->myURI == "");

    os.write(buffer_data.data(), buffer_data.size());
}

GLTF_Buffer &
ROP_GLTF_ExportRoot::CreateBuffer(GLTF_Handle &idx)
{
    myBufferData.bumpSize(myBufferData.size() + 1);
    return *myLoader.createBuffer(idx);
}

GLTF_Node &
ROP_GLTF_ExportRoot::CreateNode(GLTF_Handle &idx)
{
    return *myLoader.createNode(idx);
}

GLTF_Mesh &
ROP_GLTF_ExportRoot::CreateMesh(GLTF_Handle &idx)
{
    return *myLoader.createMesh(idx);
}

GLTF_Scene &
ROP_GLTF_ExportRoot::CreateScene(GLTF_Handle &idx)
{
    return *myLoader.createScene(idx);
}

GLTF_Image &
ROP_GLTF_ExportRoot::CreateImage(GLTF_Handle &idx)
{
    return *myLoader.createImage(idx);
}

GLTF_Texture &
ROP_GLTF_ExportRoot::CreateTexture(GLTF_Handle &idx)
{
    return *myLoader.createTexture(idx);
}

GLTF_Material &
ROP_GLTF_ExportRoot::CreateMaterial(GLTF_Handle &idx)
{
    return *myLoader.createMaterial(idx);
}

GLTF_BufferView &
ROP_GLTF_ExportRoot::CreateBufferview(GLTF_Handle &idx)
{
    return *myLoader.createBufferView(idx);
}

GLTF_Accessor &
ROP_GLTF_ExportRoot::CreateAccessor(GLTF_Handle &idx)
{
    return *myLoader.createAccessor(idx);
}

void
ROP_GLTF_ExportRoot::SerializeAsset(UT_JSONWriter &writer)
{
    writer.jsonKeyToken("asset");
    writer.jsonBeginMap();

    Output(writer, "version", GLTF_VERSION);
    Output(writer, "generator", GENERATOR_STRING);

    writer.jsonEndMap();
}

void
ROP_GLTF_ExportRoot::SerializeAccessors(UT_JSONWriter &writer)
{
    if (myLoader.getNumAccessors() == 0)
        return;

    writer.jsonKeyToken("accessors");
    writer.jsonBeginArray();

    for (const GLTF_Accessor *accessor : myLoader.getAccessors())
    {
        writer.jsonBeginMap();

        OutputDefault(writer, "bufferView", accessor->bufferView, GLTF_INVALID_IDX);
        OutputDefault(writer, "byteOffset", accessor->byteOffset, 0);
        Output(writer, "componentType", static_cast<int64>(accessor->componentType));
        OutputDefault(writer, "normalized", accessor->normalized, false);
        Output(writer, "count", accessor->count);
        Output(writer, "type", GLTF_Util::typeGetName(accessor->type));

        if (accessor->min.size() > 0)
        {
            writer.jsonKey("min");
            writer.jsonBeginArray();
            for (fpreal64 val : accessor->min)
            {
                if (accessor->componentType ==
                    GLTF_ComponentType::GLTF_COMPONENT_FLOAT)
                {
                    writer.jsonReal(static_cast<fpreal32>(val));
                }
                else
                {
                    writer.jsonInt(static_cast<int64>(val));
                }
            }
            writer.jsonEndArray();
        }

        if (accessor->max.size() > 0)
        {
            writer.jsonKey("max");
            writer.jsonBeginArray();
            for (fpreal64 val : accessor->max)
            {
                if (accessor->componentType ==
                    GLTF_ComponentType::GLTF_COMPONENT_FLOAT)
                {
                    writer.jsonReal(static_cast<fpreal32>(val));
                }
                else
                {
                    writer.jsonInt(static_cast<int64>(val));
                }
            }
            writer.jsonEndArray();
        }

        writer.jsonEndMap();
    }

    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializeBuffers(UT_JSONWriter &writer)
{
    if (myLoader.getNumBuffers() == 0)
        return;

    writer.jsonKeyToken("buffers");
    writer.jsonBeginArray();

    for (const GLTF_Buffer *buffer : myLoader.getBuffers())
    {
        writer.jsonBeginMap();

        OutputDefault(writer, "uri", buffer->myURI);
        Output(writer, "byteLength", buffer->myByteLength);
        OutputName(writer, "name", buffer->name);

        writer.jsonEndMap();
    }

    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializeBufferViews(UT_JSONWriter &writer)
{
    if (myLoader.getNumBuffers() == 0)
        return;

    writer.jsonKeyToken("bufferViews");
    writer.jsonBeginArray();

    for (const GLTF_BufferView *bufferView : myLoader.getBufferViews())
    {
        writer.jsonBeginMap();

        Output(writer, "buffer", bufferView->buffer);
        OutputDefault(writer, "byteOffset", bufferView->byteOffset, 0);
        Output(writer, "byteLength", bufferView->byteLength);
        OutputDefault(writer, "byteStride", bufferView->byteStride, 0);
        OutputDefault(writer, "target", static_cast<int64>(bufferView->target),
                      GLTF_BufferViewTarget::GLTF_BUFFER_INVALID);
        OutputName(writer, "name", bufferView->name);

        writer.jsonEndMap();
    }

    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializeNodes(UT_JSONWriter &writer)
{
    if (myLoader.getNumNodes() == 0)
        return;

    writer.jsonKeyToken("nodes");
    writer.jsonBeginArray();

    for (const GLTF_Node *node : myLoader.getNodes())
    {
        writer.jsonBeginMap();

        // Children
        if (node->children.size() > 0)
        {
            writer.jsonKey("children");
            writer.jsonBeginArray();
            for (uint32 child : node->children)
            {
                writer.jsonInt(static_cast<int64>(child));
            }
            writer.jsonEndArray();
        }

        // Matrix transform
        if (node->matrix != UT_Matrix4::getIdentityMatrix())
        {
            writer.jsonKey("matrix");
            writer.jsonBeginArray();
            for (uint32 r = 0; r < 4; r++)
            {
                for (uint32 c = 0; c < 4; c++)
                {
                    writer.jsonReal(node->matrix[r][c]);
                }
            }
            writer.jsonEndArray();
        }

        OutputName(writer, "name", node->name);
        OutputDefault(writer, "mesh", node->mesh, GLTF_INVALID_IDX);

        writer.jsonEndMap();
    }

    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializeMeshes(UT_JSONWriter &writer)
{
    if (myLoader.getNumMeshes() == 0)
        return;

    writer.jsonKeyToken("meshes");
    writer.jsonBeginArray();

    for (const GLTF_Mesh *mesh : myLoader.getMeshes())
    {
        writer.jsonBeginMap();

        UT_ASSERT(mesh->primitives.size() > 0);

        writer.jsonKey("primitives");
        SerializePrimitives(writer, mesh->primitives);

        OutputName(writer, "name", mesh->name);

        writer.jsonEndMap();
    }

    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializeMaterials(UT_JSONWriter &writer)
{
    if (myLoader.getNumMaterials() == 0)
        return;

    writer.jsonKey("materials");
    writer.jsonBeginArray();

    for (const GLTF_Material *material : myLoader.getMaterials())
    {
        writer.jsonBeginMap();

        OutputName(writer, "name", material->name);

        if (material->metallicRoughness)
        {
            writer.jsonKey("pbrMetallicRoughness");
            writer.jsonBeginMap();
            const GLTF_PBRMetallicRoughness &metallic_roughness =
                *material->metallicRoughness;
            if (metallic_roughness.baseColorFactor != UT_Vector4(1, 1, 1, 1))
            {
                writer.jsonKey("baseColorFactor");
                writer.jsonBeginArray();
                for (uint32 idx = 0; idx < 4; idx++)
                {
                    writer.jsonReal(metallic_roughness.baseColorFactor[idx]);
                }
                writer.jsonEndArray();
            }
            OutputDefault(writer, "metallicFactor",
                          metallic_roughness.metallicFactor, 1.f);
            OutputDefault(writer, "roughnessFactor",
                          metallic_roughness.roughnessFactor, 1.f);

            if (metallic_roughness.baseColorTexture)
            {
                const GLTF_TextureInfo &base_color_texture =
                    *metallic_roughness.baseColorTexture;
                writer.jsonKey("baseColorTexture");
                writer.jsonBeginMap();
                Output(writer, "index", base_color_texture.index);
                OutputDefault(writer, "texCoord", base_color_texture.texCoord, 0);
                writer.jsonEndMap();
            }

            if (metallic_roughness.metallicRoughnessTexture)
            {
                const GLTF_TextureInfo &mr_tex =
                    *metallic_roughness.metallicRoughnessTexture;
                writer.jsonKey("metallicRoughnessTexture");
                writer.jsonBeginMap();
                Output(writer, "index", mr_tex.index);
                OutputDefault(writer, "texCoord", mr_tex.texCoord, 0);
                writer.jsonEndMap();
            }
            writer.jsonEndMap();
        }

        if (material->normalTexture)
        {
            const GLTF_NormalTextureInfo &normal_tex = *material->normalTexture;
            writer.jsonKey("normalTexture");
            writer.jsonBeginMap();
            OutputDefault(writer, "index", normal_tex.index, GLTF_INVALID_IDX);
            OutputDefault(writer, "scale", normal_tex.scale, 1.f);
            OutputDefault(writer, "texCoord", normal_tex.texCoord, 0);
            writer.jsonEndMap();
        }

        if (material->emissiveTexture)
        {
            const GLTF_TextureInfo &emissive_tex = *material->emissiveTexture;
            writer.jsonKey("emissiveTexture");
            writer.jsonBeginMap();
            OutputDefault(writer, "index", emissive_tex.index,
                          GLTF_INVALID_IDX);
            OutputDefault(writer, "texCoord", emissive_tex.texCoord, 0);
            writer.jsonEndMap();
        }

		if (material->alphaMode)
		{
			OutputDefault(writer, "alphaMode", material->alphaMode);
			if (material->alphaMode == "MASK")
			{
				OutputDefault(writer, "alphaCutoff", material->alphaCutoff, 0);
			}
		}

        OutputDefault(writer, "emissiveFactor", material->emissiveFactor);

        writer.jsonEndMap();
    }

    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializePrimitives(
    UT_JSONWriter &writer, const UT_Array<GLTF_NAMESPACE::GLTF_Primitive> &primitives)
{
    writer.jsonBeginArray();
    for (const GLTF_Primitive &primitive : primitives)
    {
        writer.jsonBeginMap();

        writer.jsonKey("attributes");
        writer.jsonBeginMap();
        UT_ASSERT(primitive.attributes.size() > 0);
        for (const auto attrib : primitive.attributes)
        {
            Output(writer, attrib.first, attrib.second);
        }
        writer.jsonEndMap();

        OutputDefault(writer, "indices", primitive.indices, GLTF_INVALID_IDX);
        OutputDefault(writer, "material", primitive.material, GLTF_INVALID_IDX);
        OutputDefault(writer, "mode", static_cast<int64>(primitive.mode),
                      GLTF_RenderMode::GLTF_RENDERMODE_TRIANGLES);

        writer.jsonEndMap();
    }
    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializeTextures(UT_JSONWriter &writer)
{
    if (myLoader.getNumTextures() == 0)
        return;

    writer.jsonKeyToken("textures");
    writer.jsonBeginArray();
    for (const GLTF_Texture *texture : myLoader.getTextures())
    {
        writer.jsonBeginMap();

        OutputDefault(writer, "sampler", texture->sampler, GLTF_INVALID_IDX);
        OutputDefault(writer, "source", texture->source, GLTF_INVALID_IDX);
        OutputName(writer, "name", texture->name);

        writer.jsonEndMap();
    }
    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializeImages(UT_JSONWriter &writer)
{
    if (myLoader.getNumImages() == 0)
        return;

    writer.jsonKeyToken("images");
    writer.jsonBeginArray();
    for (const GLTF_Image *image : myLoader.getImages())
    {
        writer.jsonBeginMap();

        OutputDefault(writer, "uri", image->uri);
        OutputDefault(writer, "mimeType", image->mimeType);
        OutputDefault(writer, "bufferView", image->bufferView,
                      GLTF_INVALID_IDX);
        OutputName(writer, "name", image->name);

        writer.jsonEndMap();
    }
    writer.jsonEndArray();
}

void
ROP_GLTF_ExportRoot::SerializeScenes(UT_JSONWriter &writer)
{
    if (myLoader.getNumScenes() == 0)
        return;

    writer.jsonKeyToken("scenes");
    writer.jsonBeginArray();

    for (const GLTF_Scene *scene : myLoader.getScenes())
    {
        writer.jsonBeginMap();

        if (scene->nodes.size() > 0)
        {
            writer.jsonKey("nodes");
            writer.jsonBeginArray();
            for (uint32 node : scene->nodes)
            {
                writer.jsonInt(static_cast<int64>(node));
            }
            writer.jsonEndArray();
        }

        OutputName(writer, "name", scene->name);

        writer.jsonEndMap();
    }

    writer.jsonEndArray();
}

GLTF_Accessor *
ROP_GLTF_ExportRoot::getAccessor(GLTF_Handle idx)
{
    return myLoader.getAccessor(idx);
}

GLTF_Animation *
ROP_GLTF_ExportRoot::getAnimation(GLTF_Handle idx)
{
    return myLoader.getAnimation(idx);
}

GLTF_Asset
ROP_GLTF_ExportRoot::getAsset()
{
    return myLoader.getAsset();
}

GLTF_Buffer *
ROP_GLTF_ExportRoot::getBuffer(GLTF_Handle idx)
{
    return myLoader.getBuffer(idx);
}

GLTF_BufferView *
ROP_GLTF_ExportRoot::getBufferView(GLTF_Handle idx)
{
    return myLoader.getBufferView(idx);
}

GLTF_Camera *
ROP_GLTF_ExportRoot::getCamera(GLTF_Handle idx)
{
    return myLoader.getCamera(idx);
}

GLTF_Image *
ROP_GLTF_ExportRoot::getImage(GLTF_Handle idx)
{
    return myLoader.getImage(idx);
}

GLTF_Material *
ROP_GLTF_ExportRoot::getMaterial(GLTF_Handle idx)
{
    return myLoader.getMaterial(idx);
}

GLTF_Mesh *
ROP_GLTF_ExportRoot::getMesh(GLTF_Handle idx)
{
    return myLoader.getMesh(idx);
}

GLTF_Node *
ROP_GLTF_ExportRoot::getNode(GLTF_Handle idx)
{
    return myLoader.getNode(idx);
}

GLTF_Sampler *
ROP_GLTF_ExportRoot::getSampler(GLTF_Handle idx)
{
    return myLoader.getSampler(idx);
}

GLTF_Handle
ROP_GLTF_ExportRoot::getDefaultScene()
{
    return myLoader.getDefaultScene();
}

GLTF_Scene *
ROP_GLTF_ExportRoot::getScene(GLTF_Handle idx)
{
    return myLoader.getScene(idx);
}

GLTF_Skin *
ROP_GLTF_ExportRoot::getSkin(GLTF_Handle idx)
{
    return myLoader.getSkin(idx);
}

GLTF_Texture *
ROP_GLTF_ExportRoot::getTexture(GLTF_Handle idx)
{
    return myLoader.getTexture(idx);
}

const UT_Array<GLTF_Accessor *> &
ROP_GLTF_ExportRoot::getAccessors() const
{
    return myLoader.getAccessors();
}

const UT_Array<GLTF_Animation *> &
ROP_GLTF_ExportRoot::getAnimations() const
{
    return myLoader.getAnimations();
}

const UT_Array<GLTF_Buffer *> &
ROP_GLTF_ExportRoot::getBuffers() const
{
    return myLoader.getBuffers();
}

const UT_Array<GLTF_BufferView *> &
ROP_GLTF_ExportRoot::getBufferViews() const
{
    return myLoader.getBufferViews();
}

const UT_Array<GLTF_Camera *> &
ROP_GLTF_ExportRoot::getCameras() const
{
    return myLoader.getCameras();
}

const UT_Array<GLTF_Image *> &
ROP_GLTF_ExportRoot::getImages() const
{
    return myLoader.getImages();
}

const UT_Array<GLTF_Material *> &
ROP_GLTF_ExportRoot::getMaterials() const
{
    return myLoader.getMaterials();
}

const UT_Array<GLTF_Mesh *> &
ROP_GLTF_ExportRoot::getMeshes() const
{
    return myLoader.getMeshes();
}

const UT_Array<GLTF_Node *> &
ROP_GLTF_ExportRoot::getNodes() const
{
    return myLoader.getNodes();
}

const UT_Array<GLTF_Sampler *> &
ROP_GLTF_ExportRoot::getSamplers() const
{
    return myLoader.getSamplers();
}

const UT_Array<GLTF_Scene *> &
ROP_GLTF_ExportRoot::getScenes() const
{
    return myLoader.getScenes();
}

const UT_Array<GLTF_Skin *> &
ROP_GLTF_ExportRoot::getSkins() const
{
    return myLoader.getSkins();
}

const UT_Array<GLTF_Texture *> &
ROP_GLTF_ExportRoot::getTextures() const
{
    return myLoader.getTextures();
}

void
ROP_GLTF_ExportRoot::SetDefaultScene(GLTF_Handle idx)
{
    myLoader.setDefaultScene(idx);
}
