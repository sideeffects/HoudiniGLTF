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

#ifndef __ROP_GLTF_h__
#define __ROP_GLTF_h__

#include <GLTF/GLTF_Types.h>
#include <ROP/ROP_Node.h>
#include <UT/UT_UniquePtr.h>

using GLTF_NAMESPACE::GLTF_Node;
using GLTF_NAMESPACE::GLTF_TextureInfo;
using GLTF_NAMESPACE::GLTF_Scene;

typedef GLTF_NAMESPACE::GLTF_Handle     GLTF_Handle;

struct ROP_GLTF_ChannelMapping;
struct ROP_GLTF_ImgExportParms;
class ROP_GLTF_ExportRoot;
class ROP_GLTF_ErrorManager;
class OBJ_Geometry;

class ROP_GLTF_BaseErrorManager
{
public:
    virtual ~ROP_GLTF_BaseErrorManager() = default;
    virtual void AddError(int code, const char *msg = 0) const = 0;
    virtual void AddWarning(int code, const char *msg = 0) const = 0;
};

struct ROP_GLTF_TextureParms
{
public:
    bool myFlipGreenChannel = false;
};

class ROP_GLTF : public ROP_Node
{
public:
    static OP_Node *
    myConstructor(OP_Network *net, const char *name, OP_Operator *op);

    ///
    /// Simply wraps ROP_GLTF so that the error manager on Node
    /// can be used in external classes.
    ///
    class ROP_GLTF_ErrorManager : public ROP_GLTF_BaseErrorManager
    {
    public:
        ROP_GLTF_ErrorManager(ROP_GLTF &gltf);
        ROP_GLTF_ErrorManager(ROP_GLTF_ErrorManager &gltf) = delete;
        ROP_GLTF_ErrorManager(ROP_GLTF_ErrorManager &&gltf) = delete;
        virtual ~ROP_GLTF_ErrorManager() = default;
        virtual void AddError(int code, const char *msg = 0) const;
        virtual void AddWarning(int code, const char *msg = 0) const;

    private:
        ROP_GLTF &myNode;
    };

protected:
    ROP_GLTF(OP_Network *net, const char *name, OP_Operator *entry);
    virtual bool updateParmsFlags();
    virtual ~ROP_GLTF();

    // From ROP_Node
    virtual int startRender(int nframes, fpreal s, fpreal e);
    virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss);
    virtual ROP_RENDER_CODE endRender();

    void OUTPUT_FILE(UT_String &str, fpreal t) const
    {
        evalString(str, "file", 0, t);
    }
    void OBJPATH(UT_String &str, fpreal t) const
    {
        evalString(str, "objpath", 0, t);
    }
    void OBJECTS(UT_String &str, fpreal time) const
    {
        evalString(str, "objects", 0, time);
    }
    bool EXPORT_MATERIALS(fpreal time) const
    {
        return evalInt("exportmaterials", 0, time) != 0;
    }
    bool EXPORT_CUSTOM_ATTRIBS(fpreal time) const
    {
        return evalInt("customattribs", 0, time) != 0;
    }
    bool EXPORT_NAMES(fpreal time) const
    {
        return evalInt("exportnames", 0, time) != 0;
    }
    void IMAGEFORMAT(UT_String &str, fpreal time) const
    {
        evalString(str, "imageformat", 0, time);
    }
    void MAXRES(UT_String &str, fpreal time) const
    {
        evalString(str, "maxresolution", 0, time);
    }
    void EXPORTTYPE(UT_String &str, fpreal time) const
    {
        evalString(str, "exporttype", 0, time);
    }
    bool CULL_EMPTY_NODES(fpreal time) const
    {
        return evalInt("cullempty", 0, time) != 0;
    }
    bool POWER_OF_TWO(fpreal time) const
    {
        return evalInt("poweroftwo", 0, time) != 0;
    }
    int IMAGE_QUALITY(fpreal time) const
    {
        return evalInt("imagequality", 0, time);
    }
    bool SAVE_HIDDEN(fpreal time) const
    {
        return evalInt("savehidden", 0, time) != 0;
    }
    bool USE_SOP_PATH(fpreal time) const
    {
        return evalInt("usesoppath", 0, time) != 0;
    }
    void SOP_PATH(UT_String &str, fpreal time) const
    {
        evalString(str, "soppath", 0, time);
    }
    bool FLIP_Y_NORMALS(fpreal time) const
    {
        return evalInt("flipnormalmapy", 0, time) != 0;
    }

