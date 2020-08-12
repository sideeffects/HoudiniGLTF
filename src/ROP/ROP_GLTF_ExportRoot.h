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

#ifndef __ROP_GLTF_EXPORTROOT_h__
#define __ROP_GLTF_EXPORTROOT_h__

#include "ROP_GLTF_Image.h"
#include <GLTF/GLTF_Types.h>
#include <UT/UT_Array.h>
#include <UT/UT_ArraySet.h>
#include <UT/UT_WorkBuffer.h>

#include <GLTF/GLTF_Loader.h>

#include <vector>

constexpr const char *GENERATOR_STRING = "Houdini GLTF 2.0 Exporter";
constexpr const char *GLTF_VERSION = "2.0";

class UT_WorkBuffer;
class UT_JSONWriter;
class UT_OFStream;
class OP_Node;
struct ROP_GLTF_ChannelMapping;

typedef GLTF_NAMESPACE::GLTF_Accessor           GLTF_Accessor;
typedef GLTF_NAMESPACE::GLTF_Animation          GLTF_Animation;
typedef GLTF_NAMESPACE::GLTF_Asset              GLTF_Asset;
typedef GLTF_NAMESPACE::GLTF_Buffer             GLTF_Buffer;
typedef GLTF_NAMESPACE::GLTF_BufferView         GLTF_BufferView;
typedef GLTF_NAMESPACE::GLTF_Camera             GLTF_Camera;
typedef GLTF_NAMESPACE::GLTF_Image              GLTF_Image;
typedef GLTF_NAMESPACE::GLTF_Material           GLTF_Material;
typedef GLTF_NAMESPACE::GLTF_Mesh               GLTF_Mesh;
typedef GLTF_NAMESPACE::GLTF_Node               GLTF_Node;
typedef GLTF_NAMESPACE::GLTF_Scene              GLTF_Scene;
typedef GLTF_NAMESPACE::GLTF_Sampler            GLTF_Sampler;
typedef GLTF_NAMESPACE::GLTF_Skin               GLTF_Skin;
typedef GLTF_NAMESPACE::GLTF_Texture            GLTF_Texture;
typedef GLTF_NAMESPACE::GLTF_Material           GLTF_Material;

typedef GLTF_NAMESPACE::GLTF_Int        GLTF_Int;
typedef GLTF_NAMESPACE::GLTF_Offset     GLTF_Offset;
typedef GLTF_NAMESPACE::GLTF_Handle     GLTF_Handle;


class ROP_GLTF_ExportRoot
{
public:
    struct ExportSettings
    {
        bool exportNames = false;
    };

    ROP_GLTF_ExportRoot(ExportSettings s);
    ~ROP_GLTF_ExportRoot();

    bool HasCachedChannelImage(
        const UT_Array<ROP_GLTF_ChannelMapping> &mapping) const;
    GLTF_Handle
    GetCachedChannelImage(const UT_Array<ROP_GLTF_ChannelMapping> &mapping);
    void
    InsertCachedChannelImage(const UT_Array<ROP_GLTF_ChannelMapping> &mapping,
                             GLTF_Handle idx);

    UT_Map<UT_StringHolder, GLTF_Handle> &GetImageCache();
    UT_Map<const OP_Node *, GLTF_Handle> &GetMaterialCache();

    /// This keeps track of the amount of times a specific filename
    /// outputted to to avoid name collisions
    UT_Map<UT_StringHolder, GLTF_Int> &GetNameUsagesMap();

    ///
    /// Allocates additional space in the buffer in index bid.
    /// TODO:  stop allocating from reallocing space.
    ///
    void *BufferAlloc(GLTF_Handle bid, GLTF_Offset bytes, GLTF_Offset alignment,
                      GLTF_Offset &offset);

    ///
    /// Returns a reference to the internal root GLTF object
    ///
    GLTF_NAMESPACE::GLTF_Loader &loader();

    ///
    /// Exports this structure as a GLTF file.
    ///
    bool ExportGLTF(const UT_String &path);

    ///
    /// Exports the file as GLB on os.  If a uri is defined on buffer 0,
    /// it will be ignored.  Otherwise, there are no modifications to the
    /// outputted JSON.
    ///
    bool ExportAsGLB(const UT_String &path);
    void SerializeJSON(UT_JSONWriter &writer);

