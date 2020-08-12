/*
 * Copyright (c) COPYRIGHTYEAR
 *        Side Effects Software Inc.  All rights reserved.
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

#ifndef __SOP_GLTF_H__
#define __SOP_GLTF_H__

#include <GLTF/GLTF_Types.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_Array.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_Pair.h>

#include <GLTF/GLTF_Loader.h>
#include <GLTF/GLTF_GeoLoader.h>

class GU_Detail;
class GU_PrimPacked;

enum GLTF_LoadStyle
{
    Scene,
    Node,
    Mesh,
    Primitive
};

enum GLTF_GeoType
{
    Houdini_Geo,
    Packed_Primitives
};

typedef GLTF_NAMESPACE::GLTF_Int        GLTF_Int;
typedef GLTF_NAMESPACE::GLTF_Handle     GLTF_Handle;
typedef GLTF_NAMESPACE::GLTF_Node       GLTF_Node;

/// SOP to read GLTF geometry
class SOP_GLTF : public SOP_Node
{
public:
    // Standard hdk declarations
    static OP_Node *
    myConstructor(OP_Network *net, const char *name, OP_Operator *entry);

    static PRM_Template myTemplateList[];
    static void installSOP(OP_OperatorTable *table);

    // Returns a an array of pairs, where the index corresponds to the
    // index of the mesh, the first item in the pair is the name of
    // the mesh, and the second is the number of primitives
    const UT_Array<UT_Pair<UT_String, GLTF_Handle>> &getMeshNames() const;
    const UT_Array<UT_String> &getNodeNames() const;
    const UT_Array<UT_String> &getSceneNames() const;

protected:
    SOP_GLTF(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~SOP_GLTF();

    virtual bool updateParmsFlags() override;
    virtual OP_ERROR cookMySop(OP_Context &context) override;

    virtual void getDescriptiveParmName(UT_String &name) const override
    {
        name = "filename";
    }

private:
    void saveMeshNames(const GLTF_NAMESPACE::GLTF_Loader &loader);

    class Parms
    {
    public:
        Parms();

        UT_String myFileName;
        GLTF_LoadStyle myLoadStyle;
        GLTF_GeoType myGeoType;
        GLTF_Handle myMeshID;
        GLTF_Handle myPrimIndex;
        uint32 myUseCustomAttribs;
        GLTF_Handle myRootNode;
        GLTF_Handle myScene;
        uint32 myLoadNames;
        uint32 myLoadMats;
        uint32 myPromotePointAttrsToVertex;
        fpreal myPointConsolidationDistance;
    };

    void evaluateParms(Parms &parms, OP_Context &context);

    UT_Array<UT_String> myNodes;
    // The pair consists of <Name : Number of Primitives>
    UT_Array<UT_Pair<UT_String, GLTF_Handle>> myMeshes;
    UT_Array<UT_String> myScenes;
};

class SOP_GLTF_Loader
{
public:
    struct Options
    {
        Options() = default;
        ~Options() = default;
        bool loadNames = false;
	bool flatten = false;
        bool loadMats = false;
        bool loadCustomAttribs = false;
        UT_String material_path;
        bool promotePointAttribs = true;
        fpreal pointConsolidationDistance = 0.0001F;
    };

    SOP_GLTF_Loader(const GLTF_NAMESPACE::GLTF_Loader &loader, GU_Detail *detail,
                    Options options);

    void loadMesh(const GLTF_Handle mesh_idx);
    void loadNode(const GLTF_Node &node);
    void loadScene(GLTF_Handle scene_idx);
    bool loadPrimitive(GLTF_Handle node_idx, GLTF_Handle prim_idx);

private:
    void getMaterialPath(GLTF_Int index, UT_String &path);

    // Puts the current node in parent_gd as a packed primitive
    // with the name as well as transforms
    void loadNodeRecursive(const GLTF_Node &node, GU_Detail *parent_gd,
                           UT_Matrix4F cum_xform);

    void createAndSetName(GU_Detail *detail, const char *name) const;
    GLTF_NAMESPACE::GLTF_MeshLoadingOptions getGeoOptions() const;

    const GLTF_NAMESPACE::GLTF_Loader &myLoader;
    GU_Detail *myDetail;
    const Options myOptions;
};

#endif
