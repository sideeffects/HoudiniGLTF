/*
 * Copyright (c) 2021
 *	Side Effects Software Inc.  All rights reserved.
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

#include "SOP_GLTF.h"

#include <algorithm>

#include <GU/GU_PackedGeometry.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_Snap.h>

#include <CMD/CMD_Manager.h>
#include <GA/GA_Names.h>
#include <GEO/GEO_AttributeHandle.h>
#include <GEO/GEO_PolyCounts.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_ChoiceList.h>
#include <PRM/PRM_SpareData.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_ParallelUtil.h>
#include <UT/UT_StringStream.h>

#include <GLTF/GLTF_Cache.h>
#include <GLTF/GLTF_GeoLoader.h>
#include <GLTF/GLTF_Loader.h>
#include <GLTF/GLTF_Types.h>

#if !defined(CUSTOM_GLTF_TOKEN_PREFIX)
    #define CUSTOM_GLTF_TOKEN_PREFIX ""
    #define CUSTOM_GLTF_LABEL_PREFIX ""
#endif

using namespace GLTF_NAMESPACE;

//-*****************************************************************************
constexpr const char* GLTF_NAME_ATTRIB = "name";
constexpr const char* GLTF_SCENE_NAME_ATTRIB = "scene_name";
constexpr const char* GLTF_PATH_ATTRIB = "path";

static std::string
sopGetRealFileName(const char *name)
{
    UT_String realname;

    // complete a path search in case it is in the geometry path
    UT_PathSearch::getInstance(UT_HOUDINI_GEOMETRY_PATH)
        ->findFile(realname, name);
    return realname.toStdString();
}

//-*****************************************************************************

OP_Node *
SOP_GLTF::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new SOP_GLTF(net, name, op);
}

//******************************************************************************

static int
selectGLTFScenes(void *data, int index, fpreal t, const PRM_Template *tplate)
{
    SOP_GLTF *gltf = reinterpret_cast<SOP_GLTF *>(data);
    UT_WorkBuffer cmd;
    UT_String filename;
    UT_String objectpath;

    cmd.strcpy("listchooser");
    cmd.strcat(" -r");

    auto &scene_names = gltf->getSceneNames();
    for (exint idx = 0; idx < scene_names.size(); idx++)
    {
        auto &scene_name = scene_names[idx];
        UT_String name(UT_String::ALWAYS_DEEP);

        if (scene_name == "")
            name = ("Scene" + std::to_string(idx + 1)).c_str();
        else
            name = scene_name;

        cmd.strcat(" ");
        cmd.strcat("'");
        cmd.strcat(name);
        cmd.strcat("'");
    }

    CMD_Manager *mgr = CMDgetManager();
    UT_OStringStream oss;
    mgr->execute(cmd.buffer(), 0, &oss);
    UT_String result(oss.str().buffer());
    result.trimBoundingSpace();

    if (result != "")
    {
        gltf->setChRefInt("scene", 0, t, result.toInt());
        gltf->setChRefString("loadby", 0, t, "scene", CH_STRING_LITERAL);
    }

    return 0;
}

static int
selectGLTFMeshes(void *data, int index, fpreal t, const PRM_Template *tplate)
{
    SOP_GLTF *gltf = reinterpret_cast<SOP_GLTF *>(data);
    UT_WorkBuffer cmd;
    UT_String filename;
    UT_String objectpath;

    cmd.strcpy("listchooser");
    cmd.strcat(" -r");

    auto &mesh_names = gltf->getMeshNames();
    for (exint idx = 0; idx < mesh_names.size(); idx++)
    {
        auto &mesh_name = mesh_names[idx];
        UT_String name(UT_String::ALWAYS_DEEP);

        if (mesh_name.first == "")
            name = ("Mesh" + std::to_string(idx + 1)).c_str();
        else
            name = mesh_name.first;

        cmd.strcat(" ");
        cmd.strcat("'");
        cmd.strcat(name);
        cmd.strcat("'");
    }

    CMD_Manager *mgr = CMDgetManager();
    UT_OStringStream oss;
    mgr->execute(cmd.buffer(), 0, &oss);
    if(mgr->getStatusCode())
	return 0;

    UT_String result(oss.str().buffer());
    result.trimBoundingSpace();
    if (result != "")
    {
        gltf->setChRefInt("meshid", 0, t, result.toInt());
        gltf->setChRefString("loadby", 0, t, "primitive", CH_STRING_LITERAL);
    }

    return 0;
}

static int
selectGLTFNodes(void *data, int index, fpreal t, const PRM_Template *tplate)
{
    SOP_GLTF *gltf = reinterpret_cast<SOP_GLTF *>(data);
    UT_WorkBuffer cmd;
    UT_String filename;
    UT_String objectpath;

    cmd.strcpy("listchooser");
    cmd.strcat(" -r");

    auto &node_names = gltf->getNodeNames();
    for (exint idx = 0; idx < node_names.size(); idx++)
    {
        auto &node_name = node_names[idx];
        UT_String name(UT_String::ALWAYS_DEEP);

        if (node_name == "")
            name = ("Node " + std::to_string(idx + 1)).c_str();
        else
            name = node_name;

        cmd.strcat(" ");
        cmd.strcat("'");
        cmd.strcat(name);
        cmd.strcat("'");
    }

    CMD_Manager *mgr = CMDgetManager();
    UT_OStringStream oss;
    mgr->execute(cmd.buffer(), 0, &oss);
    UT_String result(oss.str().buffer());
    result.trimBoundingSpace();

    if (result != "")
    {
        gltf->setChRefInt("nodeid", 0, t, result.toInt());
        gltf->setChRefString("loadby", 0, t, "node", CH_STRING_LITERAL);
    }

    return 0;
}

//-*****************************************************************************

static PRM_SpareData theTreeButtonSpareData(
    PRM_SpareArgs() << PRM_SpareToken(PRM_SpareData::getButtonIconToken(),
                                      "BUTTONS_tree"));

static PRM_SpareData
    gltfPattern(PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(),
                               "*.gltf, *.glb"));

static PRM_Name prm_filenameName("filename", "File Name");
static PRM_Name prm_loadBy("loadby", "Load By");
static PRM_Name prm_meshID("meshid", "Mesh ID");
static PRM_Name prm_primitiveIndex("primitiveindex", "Primitive Index");
static PRM_Name prm_rootnode("nodeid", "Root Node");
static PRM_Name prm_scene("scene", "Scene");
static PRM_Name prm_loadCustomAttribs("usecustomattribs", "Import Custom Attributes");
static PRM_Name prm_LoadNames("loadnames", "Import Names");
static PRM_Name prm_meshChooser("meshchooser", "Choose Mesh");
static PRM_Name prm_sceneChooser("scenechooser", "Choose Scene");
static PRM_Name prm_nodeChooser("nodechooser", "Choose Node");

static PRM_Name prm_geoType("geotype", "Geometry Type");
static PRM_Name prm_materialAssigns("materialassigns", "Import Material Assignments");

static PRM_Name prm_promotePointAttribs("promotepointattrs", "Promote Point Attributes to Vertex");
static PRM_Name prm_pointConsolidateDistance("pointconsolidatedist", "Points Merge Distance");

static PRM_Default prm_filenameDefault(0, "default.gltf");

static PRM_Name prm_addPathAttribute("addpathattribute", "Add Path Attribute");
static PRM_Name prm_pathAttribute("pathattribute", "Path Attribute");
static PRM_Default prm_pathAttributeDefault(0, GLTF_PATH_ATTRIB);

// Dropdown menus

static PRM_Name prm_loadByOptions[] = {PRM_Name("primitive", "Primitive"),
                                       PRM_Name("mesh", "Mesh"),
                                       PRM_Name("node", "Node"),
                                       PRM_Name("scene", "Scene"), PRM_Name()};

static PRM_Default prm_loadByDefault(0, "primitive");

static PRM_Name prm_geoTypeOptions[] = {
    PRM_Name("flattenedgeo", "Flattened Geometry"),
    PRM_Name("packedprim", "Packed Primitive"), PRM_Name()};

static PRM_Default prm_geoTypeDefault(0, "flattenedgeo");

static PRM_ChoiceList
    prm_loadByChoices(PRM_CHOICELIST_SINGLE, prm_loadByOptions);

static PRM_ChoiceList
    prm_geoTypeChoices(PRM_CHOICELIST_SINGLE, prm_geoTypeOptions);

PRM_Template SOP_GLTF::myTemplateList[] = {
    PRM_Template(PRM_FILE, 1, &prm_filenameName, &prm_filenameDefault, 0, 0, 0,
                 &gltfPattern),
    PRM_Template(PRM_ORD, 1, &prm_loadBy, &prm_loadByDefault,
                 &prm_loadByChoices),
    PRM_Template(PRM_INT_J, PRM_TYPE_JOIN_PAIR, 1, &prm_meshID),
    PRM_Template(PRM_CALLBACK, PRM_TYPE_NO_LABEL, 1, &prm_meshChooser, 0, 0, 0,
                 selectGLTFMeshes, &theTreeButtonSpareData),
    PRM_Template(PRM_INT_J, 1, &prm_primitiveIndex),
    PRM_Template(PRM_INT_J, PRM_TYPE_JOIN_PAIR, 1, &prm_rootnode),
    PRM_Template(PRM_CALLBACK, PRM_TYPE_NO_LABEL, 1, &prm_nodeChooser, 0, 0, 0,
                 selectGLTFNodes, &theTreeButtonSpareData),
    PRM_Template(PRM_INT_J, PRM_TYPE_JOIN_PAIR, 1, &prm_scene),
    PRM_Template(PRM_CALLBACK, PRM_TYPE_NO_LABEL, 1, &prm_sceneChooser, 0, 0, 0,
                 selectGLTFScenes, &theTreeButtonSpareData),
    PRM_Template(PRM_ORD, 1, &prm_geoType, &prm_geoTypeDefault,
                 &prm_geoTypeChoices),
    PRM_Template(PRM_TOGGLE, 1, &prm_promotePointAttribs, PRMoneDefaults),
    PRM_Template(PRM_FLT_J, 1, &prm_pointConsolidateDistance, &PRMfitToleranceDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_loadCustomAttribs, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &prm_LoadNames, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &prm_materialAssigns, PRMzeroDefaults),
    PRM_Template(PRM_TOGGLE, PRM_TYPE_TOGGLE_JOIN, 1, &prm_addPathAttribute,
                 PRMzeroDefaults),
    PRM_Template(PRM_STRING, 1, &prm_pathAttribute, &prm_pathAttributeDefault),

    PRM_Template()};

//-*****************************************************************************

SOP_GLTF::SOP_GLTF(OP_Network *net, const char *name, OP_Operator *op)
    : SOP_Node(net, name, op)
{
}

SOP_GLTF::~SOP_GLTF() {}

//-*****************************************************************************

bool
SOP_GLTF::updateParmsFlags()
{
    Parms parms;
    OP_Context c(0);
    evaluateParms(parms, c);

    GLTF_LoadStyle loadStyle = parms.myLoadStyle;
    uint32 promotePointAttrs = parms.myPromotePointAttrsToVertex;

    bool changed = false;
    changed |= enableParm("meshid", loadStyle == GLTF_LoadStyle::Primitive ||
						 loadStyle == GLTF_LoadStyle::Mesh);
    changed |= enableParm("primitiveindex", loadStyle == GLTF_LoadStyle::Primitive);
    changed |= enableParm("nodeid", loadStyle == GLTF_LoadStyle::Node);
    changed |= enableParm("scene", loadStyle == GLTF_LoadStyle::Scene);

    changed |= enableParm("pointconsolidatedist", promotePointAttrs == 1);
    changed |= enableParm("addpathattribute",
        loadStyle == GLTF_LoadStyle::Scene || loadStyle == GLTF_LoadStyle::Node);
    changed |= enableParm("pathattribute", parms.myAddPathAttribute);

    return changed;
}

OP_ERROR
SOP_GLTF::cookMySop(OP_Context &context)
{
    Parms parms;
    evaluateParms(parms, context);

    // Don't bother trying to load if we've yet to input a filename
    if (parms.myFileName == "")
        return error();

    auto loader = GLTF_Cache::GetInstance().LoadLoader(parms.myFileName);
    if (!loader)
        return UT_ERROR_ABORT;

    saveMeshNames(*loader);

    gdp->clearAndDestroy();

    const UT_String version = loader->getAsset().version;
    // Compare version with a lexiographic compare, should be
    // good enough for a warning
    if (version < "2.0" || version > "3.0")
    {
        addWarning(SOP_MESSAGE, "Attempting to load unsupported version");
        return error();
    }

    SOP_GLTF_Loader::Options options;
    options.loadNames = parms.myLoadNames;
    options.loadCustomAttribs = parms.myUseCustomAttribs;
    options.flatten = parms.myGeoType == GLTF_GeoType::Houdini_Geo;
    options.loadMats = parms.myLoadMats;
    options.promotePointAttribs = parms.myPromotePointAttrsToVertex;
    options.consolidateByMesh = !options.flatten
                                || parms.myLoadStyle
                                           == GLTF_LoadStyle::Primitive;
    options.pointConsolidationDistance = parms.myPointConsolidationDistance;
    options.addPathAttribute = parms.myAddPathAttribute;
    options.pathAttribute = parms.myPathAttribute;    

    if (getParent() && getParent()->getParent())
    {
	options.material_path = getParent()->getParent()->getFullPath();
	options.material_path.append("/materials/");
    }
    else
    {
        options.loadMats = false;
    }

    SOP_GLTF_Loader sop_loader(*loader, gdp, options);

    if (parms.myLoadStyle == GLTF_LoadStyle::Node)
    {
        GLTF_Handle node = parms.myRootNode;
        if (node >= loader->getNumNodes())
        {
            addError(SOP_MESSAGE, "Invalid Node");
            return error();
        }
        sop_loader.loadNode(*loader->getNode(node));
    }
    else if (parms.myLoadStyle == GLTF_LoadStyle::Mesh)
    {
        GLTF_Handle mesh = parms.myMeshID;
        if (mesh >= loader->getNumMeshes())
        {
            addError(SOP_MESSAGE, "Invalid Mesh");
            return error();
        }
        sop_loader.loadMesh(mesh);
    }
    else if (parms.myLoadStyle == GLTF_LoadStyle::Primitive)
    {
        // Just load in a single primitive
        if (!sop_loader.loadPrimitive(parms.myMeshID, parms.myPrimIndex))
        {
            addError(SOP_MESSAGE, "Invalid Primitive");
            return error();
        }
    }
    else if (parms.myLoadStyle == GLTF_LoadStyle::Scene)
    {
        GLTF_Handle scene = parms.myScene;

        if (scene >= loader->getNumScenes())
        {
            addError(SOP_MESSAGE, "Invalid Scene");
            return error();
        }

        sop_loader.loadScene(scene);
    }

    return error();
}

//*****************************************************************************

const UT_Array<std::pair<UT_String, GLTF_Handle>> &
SOP_GLTF::getMeshNames() const
{
    return myMeshes;
}

const UT_Array<UT_String> &
SOP_GLTF::getNodeNames() const
{
    return myNodes;
}

const UT_Array<UT_String> &
SOP_GLTF::getSceneNames() const
{
    return myScenes;
}

void
SOP_GLTF::installSOP(OP_OperatorTable *table)
{
    OP_Operator *gltf_op =
        new OP_Operator(
                        CUSTOM_GLTF_TOKEN_PREFIX "gltf",    // Internal name
                        CUSTOM_GLTF_LABEL_PREFIX "GLTF",    // GUI name
                        SOP_GLTF::myConstructor,            // Op Constructr
                        SOP_GLTF::myTemplateList,           // Parameter Definition
                        0,                                  // Min # of Inputs
                        0,                                  // Max # of Inputs
                        0,                                  // Local variables
                        OP_FLAG_GENERATOR                   // Generator flag
        );

    gltf_op->setIconName("OBJ_gltf_hierarchy");
    table->addOperator(gltf_op);
}

void
newSopOperator(OP_OperatorTable *table)
{
    SOP_GLTF::installSOP(table);
}

//////////////////////////////////////////////////////////

void
SOP_GLTF::saveMeshNames(const GLTF_NAMESPACE::GLTF_Loader &loader)
{
    myScenes.clear();
    myMeshes.clear();
    myNodes.clear();

    for (const GLTF_Scene *scene : loader.getScenes())
    {
        UT_String scene_name(UT_String::ALWAYS_DEEP, scene->name);
        myScenes.append(scene_name);
    }

    for (const GLTF_Node *node : loader.getNodes())
    {
        UT_String scene_name(UT_String::ALWAYS_DEEP, node->name);
        myNodes.append(scene_name);
    }

    for (const GLTF_Mesh *mesh : loader.getMeshes())
    {
        UT_String mesh_name(UT_String::ALWAYS_DEEP, mesh->name);
        myMeshes.append(
            {mesh_name, static_cast<GLTF_Handle>(mesh->primitives.size())});
    }
}

void
SOP_GLTF_Loader::getMaterialPath(GLTF_Int index, UT_String &path)
{   
    UT_String name;
    name.harden(myLoader.getMaterial(index)->name);

    // This is technically n^2 string compares, but it realistically doesn't matter
    bool is_duplicate = false;
    const auto& materials = myLoader.getMaterials();
    for (exint i = 0 ; i < materials.size(); i++)
    {
        if (i == index)
	{
	    continue;
	}

	if (materials[i]->name == name)
	{
	    is_duplicate = true;
	    break;
	}
    }

    // Now perform the same sanitization scheme as the Python script
    for (exint i = 0; i < name.length(); i++)
    {
        if (!isalnum(name[i]))
            name[i] = '_';
    }

    if (name[0] >= '0' && name[0] <= '9')
        name.prepend("_");
    
    // If the name is a duplicate, we use the format "matname_index"
    // This rule also applies when the material is unnamed
    if (is_duplicate)
    {
        name.append(UT_String("_" + std::to_string(index)));
    }

    // TODO: INCOMPLETE
    path.harden(myOptions.material_path);
    path.append(name);
}

void
SOP_GLTF::evaluateParms(Parms &parms, OP_Context &context)
{
    fpreal t = context.getTime();

    UT_String filename;
    evalString(filename, "filename", 0, t);

    if (filename.isstring())
    {
        parms.myFileName = sopGetRealFileName(filename);
    }

    int mesh_id;
    int primitive_index;
    int use_custom_attribs;
    int root_node;
    int scene;
    int load_mesh_names;
    int load_mats;
    int promote_points_attrs_to_vertex;
    fpreal point_consolidation_dist;

    mesh_id = evalInt("meshid", 0, t);
    primitive_index = evalInt("primitiveindex", 0, t);
    use_custom_attribs = evalInt("usecustomattribs", 0, t);
    root_node = evalInt("nodeid", 0, t);
    scene = evalInt("scene", 0, t);
    load_mesh_names = evalInt("loadnames", 0, t);
    load_mats = evalInt("materialassigns", 0, t);
    promote_points_attrs_to_vertex = evalInt("promotepointattrs", 0, t);
    point_consolidation_dist = evalFloat("pointconsolidatedist", 0, t);


    parms.myMeshID = static_cast<uint32>(mesh_id);
    parms.myPrimIndex = static_cast<uint32>(primitive_index);
    parms.myUseCustomAttribs = static_cast<uint32>(use_custom_attribs);
    parms.myRootNode = static_cast<uint32>(root_node);
    parms.myScene = static_cast<uint32>(scene);
    parms.myLoadNames = static_cast<uint32>(load_mesh_names);
    parms.myLoadMats = static_cast<uint32>(load_mats);
    parms.myPromotePointAttrsToVertex = static_cast<uint32>(promote_points_attrs_to_vertex);
    parms.myPointConsolidationDistance = point_consolidation_dist;

    UT_String l_type;
    evalString(l_type, "loadby", 0, t);

    UT_String geo_type;
    evalString(geo_type, "geotype", 0, t);

    if (l_type == "scene")
        parms.myLoadStyle = GLTF_LoadStyle::Scene;
    else if (l_type == "primitive")
        parms.myLoadStyle = GLTF_LoadStyle::Primitive;
    else if (l_type == "node")
        parms.myLoadStyle = GLTF_LoadStyle::Node;
    else if (l_type == "mesh")
        parms.myLoadStyle = GLTF_LoadStyle::Mesh;
    else
        UT_ASSERT(false);
    
    if (geo_type == "flattenedgeo")
        parms.myGeoType = GLTF_GeoType::Houdini_Geo;
    else if (geo_type == "packedprim")
        parms.myGeoType = GLTF_GeoType::Packed_Primitives;
    else
        UT_ASSERT(false);

    if (l_type == "scene" || l_type == "node")
    {
        parms.myAddPathAttribute = evalInt("addpathattribute", 0, t);
        evalString(parms.myPathAttribute, "pathattribute", 0, t);
    }
    else
    {
        parms.myAddPathAttribute = false;
    }
}

SOP_GLTF_Loader::SOP_GLTF_Loader(const GLTF_NAMESPACE::GLTF_Loader &loader, GU_Detail *detail,
                                 Options options)
    : myLoader(loader), myDetail(detail), myOptions(options)
{
}

void
SOP_GLTF_Loader::loadMesh(const GLTF_Handle mesh_idx)
{
    GLTF_Node dummy_node;
    dummy_node.mesh = mesh_idx;
    loadNode(dummy_node);
}

static void
sopConsolidatePoints(GU_Detail &detail, fpreal distance)
{
    // Consolidate points using GU_Snap
    GU_Snap::PointSnapParms snap_parms;

    GA_ElementGroup *output_grp = UTverify_cast<GA_ElementGroup *>(
            detail.getElementGroupTable(GA_ATTRIB_POINT).newInternalGroup());

    snap_parms.myConsolidate = true;
    snap_parms.myDeleteConsolidated = true;
    snap_parms.myDistance = distance;
    snap_parms.myModifyBothQueryAndTarget = true;
    snap_parms.myQPosH.bind(&detail, GA_ATTRIB_POINT, GA_Names::P);
    snap_parms.myTPosH.bind(&detail, GA_ATTRIB_POINT, GA_Names::P);
    snap_parms.myOutputGroup = output_grp;
    snap_parms.myMatchTol = 0.f;
    snap_parms.myMismatch = false;
    GU_Snap::snapPoints(detail, nullptr, snap_parms);
    GA_PrimitiveGroup prim_grp(detail);
    prim_grp.combine(output_grp);
    detail.cleanData(&prim_grp, false, 0.001F, true, true, true);
    detail.bumpDataIdsForAddOrRemove(true, false, false);
    detail.destroyGroup(output_grp);
}

void
SOP_GLTF_Loader::loadNode(const GLTF_Node &node)
{
    if (myOptions.loadNames)
    {
        myDetail->addStringTuple(GA_ATTRIB_PRIMITIVE, GLTF_NAME_ATTRIB, 1);
    }

    UT_WorkBuffer path_attrib;
    loadNodeRecursive(node, myDetail, UT_Matrix4F(1), path_attrib);

    if (myOptions.promotePointAttribs && !myOptions.consolidateByMesh)
    {
	// Consolidate points of the full detail
        sopConsolidatePoints(*myDetail, myOptions.pointConsolidationDistance);
    }
}

void
SOP_GLTF_Loader::loadScene(GLTF_Handle scene_idx)
{
    auto *scene = myLoader.getScene(scene_idx);

    if (myOptions.loadNames)
    {
        GA_RWHandleS scene_name_attr;
        scene_name_attr = myDetail->addStringTuple(GA_ATTRIB_DETAIL,
                                                   GLTF_SCENE_NAME_ATTRIB, 1);

        scene_name_attr.set(GA_Offset(0), 0, scene->name);
    }

    // The scene can have multiple nodes, so we create a dummy node to represent
    // the scene root
    GLTF_Node dummy_node;
    dummy_node.children = scene->nodes;
    loadNode(dummy_node);
}

bool
SOP_GLTF_Loader::loadPrimitive(GLTF_Handle node_idx, GLTF_Handle prim_idx)
{
    GLTF_GeoLoader loader(myLoader, node_idx, prim_idx, getGeoOptions());

    if (!loader.loadIntoDetail(*myDetail))
        return false;

    // Assign names or materials as required
    if (myOptions.loadNames)
    {
	// Loads the name in the format mesh_(i) where i is the index
	// of the primitive
        UT_String new_name;
	new_name.harden(myLoader.getMesh(node_idx)->name);
        new_name.append("_");
	new_name.append(std::to_string(prim_idx).c_str());

        GA_RWHandleS sm_name_attrib;
        sm_name_attrib =
            myDetail->addStringTuple(GA_ATTRIB_PRIMITIVE, GLTF_NAME_ATTRIB, 1);

        for (GA_Offset off : myDetail->getPrimitiveRange())
        {
            sm_name_attrib.set(off, 0, new_name);
        }
    }

    if (myOptions.loadMats)
    {
        UT_String mat_path;
        const GLTF_Primitive& prim = myLoader.getMesh(node_idx)->primitives[prim_idx];
        if (prim.material != GLTF_INVALID_IDX)
        {
            getMaterialPath(prim.material, mat_path);
        }

        GA_RWHandleS sm_mat_attrib;
        sm_mat_attrib = myDetail->addStringTuple(GA_ATTRIB_PRIMITIVE,
                                                GA_Names::shop_materialpath, 1);

        for (GA_Offset off : myDetail->getPrimitiveRange())
        {
            sm_mat_attrib.set(off, 0, mat_path);
        }
    }

    return true;
}

void
SOP_GLTF_Loader::loadNodeRecursive(const GLTF_Node &node, GU_Detail *parent_gd,
    UT_Matrix4F cum_xform, UT_WorkBuffer parent_path_attrib)
{
    UTgetInterrupt()->opInterrupt();

    UT_Matrix4F transform;
    node.getTransformAsMatrix(transform);

    cum_xform = transform * cum_xform;

    // The detail which is currently being operated on
    GU_Detail *gd;
    GA_RWHandleS name_attr;
    GA_RWHandleS mat_attr;
    GA_RWHandleS path_attr;

    GU_DetailHandle gdh;

    UT_WorkBuffer my_path_attrib = parent_path_attrib;
    if (myOptions.addPathAttribute)
    {
        if (!my_path_attrib.isEmpty())
           my_path_attrib.append("/");

        my_path_attrib.append(node.name);
    }

    if (!myOptions.flatten)
    {
        gdh.allocateAndSet(new GU_Detail);
        gd = gdh.writeLock();

        if (myOptions.loadNames)
        {
            name_attr = gd->addStringTuple(GA_ATTRIB_PRIMITIVE,
                GLTF_NAME_ATTRIB, 1);
        }
        if (myOptions.loadMats)
        {
            mat_attr = gd->addStringTuple(GA_ATTRIB_PRIMITIVE,
                GA_Names::shop_materialpath, 1);
        }
    }
    else
    {
	gd = parent_gd;
    }

    if (node.mesh != GLTF_INVALID_IDX)
    {
        const GLTF_Mesh &mesh = *myLoader.getMesh(node.mesh);
        const UT_Array<GLTF_Primitive> &primitives = mesh.primitives;

        for (GLTF_Handle idx = 0; idx < primitives.size(); idx++)
        {
            GU_DetailHandle prim_gdh;
	    prim_gdh.allocateAndSet(new GU_Detail, true);
            GU_Detail *prim_gd = prim_gdh.writeLock();

	    auto primitive = primitives[idx];
            UT_String mat_path;
	    if (primitive.material != GLTF_INVALID_IDX)
	    {
		getMaterialPath(primitive.material, mat_path);
	    }

            if (!GLTF_GeoLoader::load(myLoader, node.mesh, idx, *prim_gd,
                    getGeoOptions(my_path_attrib.buffer())))
            {
                prim_gdh.unlock(prim_gd);
                continue;
            }

            UTgetInterrupt()->opInterrupt();

            // Load as packed primitive
            if (!myOptions.flatten)
            {
                GU_PrimPacked *packed =
                    GU_PackedGeometry::packGeometry(*gd, prim_gdh);

                if (myOptions.loadNames)
                {
                    name_attr.set(packed->getPointOffset(0), 0, mesh.name);
                }
                
		if (myOptions.loadMats)
		{
                    mat_attr.set(packed->getPointOffset(0), 0, mat_path);
		}
                
                if (myOptions.addPathAttribute)
                {
                    UT_StringHolder the_path_attrib(my_path_attrib);
                    packed->setPathAttribute(the_path_attrib,
                        myOptions.pathAttribute);
                }
            }
            // Else load as a flattened hiereachy
            else
            {
                if (myOptions.loadNames)
                {
                    GA_RWHandleS sm_name_attrib;
                    sm_name_attrib = prim_gd->addStringTuple(
                        GA_ATTRIB_PRIMITIVE, GLTF_NAME_ATTRIB, 1);

                    for (GA_Offset off : prim_gd->getPrimitiveRange())
                    {
                        sm_name_attrib.set(off, 0, mesh.name);
                    }
                }

		if (myOptions.loadMats)
		{
		    GA_RWHandleS sm_mat_attrib;
		    sm_mat_attrib = prim_gd->addStringTuple(
			GA_ATTRIB_PRIMITIVE, GA_Names::shop_materialpath, 1);

		    for (GA_Offset off : prim_gd->getPrimitiveRange())
		    {
			sm_mat_attrib.set(off, 0, mat_path);
		    }
		}

                prim_gd->transform(cum_xform, 0, 0, true, true, true, true, true);
                gd->copy(*prim_gd, GEO_COPY_ADD, true, false, GA_DATA_ID_BUMP);
	    }

            prim_gdh.unlock(prim_gd);
        }
    }

    // Now run this on all children with the new transform
    for (GLTF_Handle child : node.children)
    {
        loadNodeRecursive(*myLoader.getNode(child), gd, cum_xform,
            my_path_attrib);
    }

    if (!myOptions.flatten)
    {
        GU_PrimPacked *packed =
            GU_PackedGeometry::packGeometry(*parent_gd, gdh);
        packed->transform(transform);

        UT_Vector3F translate;
        transform.getTranslates(translate);
        parent_gd->setPos3(packed->getPointOffset(0), translate);

        if (myOptions.loadNames)
        {
            GA_RWHandleS pname_attrib(parent_gd->findStringTuple(
                GA_ATTRIB_PRIMITIVE, GLTF_NAME_ATTRIB, 1, 1));
            if (pname_attrib.isValid())
                pname_attrib.set(packed->getPointOffset(0), 0, node.name);
        }

        if (myOptions.addPathAttribute)
        {
            UT_StringHolder the_path_attrib(my_path_attrib);
            packed->setPathAttribute(the_path_attrib, myOptions.pathAttribute);
        }

        gdh.unlock(gd);
    }
}

GLTF_NAMESPACE::GLTF_MeshLoadingOptions
SOP_GLTF_Loader::getGeoOptions(const char* pathAttributeValue) const
{
    GLTF_NAMESPACE::GLTF_MeshLoadingOptions options;
    options.loadCustomAttribs = myOptions.loadCustomAttribs;
    options.promotePointAttribs = myOptions.promotePointAttribs;
    options.consolidatePoints = myOptions.consolidateByMesh;
    options.pointConsolidationDistance = myOptions.pointConsolidationDistance;
    options.addPathAttribute = myOptions.addPathAttribute;
    options.pathAttributeName = myOptions.pathAttribute;
    
    if (pathAttributeValue)
        options.pathAttributeValue = pathAttributeValue;

    return options;
}
//