    //
    // Convenience functions:  Will return the index of the created
    // node in the parameter idx.
    //
    GLTF_Accessor &CreateAccessor(GLTF_Handle &idx);
    GLTF_Buffer &CreateBuffer(GLTF_Handle &idx);
    GLTF_BufferView &CreateBufferview(GLTF_Handle &idx);
    GLTF_Node &CreateNode(GLTF_Handle &idx);
    GLTF_Mesh &CreateMesh(GLTF_Handle &idx);
    GLTF_Scene &CreateScene(GLTF_Handle &idx);
    GLTF_Image &CreateImage(GLTF_Handle &idx);
    GLTF_Texture &CreateTexture(GLTF_Handle &idx);
    GLTF_Material &CreateMaterial(GLTF_Handle &idx);

    GLTF_Accessor *getAccessor(GLTF_Handle idx);
    GLTF_Animation *getAnimation(GLTF_Handle idx);
    GLTF_Asset getAsset();
    GLTF_Buffer *getBuffer(GLTF_Handle idx);
    GLTF_BufferView *getBufferView(GLTF_Handle idx);
    GLTF_Camera *getCamera(GLTF_Handle idx);
    GLTF_Image *getImage(GLTF_Handle idx);
    GLTF_Material *getMaterial(GLTF_Handle idx);
    GLTF_Mesh *getMesh(GLTF_Handle idx);
    GLTF_Node *getNode(GLTF_Handle idx);
    GLTF_Sampler *getSampler(GLTF_Handle idx);
    GLTF_Handle getDefaultScene();
    GLTF_Scene *getScene(GLTF_Handle idx);
    GLTF_Skin *getSkin(GLTF_Handle idx);
    GLTF_Texture *getTexture(GLTF_Handle idx);

    void SetDefaultScene(GLTF_Handle idx);


    const UT_Array<GLTF_Accessor *> &getAccessors() const;
    const UT_Array<GLTF_Animation *> &getAnimations() const;
    const UT_Array<GLTF_Buffer *> &getBuffers() const;
    const UT_Array<GLTF_BufferView *> &getBufferViews() const;
    const UT_Array<GLTF_Camera *> &getCameras() const;
    const UT_Array<GLTF_Image *> &getImages() const;
    const UT_Array<GLTF_Material *> &getMaterials() const;
    const UT_Array<GLTF_Mesh *> &getMeshes() const;
    const UT_Array<GLTF_Node *> &getNodes() const;
    const UT_Array<GLTF_Sampler *> &getSamplers() const;
    const UT_Array<GLTF_Scene *> &getScenes() const;
    const UT_Array<GLTF_Skin *> &getSkins() const;
    const UT_Array<GLTF_Texture *> &getTextures() const;

private:
    void
    OutputName(UT_JSONWriter &writer, const char *string, const char *value);

    void SerializeAsset(UT_JSONWriter &writer);
    void SerializeAccessors(UT_JSONWriter &writer);
    void SerializeBuffers(UT_JSONWriter &writer);
    void SerializeBufferViews(UT_JSONWriter &writer);
    void SerializeNodes(UT_JSONWriter &writer);
    void SerializeMeshes(UT_JSONWriter &writer);
    void SerializeMaterials(UT_JSONWriter &writer);
    void SerializePrimitives(UT_JSONWriter &writer,
                             const UT_Array<GLTF_NAMESPACE::GLTF_Primitive> &primitive);
    void SerializeTextures(UT_JSONWriter &writer);
    void SerializeImages(UT_JSONWriter &writer);
    void SerializeScenes(UT_JSONWriter &writer);

    bool OutputBuffer(const char *folder, GLTF_Handle buffer) const;
    void OutputGLBBuffer(UT_OFStream &stream) const;

    // Pre-output pass:  these should be used only before outputting as they
    // may potentially invalidate handles
    void ResolveBufferLengths();
    void RemoveEmptyBuffers();

    // Any external files must be copied to the output folder as
    // GLTF is only required to support "http://" and relative URI
    void ConvertAbsolutePaths(const UT_String &base_path);

    // Returns true if opened output stream to given path.
    // Automatically creates directories in path.
    bool OpenFileStreamAtPath(const UT_String &path, UT_OFStream &os);

    UT_Array<UT_Array<char>> myBufferData;

    UT_Map<UT_StringHolder, GLTF_Handle> myNameUsagesMap;
    UT_Map<UT_StringHolder, GLTF_Handle> myImageMap;
    UT_Map<const OP_Node *, GLTF_Handle> myMaterialMap;

    std::map<std::vector<ROP_GLTF_ChannelMapping>, GLTF_Handle>
        myChannelImageMap;

    GLTF_NAMESPACE::GLTF_Loader myLoader;
    ExportSettings mySettings;
};

#endif