private:
    class GLTF_HierarchyBuilder
    {
    public:
        GLTF_HierarchyBuilder(
            OP_Node *root_node, GLTF_Node *root_gltf,
            ROP_GLTF_ExportRoot &export_root,
            std::function<void(GLTF_Node &, OBJ_Node *, fpreal)> proc_func);

        uint32 Traverse(OBJ_Node *node, fpreal time);

    private:
        const OP_Node *myRootNode;
        ROP_GLTF_ExportRoot &myRootExporter;
        GLTF_Node *myRootGLTF;
        UT_Map<OP_Node *, uint32> myNodeMap;
        const std::function<void(GLTF_Node &, OBJ_Node *, fpreal)> myFunc;
    };

    const IMG_Format *GetImageFormat(fpreal time) const;
    const char *GetImageMimeType(fpreal time) const;

    // Takes a glTF node and a Houdini node, and assigns the transformation
    // of the Houdini node to the glTF node.
    void AssignGLTFTransform(GLTF_Node &gltf_node, OBJ_Node *node,
                             fpreal time) const;
    void
    AssignGLTFName(GLTF_Node &gltf_node, OBJ_Node *node, fpreal time) const;

    // Creates a GLTF_Mesh mesh from *node and assigns it to the glTF node.
    // If the sop is not null, then it will pull geometry from *sop.  Otherwise
    // it will pull geometry from the current node being rendered.
    void SetupGLTFMesh(GLTF_Node &gltf_node, OBJ_Node *node, fpreal time,
                       SOP_Node *sop = nullptr);

    // Translation of Houdini principled shader parameters -> GLTF material 
    // Parameters
    uint32 TranslatePrincipledShader(const OP_Context &context,
                                     const OP_Node *ps_node);

    // As we are pulling from multiple directories and outputting
    // the images in a single directory, there is a possiblity of
    // name collisions.  This numbers files with the same name
    // in an arbitrary order.
    void GetNonCollidingName(const UT_String &s, UT_String &o);

    SOP_Node *GetSOPNode(fpreal time) const;
    bool hasSOPInput(fpreal time) const;

    // For handling textures with input from only a single channel
    bool
    TranslateTexture(const UT_String &path, const OP_Context &context,
                        GLTF_TextureInfo &tex_info,
                        const ROP_GLTF_TextureParms &tex_parms = {});

    // For handling textures with input from multiple channels
    bool
    TranslateTexture(const UT_Array<ROP_GLTF_ChannelMapping> &mappings,
                     const OP_Context &context, GLTF_TextureInfo &tex_info,
                     const ROP_GLTF_TextureParms &tex_parms = {});

    uint32
    OutputTexture(const UT_String &output_path, const ROP_GLTF_TextureParms &parms,
                  std::function<bool(std::ostream &, const IMG_Format *,
                  const ROP_GLTF_ImgExportParms &)> output_function,
                  GLTF_TextureInfo &tex_info, const OP_Context &context);

    GLTF_Scene &InitializeBasicGLTFScene(GLTF_Handle &root_scene_idx);

    bool IsExportingGLB() const;
    const UT_String &GetBasePath() const;

    bool WriteTreeToDisk(fpreal time);
    void InitializeGLTFTree(fpreal time);

    bool BuildGLTFTree(fpreal time);
    bool BuildFromSOP(fpreal time, SOP_Node *sop, OBJ_Node *geo);
    bool BuildGLTFTreeFromHierarchy(OP_Node *root_node, OP_Bundle *bundle,
                                    fpreal time);

    ////////////////////////////////////////////

    UT_UniquePtr<ROP_GLTF_ExportRoot> myRoot;
    fpreal myEndTime;
    fpreal myStartTime;
    bool myExportingGLB;
    UT_String myFilename;
    UT_String myBasepath;
    ROP_GLTF_ErrorManager *myErrorHandler;
};

#endif
