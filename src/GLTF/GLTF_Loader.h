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

#ifndef __SOP_GLTFLOADER_H__
#define __SOP_GLTFLOADER_H__

#include "GLTF_API.h"
#include "GLTF_Types.h"

#include <UT/UT_Array.h>
#include <UT/UT_String.h>
#include <UT/UT_Lock.h>

class UT_JSONValue;
class UT_JSONValueMap;
class UT_JSONValueArray;

namespace GLTF_NAMESPACE
{

//=================================================

///
/// A class for loading a GLTF file into a more usuable structure.
///
class GLTF_API GLTF_Loader
{
public:
    GLTF_Loader();
    GLTF_Loader(UT_String filename);
    virtual ~GLTF_Loader();

    // Delete copy constructor
    GLTF_Loader(GLTF_Loader &loader) = delete;
    GLTF_Loader(const GLTF_Loader &loader) = delete;

    ///
    /// Loads and parses the JSON data within this GLTF file.
    /// Does not load any associated buffer data.
    /// @return Whether or not the load suceeded
    ///
    bool Load();

    ///
    /// Loads all data that can be accessed with the given accessor and returns
    /// a pointer to the beginning of the data.  The caller is not responsible
    /// for deleting the returned data.
    /// @return Whether or not the accessor data load suceeded
    ///
    bool LoadAccessorData(const GLTF_Accessor &accessor, unsigned char *&data) const;

    GLTF_Accessor *createAccessor(GLTF_Handle& idx);
    GLTF_Animation *createAnimation(GLTF_Handle& idx);
    GLTF_Buffer *createBuffer(GLTF_Handle& idx);
    GLTF_BufferView *createBufferView(GLTF_Handle& idx);
    GLTF_Camera *createCamera(GLTF_Handle& idx);
    GLTF_Image *createImage(GLTF_Handle& idx);
    GLTF_Material *createMaterial(GLTF_Handle& idx);
    GLTF_Mesh *createMesh(GLTF_Handle& idx);
    GLTF_Node *createNode(GLTF_Handle& idx);
    GLTF_Sampler *createSampler(GLTF_Handle& idx);
    GLTF_Scene *createScene(GLTF_Handle& idx);
    GLTF_Skin *createSkin(GLTF_Handle& idx);
    GLTF_Texture *createTexture(GLTF_Handle& idx);

    GLTF_Accessor const *getAccessor(GLTF_Handle idx) const;
    GLTF_Animation const *getAnimation(GLTF_Handle idx) const;
    GLTF_Asset getAsset() const;
    GLTF_Buffer const *getBuffer(GLTF_Handle idx) const;
    GLTF_BufferView const *getBufferView(GLTF_Handle idx) const;
    GLTF_Camera const *getCamera(GLTF_Handle idx) const;
    GLTF_Image const *getImage(GLTF_Handle idx) const;
    GLTF_Material const *getMaterial(GLTF_Handle idx) const;
    GLTF_Mesh const *getMesh(GLTF_Handle idx) const;
    GLTF_Node const *getNode(GLTF_Handle idx) const;
    GLTF_Sampler const *getSampler(GLTF_Handle idx) const;
    GLTF_Handle getDefaultScene() const;
    GLTF_Scene const *getScene(GLTF_Handle idx) const;
    GLTF_Skin const *getSkin(GLTF_Handle idx) const;
    GLTF_Texture const *getTexture(GLTF_Handle idx) const;

    GLTF_Accessor *getAccessor(GLTF_Handle idx);
    GLTF_Animation *getAnimation(GLTF_Handle idx);
    GLTF_Buffer *getBuffer(GLTF_Handle idx);
    GLTF_BufferView *getBufferView(GLTF_Handle idx);
    GLTF_Camera *getCamera(GLTF_Handle idx);
    GLTF_Image *getImage(GLTF_Handle idx);
    GLTF_Material *getMaterial(GLTF_Handle idx);
    GLTF_Mesh *getMesh(GLTF_Handle idx);
    GLTF_Node *getNode(GLTF_Handle idx);
    GLTF_Sampler *getSampler(GLTF_Handle idx);
    GLTF_Scene *getScene(GLTF_Handle idx);
    GLTF_Skin *getSkin(GLTF_Handle idx);
    GLTF_Texture *getTexture(GLTF_Handle idx);

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

    void removeBuffer(GLTF_Handle idx);
    void removeNode(GLTF_Handle idx);

    void setDefaultScene(const GLTF_Handle &idx);
    void setAsset(const GLTF_Asset &asset);

    exint getNumAccessors() const;
    exint getNumAnimations() const;
    exint getNumBuffers() const;
    exint getNumBufferViews() const;
    exint getNumCameras() const;
    exint getNumImages() const;
    exint getNumMaterials() const;
    exint getNumMeshes() const;
    exint getNumNodes() const;
    exint getNumSamplers() const;
    exint getNumScenes() const;
    exint getNumSkins() const;
    exint getNumTextures() const;

private:
    // Handles reading the JSON map, which may be external or
    // embedded in a GLB file
    bool ReadJSON(const UT_JSONValueMap &root_json);

    bool ReadGLTF();
    bool ReadGLB();

    bool ReadAsset(const UT_JSONValueMap &asset_json);

    // Read items from GLTF JSON -> GLTF struct
    bool ReadNode(const UT_JSONValueMap &node_json, const exint idx);
    bool ReadBuffer(const UT_JSONValueMap &buffer_json, const exint idx);
    bool ReadBufferView(const UT_JSONValueMap &bufferview_json, const exint idx);
    bool ReadAccessor(const UT_JSONValueMap &accessor_json, const exint idx);
    bool ReadPrimitive(const UT_JSONValue *primitive_json, GLTF_Primitive *primitive);
    bool ReadMesh(const UT_JSONValueMap &mesh_json, const exint idx);
    bool ReadTexture(const UT_JSONValueMap &texture_json, const exint idx);
    bool ReadSampler(const UT_JSONValueMap &sampler_json, const exint idx);
    bool ReadImage(const UT_JSONValueMap &image_json, const exint idx);
    bool ReadScene(const UT_JSONValueMap &scene_json, const exint idx);
    bool ReadMaterial(const UT_JSONValueMap &material_json, const exint idx);

    // Utility:  Takes an array of maps, and calls funcs() on every item
    bool ReadArrayOfMaps(const UT_JSONValue *arr, bool required,
                         std::function<bool(const UT_JSONValueMap &, const exint idx)> func);

    UT_String myFilename;
    UT_String myBasePath;

    bool myIsLoaded;

    // Retrieves the buffer at idx, potentially from cache if cached.
    bool LoadBuffer(uint32 idx, unsigned char *&buffer_data) const;

    // Simply an indexed array of pointers to buffer data
    UT_Array<GLTF_Accessor *> myAccesors;
    UT_Array<GLTF_Animation *> myAnimations;
    GLTF_Asset myAsset;
    UT_Array<GLTF_Buffer *> myBuffers;
    UT_Array<GLTF_BufferView *> myBufferViews;
    UT_Array<GLTF_Camera *> myCameras;
    UT_Array<GLTF_Image *> myImages;
    UT_Array<GLTF_Material *> myMaterials;
    UT_Array<GLTF_Mesh *> myMeshes;
    UT_Array<GLTF_Node *> myNodes;
    UT_Array<GLTF_Sampler *> mySamplers;
    uint32 myScene = GLTF_INVALID_IDX;
    UT_Array<GLTF_Scene *> myScenes;
    UT_Array<GLTF_Skin *> mySkins;
    UT_Array<GLTF_Texture *> myTextures;

    // Loading is transparent to the caller -- we want const 
    // semantics that 
    mutable UT_Lock myAccessorLock;
    mutable UT_Array<unsigned char *> myBufferCache;
};

//=================================================

} // end GLTF_NAMESPACE

#endif
