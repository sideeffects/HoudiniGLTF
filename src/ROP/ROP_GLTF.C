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
#include "ROP_GLTF.h"

#include <CH/CH_LocalVariable.h>
#include <IMG/IMG_Format.h>
#include <OBJ/OBJ_Geometry.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Director.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Templates.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_IOTable.h>
#include <UT/UT_IStream.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_JSONWriter.h>
#include <UT/UT_OFStream.h>

#include <PRM/PRM_Parm.h>

#include "ROP_GLTF_Image.h"
#include "ROP_GLTF_Refiner.h"

#include "ROP_GLTF_ExportRoot.h"
#include <GLTF/GLTF_Loader.h>
#include <GLTF/GLTF_Types.h>
#include <UT/UT_ArraySet.h>
#include <UT/UT_StringStream.h>

#if !defined(CUSTOM_GLTF_TOKEN_PREFIX)
    #define CUSTOM_GLTF_TOKEN_PREFIX ""
    #define CUSTOM_GLTF_LABEL_PREFIX ""
#endif

typedef GLTF_NAMESPACE::GLTF_PBRMetallicRoughness       GLTF_PBRMetallicRoughness;
typedef GLTF_NAMESPACE::GLTF_NormalTextureInfo          GLTF_NormalTextureInfo;

static PRM_Name theExportGeometryName("exportgeo", "Export Geometry");
static PRM_Name theCustomAttribsName("customattribs", "Export Custom Attributes");
static PRM_Name theExportMaterialsName("exportmaterials", "Export Materials");

static PRM_Name theFileName("file", "Output File");
static PRM_Name theExportTypeName("exporttype", "Export Type");
static PRM_Name theObjPathName("objpath", "Root Object");
static PRM_Name theObjectsName("objects", "Objects");
static PRM_Name theImageFormatName("imageformat", "Texture Format");
static PRM_Name theImageQualityName("imagequality", "Texture Quality");
static PRM_Name theMaxResolutionName("maxresolution", "Max Texture Resolution");
static PRM_Name theExportHiddenName("savehidden", "Save Non-Displayed (Hidden) Objects");
static PRM_Name theUseSOPPathName("usesoppath", "Use SOP Path");
static PRM_Name theFlipNormalmapYName("flipnormalmapy", "Flip Normal Map Y");
static PRM_Name theSOPPathName("soppath", "SOP Path");

static PRM_Name theExportNamesName("exportnames", "Export Names");
static PRM_Name theCullEmptyNodesName("cullempty", "Cull Empty Nodes");
static PRM_Name thePow2TexName("poweroftwo", "Rescale Texture as Power of Two");

static PRM_Default theFileDefault(0, "$HIP/output.gltf");
static PRM_Default theRootDefault(0, "/obj");
static PRM_Default theStarDefault(0, "*");
static PRM_Default theImageFormatDefault(0, "png");
static PRM_Default theMaxResolutionDefault(0, "png");
static PRM_Default theImageQualityDefault(75.f);
static PRM_Default theExportTypeDefault(0, "auto");

static PRM_SpareData gltfPattern(
    PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.gltf, *.glb"));

static PRM_Name theMaxResItems[] = {
    PRM_Name("0", "No Limit"),
    PRM_Name("256", "256x256"),
    PRM_Name("512", "512x512"),
    PRM_Name("1024", "1024x1024"), 
    PRM_Name("2048", "2048x2048"),
    PRM_Name("4096", "4096x4096"), 
    PRM_Name()
};

static PRM_Name theImageFormatItems[] = {
    PRM_Name("png", "PNG"),
    PRM_Name("jpg", "JPEG"),
    PRM_Name()
};

static PRM_Name theExportTypeItems[] = {
    PRM_Name("auto", "Detect from Filename"),
    PRM_Name("gltf", "glTF"), 
    PRM_Name("glb", "glb"), 
    PRM_Name()
};

static PRM_ChoiceList
    theImageFormatMenu(PRM_CHOICELIST_SINGLE, theImageFormatItems);

static PRM_ChoiceList
    theMaxResolutionMenu(PRM_CHOICELIST_SINGLE, theMaxResItems);

static PRM_ChoiceList
    theExportTypeMenu(PRM_CHOICELIST_SINGLE, theExportTypeItems);

static void
buildBundleMenu(void *, PRM_Name *menu, int max, const PRM_SpareData *spare,
                const PRM_Parm *)
{
    OPgetDirector()->getBundles()->buildBundleMenu(
        menu, max, spare ? spare->getValue("opfilter") : 0);
}

static PRM_ChoiceList theObjectsMenu(PRM_CHOICELIST_REPLACE, buildBundleMenu);

static PRM_SpareData
    theObjectList(PRM_SpareArgs() << PRM_SpareToken("opfilter", "!!OBJ!!")
                                  << PRM_SpareToken("oprelative", "/obj"));

static PRM_Template gltf_templates[] = {
    PRM_Template(PRM_FILE, 1, &theFileName, &theFileDefault, 0, 0, 0,
&gltfPattern),
    PRM_Template(PRM_ORD, 1, &theExportTypeName, &theExportTypeDefault,
                 &theExportTypeMenu),
    PRM_Template(PRM_TOGGLE, 1, &theUseSOPPathName),
    PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &theSOPPathName, 0, 0, 0,
                 0, &PRM_SpareData::sopPath),
    PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &theObjPathName,
                 &theRootDefault, 0, 0, 0, &PRM_SpareData::objPath),
    PRM_Template(PRM_STRING_OPLIST, PRM_TYPE_DYNAMIC_PATH_LIST, 1,
                 &theObjectsName, &theStarDefault, &theObjectsMenu, 0, 0,
                 &theObjectList),
    PRM_Template(PRM_ORD | PRM_TYPE_JOIN_NEXT, 1, &theImageFormatName,
                 &theImageFormatDefault,
                 &theImageFormatMenu),
    PRM_Template(PRM_INT_J, 1, &theImageQualityName, PRM90Defaults, 0,
                 &PRMnonNegativeRange),
    PRM_Template(PRM_ORD | PRM_TYPE_JOIN_NEXT, 1, &theMaxResolutionName,
                 &theMaxResolutionDefault,
                 &theMaxResolutionMenu),
    PRM_Template(PRM_TOGGLE, 1, &thePow2TexName, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theFlipNormalmapYName, PRMzeroDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theExportHiddenName, PRMzeroDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theCullEmptyNodesName, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theCustomAttribsName, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theExportNamesName, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theExportMaterialsName, PRMoneDefaults),
    PRM_Template()};

OP_Node *
ROP_GLTF::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new ROP_GLTF(net, name, op);
}

ROP_GLTF::ROP_GLTF(OP_Network *net, const char *name, OP_Operator *entry)
    : ROP_Node(net, name, entry)
{
    myErrorHandler = new ROP_GLTF_ErrorManager(*this);
}

bool
ROP_GLTF::updateParmsFlags()
{
    bool changed = false;

    // These parms need to be present since it's a ROP), but we don't actually
    // use them so we just hide them
    changed |= setVisibleState("trange", false);
    changed |= setVisibleState("take", false);
    changed |= setVisibleState("renderdialog", false);
    changed |= setVisibleState("f", false);

    const bool has_sop_input = hasSOPInput(0);
    const bool using_sop = has_sop_input || USE_SOP_PATH(0);
    const bool exporting_texture = EXPORT_MATERIALS(0);

    changed |= enableParm("usesoppath", !has_sop_input);
    changed |= enableParm("soppath", !has_sop_input && USE_SOP_PATH(0));
    changed |= enableParm("objpath", !using_sop);
    changed |= enableParm("objects", !using_sop);
    changed |= enableParm("poweroftwo", exporting_texture);
    changed |= enableParm("cullempty", !using_sop);

    UT_String format;
    IMAGEFORMAT(format, 0);

    changed |= setVisibleState("imagequality", format == "jpg");

    return changed;
}

ROP_GLTF::~ROP_GLTF()
{
    delete myErrorHandler;
}

int
ROP_GLTF::startRender(int, fpreal tstart, fpreal tend)
{
    if (!executePreRenderScript(tstart))
        return 0;

    myStartTime = tstart;
    myEndTime = tend;
    myExportingGLB = false;

    UT_String filename;
    OUTPUT_FILE(filename, myStartTime);
    UT_String ext(filename.fileExtension());
    ext.toLower();

    UT_String export_type;
    EXPORTTYPE(export_type, myStartTime);
    if ((export_type == "auto" && ext == ".glb") || export_type == "glb")
    {
        myExportingGLB = true;
    }

    filename.splitPath(myBasepath, myFilename);

    return 1;
}

ROP_RENDER_CODE
ROP_GLTF::renderFrame(fpreal time, UT_Interrupt *boss)
{
    if (!myRoot)
    {
        // Build from hierarchy
        InitializeGLTFTree(time);
        BuildGLTFTree(time);
    }
    return ROP_CONTINUE_RENDER;
}

ROP_RENDER_CODE
ROP_GLTF::endRender()
{
    if (!WriteTreeToDisk(myStartTime))
    {
        myErrorHandler->AddError(ROP_MESSAGE, "Unable to output file.");
    }

    myRoot = nullptr;
    return ROP_CONTINUE_RENDER;
}

void
ROP_GLTF::AssignGLTFTransform(GLTF_Node &gltf_node, OBJ_Node *node,
                              fpreal time) const
{
    OP_Context context(time);
    UT_Matrix4D pre_transform;
    UT_Matrix4D parm_transform;
    UT_Matrix4D transform;

    node->getTransform(TransformMode::TRANSFORM_PRE, pre_transform, context);
    node->getTransform(TransformMode::TRANSFORM_PARM, parm_transform, context);

    transform = pre_transform * parm_transform;
    gltf_node.matrix = transform;
}

void
ROP_GLTF::AssignGLTFName(GLTF_Node &gltf_node, OBJ_Node *node,
                         fpreal time) const
{
    gltf_node.name = node->getName();
}

void
ROP_GLTF::SetupGLTFMesh(GLTF_Node &gltf_node, OBJ_Node *node, fpreal time,
                        SOP_Node *sop)
{
    OP_Context context(time);
    OBJ_Geometry *geo = nullptr;

    if (node->getObjectType() == OBJ_GEOMETRY)
        geo = node->castToOBJGeometry();

    if (!geo)
        return;

    if (!sop)
    {
        sop = geo->getRenderSopPtr();
        if (!sop)
            return;
    }

    GU_ConstDetailHandle gdh = sop->getCookedGeoHandle(context);
    GU_DetailHandleAutoReadLock rlock(gdh);
    const GU_Detail *gdp = rlock.getGdp();

    if (!gdp)
        return;

    // Create geometry
    GLTF_Mesh sop_mesh;
    UT_Array<UT_StringHolder> prim_materials;
    bool should_export_materials = EXPORT_MATERIALS(time);

    auto create_material_node_func =
        [&](const UT_StringHolder &mat_str) -> GLTF_Handle {
        if (!should_export_materials)
        {
            return GLTF_INVALID_IDX;
        }
        const OP_Node *mat_node = node->findNode(mat_str);
        if (!mat_node)
        {
            myErrorHandler->AddWarning(UT_ERROR_MESSAGE,
                                       "Skipped invalid material node.");
            return GLTF_INVALID_IDX;
        }

        GLTF_Handle material_idx = TranslatePrincipledShader(context, mat_node);
        if (material_idx == GLTF_INVALID_IDX)
        {
            return GLTF_INVALID_IDX;
        }

        return material_idx;
    };

    const PRM_Parm &mat_parm = geo->getParm("shop_materialpath");
    UT_StringHolder mat_path;

    mat_parm.getValue(context.getTime(), mat_path, 0, /*expand=*/true,
                      SYSgetSTID());

    ROP_GLTF_Refiner::Refine_Options options;
    options.output_custom_attribs = EXPORT_CUSTOM_ATTRIBS(time);

    ROP_GLTF_Refiner::refine(gdp, *myRoot, gltf_node, mat_path,
                             create_material_node_func, options);
}

bool
ROP_GLTF::IsExportingGLB() const
{
    return myExportingGLB;
}

const UT_String &
ROP_GLTF::GetBasePath() const
{
    return myBasepath;
}

const IMG_Format *
ROP_GLTF::GetImageFormat(fpreal time) const
{
    UT_String image_format;
    IMAGEFORMAT(image_format, time);
    if (image_format == "png")
    {
        return IMG_Format::findFormatByName("PNG");
    }
    else if (image_format == "jpg")
    {
        return IMG_Format::findFormatByName("JPEG");
    }
    return nullptr;
}

const char *
ROP_GLTF::GetImageMimeType(fpreal time) const
{
    UT_String image_format;
    IMAGEFORMAT(image_format, time);
    if (image_format == "png")
    {
        return "image/png";
    }
    else if (image_format == "jpg")
    {
        return "image/jpeg";
    }
    return nullptr;
}

bool
ROP_GLTF::WriteTreeToDisk(fpreal time)
{
    UT_String savepath;
    OUTPUT_FILE(savepath, time);

    if (IsExportingGLB())
    {
        if (!myRoot->ExportAsGLB(savepath))
            return false;
    }
    else
    {
        if (!myRoot->ExportGLTF(savepath))
            return false;
    }

    return true;
}

void
ROP_GLTF::InitializeGLTFTree(fpreal time)
{
    ROP_GLTF_ExportRoot::ExportSettings settings;
    settings.exportNames = EXPORT_NAMES(time);
    myRoot =
        UT_UniquePtr<ROP_GLTF_ExportRoot>(new ROP_GLTF_ExportRoot(settings));
}

bool
ROP_GLTF::BuildGLTFTree(fpreal time)
{
    // If we have a SOP specified, then build from that
    SOP_Node *sop = GetSOPNode(time);
    OBJ_Node *geo = nullptr;
    if (sop)
    {
        OBJ_Node *obj = CAST_OBJNODE(sop->getCreator());
        if (!obj)
            return false;

        geo = obj->castToOBJGeometry();

        if (!geo)
            return false;

        BuildFromSOP(time, sop, geo);
        return true;
    }

    // Otherwise, we output the entire hierarchy starting from some root node
    UT_String object_path;
    UT_String objects;
    OP_Bundle *bundle;

    OBJPATH(object_path, time);
    OP_Node *rootnode = findNode(object_path);

    if (!rootnode)
        return false;

    OBJECTS(objects, time);

    bundle = getParmBundle("objects", 0, objects,
                           OPgetDirector()->getManager("obj"), "!!OBJ!!");

    if (!bundle)
        return false;

    BuildGLTFTreeFromHierarchy(rootnode, bundle, time);
    return true;
}

bool
ROP_GLTF::BuildFromSOP(fpreal time, SOP_Node *sop, OBJ_Node *geo)
{
    uint32 root_scene_idx;
    GLTF_Scene &root_scene = InitializeBasicGLTFScene(root_scene_idx);

    GLTF_Node mock_root;
    AssignGLTFName(mock_root, geo, time);
    SetupGLTFMesh(mock_root, geo, time, sop);

    // If we're just exporting a bunch of packed primitives - then
    // set them up as multiple nodes on a scene instead.  This is
    // a bit hacky, but needed to deal with SOP roundtripping.

    if (mock_root.mesh == GLTF_INVALID_IDX)
    {
        root_scene.name = mock_root.name;
        root_scene.nodes = mock_root.children;
    }
    else
    {
        uint32 root_node_idx;
        GLTF_Node &root_node = myRoot->CreateNode(root_node_idx);
        root_node = mock_root;
        root_scene.nodes.append(root_node_idx);
    }

    return true;
}

bool
ROP_GLTF::BuildGLTFTreeFromHierarchy(OP_Node *root_node, OP_Bundle *bundle,
                                     fpreal time)
{
    UT_Array<OBJ_Node *> work;
    UT_Set<OBJ_Node *> visited;

    GLTF_Handle root_scene_idx;
    GLTF_Scene &root_scene = InitializeBasicGLTFScene(root_scene_idx);

    GLTF_Handle root_node_idx;
    GLTF_Node &root_gltf_node = myRoot->CreateNode(root_node_idx);
    root_gltf_node.name = "Root";

    root_scene.nodes.append(root_node_idx);

    for (exint i = 0; i < bundle->entries(); ++i)
    {
        OBJ_Node *obj = bundle->getNode(i)->castToOBJNode();
        if (obj && visited.find(obj) == visited.end())
        {
            visited.insert(obj);
            work.append(obj);
        }
    }

    auto translate_node = [&](GLTF_Node &gltf_node, OBJ_Node *node,
                              fpreal time) -> void {
        AssignGLTFTransform(gltf_node, node, time);
        AssignGLTFName(gltf_node, node, time);

        if (SAVE_HIDDEN(time) || node->getObjectDisplay(time))
        {
            SetupGLTFMesh(gltf_node, node, time);
        }
    };

    GLTF_HierarchyBuilder builder(root_node, &root_gltf_node, *myRoot,
                                  translate_node);

    OP_Context context(time);
    UT_Map<OP_Node *, uint32> node_map;

    for (OBJ_Node *job : work)
    {
        if (!job)
            continue;

        if (CULL_EMPTY_NODES(context.getTime()) &&
            job->getObjectType() != OBJ_GEOMETRY)
        {
            continue;
        }

        builder.Traverse(job, time);
    }

    return true;
}

uint32
ROP_GLTF::TranslatePrincipledShader(const OP_Context &context,
                                    const OP_Node *ps_node)
{
    auto cached_mat = myRoot->GetMaterialCache().find(ps_node);
    if (cached_mat != myRoot->GetMaterialCache().end())
        return cached_mat->second;

    UT_String op_name = UT_String(ps_node->getOperator()->getName());
    if (op_name != "principledshader::2.0")
    {
        myErrorHandler->AddWarning(
            UT_ERROR_MESSAGE,
            "Non-principled-shader material assigned, skipping.");
        return GLTF_INVALID_IDX;
    }

    uint32 mat_idx;
    GLTF_Material &material = myRoot->CreateMaterial(mat_idx);
    GLTF_PBRMetallicRoughness metallic_roughness;

    material.name = ps_node->getName();
    material.name.harden();

	// Set alphamode for material based on spare parms
	if (ps_node->hasParm("gltf_alphamode"))
	{
		ps_node->getParm("gltf_alphamode")
			.getValue(context.getTime(), material.alphaMode, 0,
				true, SYSgetSTID());

		if (material.alphaMode == "MASK")
		{
			if (ps_node->hasParm("gltf_alphacutoff"))
			{
				fpreal cutoff = 0;
				ps_node->getParm("gltf_alphacutoff")
					.getValue(context.getTime(), cutoff, 0, SYSgetSTID());
				material.alphaCutoff = cutoff;
			}
		}
	}
	
    // Translate pbrMetallicRoughness properties
    {
        const PRM_Parm &mat_parm = ps_node->getParm("basecolor");
        fpreal32 color[3];
        mat_parm.getValues(context.getTime(), color, SYSgetSTID());
        metallic_roughness.baseColorFactor = UT_Vector4F(color, 1.0f);
    }

    int basecolor_use_texture = 0;

    ps_node->getParm("basecolor_useTexture")
        .getValue(context.getTime(), basecolor_use_texture, 0, SYSgetSTID());

    if (basecolor_use_texture)
    {
        UT_StringHolder basecolor_texture;
        ps_node->getParm("basecolor_texture")
            .getValue(context.getTime(), basecolor_texture, 0,
                      /*expand=*/true, SYSgetSTID());

        if (basecolor_texture)
        {
            auto texinfo =  GLTF_TextureInfo();
	    if (TranslateTexture(UT_String(basecolor_texture), context, texinfo))
	    {
		metallic_roughness.baseColorTexture = texinfo;
	    }
        }
    }

    {
        UT_StringHolder rough_texture;
        UT_StringHolder metallic_texture;
        int rough_use_texture = 0;
        int met_use_texture = 0;

        UT_Array<ROP_GLTF_ChannelMapping> mapping{};

        ps_node->getParm("rough_useTexture")
            .getValue(context.getTime(), rough_use_texture, 0, SYSgetSTID());

        if (rough_use_texture)
        {
            ps_node->getParm("rough_texture")
                .getValue(context.getTime(), rough_texture, 0, /*expand=*/true,
                          SYSgetSTID());
            mapping.append({rough_texture, 1, 1});
        }

        ps_node->getParm("metallic_useTexture")
            .getValue(context.getTime(), met_use_texture, 0, SYSgetSTID());

        if (met_use_texture)
        {
            ps_node->getParm("metallic_texture")
                .getValue(context.getTime(), metallic_texture, 0,
                          /*expand=*/true, SYSgetSTID());
            mapping.append({metallic_texture, 2, 2});
        }

        if (mapping.size() > 0)
        {
            auto texinfo = GLTF_TextureInfo();
	    if (TranslateTexture(mapping, context, texinfo))
	    {
		metallic_roughness.metallicRoughnessTexture = texinfo;
	    }

        }
    }

    fpreal rough;
    ps_node->getParm("rough").getValue(context.getTime(), rough, 0,
                                       SYSgetSTID());
    metallic_roughness.roughnessFactor = rough;

    fpreal metallic;
    ps_node->getParm("metallic")
        .getValue(context.getTime(), metallic, 0, SYSgetSTID());
    metallic_roughness.metallicFactor = metallic;

    material.metallicRoughness = std::move(metallic_roughness);

    int normal_use_texture = 0;
    ps_node->getParm("baseBumpAndNormal_enable")
        .getValue(context.getTime(), normal_use_texture, 0, SYSgetSTID());

    if (normal_use_texture)
    {
        UT_StringHolder bump_type;
        ps_node->getParm("baseBumpAndNormal_type")
            .getValue(context.getTime(), bump_type, 0, /*expand=*/true, SYSgetSTID());
        if (bump_type == "normal")
        {

            UT_StringHolder normal_texture;
            ps_node->getParm("baseNormal_texture")
                .getValue(context.getTime(), normal_texture, 0, /*expand=*/true,
                    SYSgetSTID());

            if (normal_texture)
            {
                ROP_GLTF_TextureParms tex_parms;
                tex_parms.myFlipGreenChannel = FLIP_Y_NORMALS(context.getTime());

                GLTF_NormalTextureInfo texinfo;
                if (TranslateTexture(UT_String(normal_texture), context,
                    texinfo, tex_parms))
                {
                    material.normalTexture = texinfo;
                }
            }
        }
    }

    int emissive_use_texture = 0;

    ps_node->getParm("emitcolor_useTexture")
        .getValue(context.getTime(), emissive_use_texture, 0, SYSgetSTID());
    if (emissive_use_texture)
    {
        UT_StringHolder emissive_texture;
        ps_node->getParm("emitcolor_texture")
            .getValue(context.getTime(), emissive_texture, 0, /*expand=*/true,
                      SYSgetSTID());

        if (emissive_texture)
        {
            GLTF_TextureInfo texinfo;
	    if (TranslateTexture(UT_String(emissive_texture), context, texinfo))
	    {
		material.emissiveTexture = texinfo;
	    }
        }
    }

    {
        fpreal32 emissive[3];
        ps_node->getParm("emitcolor")
            .getValues(context.getTime(), emissive, SYSgetSTID());
        material.emissiveFactor = UT_Vector3F(emissive);
    }

    myRoot->GetMaterialCache().insert({ps_node, mat_idx});

    return mat_idx;
}

void
ROP_GLTF::GetNonCollidingName(const UT_String &s, UT_String &out)
{
    UT_String name = s;
    while (myRoot->GetNameUsagesMap().find(name) !=
           myRoot->GetNameUsagesMap().end())
    {
        exint times = myRoot->GetNameUsagesMap().find(s)->second++;
        name += std::to_string(times);
    }
    myRoot->GetNameUsagesMap().insert({s, 1});
    name.harden();

    out = UT_String(UT_String::ALWAYS_DEEP, name);
}

bool
ROP_GLTF::TranslateTexture(const UT_String &path, const OP_Context &context,
                           GLTF_TextureInfo &tex_info, const ROP_GLTF_TextureParms& tex_parms)
{
    auto cached_tex = myRoot->GetImageCache().find(path);
    if (cached_tex != myRoot->GetImageCache().end())
    {
        tex_info.index = cached_tex->second;
        return cached_tex->second != GLTF_INVALID_IDX;
    }

    UT_String new_path = GetBasePath();
    UT_String file_name;
    const IMG_Format *format = GetImageFormat(context.getTime());

    if (path.startsWith("op:"))
    {
        OP_Node *node = OPgetDirector()->findNode(path);
        UT_ASSERT(node);
        // We should always find the node, but just in case...
        if (!node)
            file_name = "image"; 
        file_name = node->getName();
    }
    else
    {
        UT_String dir_name;
        path.splitPath(dir_name, file_name);
    }

    // Sanitize the filename
    file_name = file_name.pathUpToExtension();
    file_name.forceValidVariableName();

    new_path += "/";
    new_path += file_name;
    GetNonCollidingName(new_path, new_path);
    new_path += ".";
    new_path += format->getDefaultExtension();

    auto output_imagedata = [&](std::ostream &os, const IMG_Format *format,
                                const ROP_GLTF_ImgExportParms &parms) -> bool
    {
        UT_AutoInterrupt progress("Outputting Images");
        if (progress.wasInterrupted())
            return false;

	if (!ROP_GLTF_Image::OutputImage(path, format, os, context.getTime(),
	    parms, *myErrorHandler))
	{
	    return false;
	}

	return true;
    };

    uint32 img_idx = OutputTexture(new_path, tex_parms, output_imagedata,
                                   tex_info, context);

    if (img_idx == GLTF_INVALID_IDX)
    {
        UT_String s = "Failed to create texture at ";
        s += new_path;
        myErrorHandler->AddWarning(UT_ERROR_MESSAGE, s);
    }

    myRoot->GetImageCache().insert({path, img_idx});

    return img_idx != GLTF_INVALID_IDX;
}

bool
ROP_GLTF::TranslateTexture(const UT_Array<ROP_GLTF_ChannelMapping> &mappings,
                           const OP_Context &context, GLTF_TextureInfo &tex_info,
                           const ROP_GLTF_TextureParms &tex_parms)
{
    if (myRoot->HasCachedChannelImage(mappings))
    {
        uint32 img_idx = myRoot->GetCachedChannelImage(mappings);
        tex_info.index = img_idx;
        return img_idx != GLTF_INVALID_IDX;
    }

    UT_ASSERT(mappings.size() > 0);

    UT_String new_path = GetBasePath();
    UT_String dir_name;
    UT_String file_name;
    UT_String first_elem_str(mappings.begin()->path);
    const IMG_Format *format = GetImageFormat(context.getTime());
    first_elem_str.splitPath(dir_name, file_name);

    // Sanitize the filename
   
    file_name = file_name.pathUpToExtension();
    file_name.forceValidVariableName();

    new_path += "/";
    new_path += file_name;
    GetNonCollidingName(new_path, new_path);
    new_path += ".";
    new_path += format->getDefaultExtension();

    auto output_imagedata = [&](std::ostream &os, const IMG_Format *format,
                                const ROP_GLTF_ImgExportParms &parms) -> bool
    {
        UT_AutoInterrupt progress("Outputting Images");
        if (progress.wasInterrupted())
            return false;

        if (!ROP_GLTF_Image::CreateMappedTexture(mappings, os, format,
                                                 context.getTime(), parms,
                                                 *myErrorHandler))
        {
	    return false;
        }

        return true;
    };

    uint32 tex_idx = OutputTexture(new_path, tex_parms, output_imagedata, tex_info, context);

    if (tex_idx == GLTF_INVALID_IDX)
    {
        UT_String s = "Failed to create metallic roughness texture at ";
        s += new_path;
        myErrorHandler->AddWarning(UT_ERROR_MESSAGE, s);
    }

    myRoot->InsertCachedChannelImage(mappings, tex_idx);

    return tex_idx != GLTF_INVALID_IDX;
}

uint32
ROP_GLTF::OutputTexture(const UT_String &output_path, const ROP_GLTF_TextureParms &parms,
                        std::function<bool(std::ostream &, const IMG_Format *,
			const ROP_GLTF_ImgExportParms &)> output_function,
                        GLTF_TextureInfo &tex_info, const OP_Context &context)
{
    // todo: better error handling
    ROP_GLTF_ImgExportParms img_parms;
    
    GLTF_Image image;
    GLTF_Texture tex;
    const IMG_Format *format = GetImageFormat(context.getTime());

    img_parms.roundUpPowerOfTwo = POWER_OF_TWO(context.getTime());
    img_parms.flipGreen = parms.myFlipGreenChannel;

    int imgq = IMAGE_QUALITY(context.getTime());
    img_parms.quality = static_cast<exint>(SYSclamp(imgq, 0, 100));

    UT_String res;
    MAXRES(res, context.getTime());
    img_parms.max_res = static_cast<exint>(res.toInt());
    if (img_parms.max_res == 0)
        img_parms.max_res = SYS_EXINT_MAX;

    if (IsExportingGLB())
    {
        uint32 databuffer_offset;
        std::stringstream image_binary;
        uint32 image_size;
        uint32 bufferview_idx;
        GLTF_BufferView &bufferview = myRoot->CreateBufferview(bufferview_idx);
        char *gltf_imagedata;
        const IMG_Format *format = GetImageFormat(context.getTime());

	if (!output_function(image_binary, format, img_parms))
	    return GLTF_INVALID_IDX;

        image_size = image_binary.tellp();

        gltf_imagedata = static_cast<char *>(
            myRoot->BufferAlloc(0, image_size, 4, databuffer_offset));

        image_binary.read(gltf_imagedata, image_size);

        bufferview.buffer = GLTF_NAMESPACE::GLB_BUFFER_IDX;
        bufferview.byteLength = image_size;
        bufferview.byteOffset = databuffer_offset;

        image.bufferView = bufferview_idx;
    }
    else
    {
        std::stringstream image_data;
        if(!output_function(image_data, format, img_parms))
            return GLTF_INVALID_IDX;

        UT_OFStream fs;
        fs.open(output_path);
        fs << image_data.rdbuf();
        fs.close();

        image.uri = output_path;
        image.uri.harden();
    }

    image.mimeType = GetImageMimeType(context.getTime());
    
    uint32 image_idx;
    uint32 tex_idx;

    myRoot->CreateImage(image_idx) = image;
    tex.source = image_idx;

    myRoot->CreateTexture(tex_idx) = tex;

    tex_info.texCoord = 0;
    tex_info.index = image_idx;

    return tex_idx;
}

GLTF_Scene &
ROP_GLTF::InitializeBasicGLTFScene(GLTF_Handle &root_scene_idx)
{
    GLTF_Handle scene_idx;
    GLTF_Scene &default_scene = myRoot->CreateScene(scene_idx);
    GLTF_Handle buffer_idx;
    GLTF_Buffer &defaultBuffer = myRoot->CreateBuffer(buffer_idx);

    myRoot->SetDefaultScene(scene_idx);

    if (!IsExportingGLB())
    {
        defaultBuffer.myURI = myFilename.pathUpToExtension();
        defaultBuffer.myURI += "_data.bin";
        defaultBuffer.name = "main_buffer";
    }

    root_scene_idx = scene_idx;

    return default_scene;
}

ROP_GLTF::GLTF_HierarchyBuilder::GLTF_HierarchyBuilder(
    OP_Node *root_node, GLTF_Node *root_gltf, ROP_GLTF_ExportRoot &export_root,
    std::function<void(GLTF_Node &, OBJ_Node *, fpreal time)> proc_func)
    : myRootNode(root_node),
      myRootExporter(export_root),
      myRootGLTF(root_gltf),
      myFunc(proc_func)
{
}

bool
ROP_GLTF::hasSOPInput(fpreal time) const
{
    SOP_Node *sop = CAST_SOPNODE(getInput(0));
    return (sop);
}

SOP_Node *
ROP_GLTF::GetSOPNode(fpreal time) const
{
    SOP_Node *sop = CAST_SOPNODE(getInput(0));
    if (!sop && USE_SOP_PATH(time))
    {
        UT_String sop_path;
        SOP_PATH(sop_path, time);
        sop_path.trimBoundingSpace();
        if (sop_path.isstring())
            sop = CAST_SOPNODE(findNode(sop_path));
    }

    return sop;
}

uint32
ROP_GLTF::GLTF_HierarchyBuilder::Traverse(OBJ_Node *node, fpreal time)
{
    bool parent_is_root = false;
    bool i_am_root = false;
    const auto node_it = myNodeMap.find(node);
    OP_Node *parent;
    OBJ_Node *obj_parent;

    // If we've already been parsed, then return
    if (node_it != myNodeMap.cend())
        return node_it->second;

    // If parent hasn't been traversed, then traverse the parent
    parent = node->getInput(0);
    if (!parent)
    {
        parent = node->getParent();
    }

    if (parent)
    {
        obj_parent = parent->castToOBJNode();
        if (parent == myRootNode)
        {
            parent_is_root = true;
        }
        else if (node == myRootNode)
        {
            i_am_root = true;
        }
        else if (obj_parent)
        {
            Traverse(obj_parent, time);
        }
    }

    // Create node, register ourself as child of parent
    uint32 node_idx;
    GLTF_Node *gltf_node;

    if (i_am_root)
    {
        gltf_node = myRootGLTF;
        node_idx = 0;
    }
    else
    {
        gltf_node = &myRootExporter.CreateNode(node_idx);
    }

    // Assign properties to the node
    myFunc(*gltf_node, node, time);

    // Parse parents
    if (!i_am_root)
    {
        if (parent_is_root)
        {
            myRootGLTF->children.append(node_idx);
        }
        else
        {
            const auto parent_it = myNodeMap.find(parent);
            if (parent_it != myNodeMap.end())
            {
                myRootExporter.getNode(parent_it->second)
                    ->children.append(node_idx);
            }
        }
    }

    myNodeMap.insert({node, node_idx});
    return node_idx;
}

ROP_GLTF::ROP_GLTF_ErrorManager::ROP_GLTF_ErrorManager(ROP_GLTF &gltf)
    : myNode(gltf)
{
}

void
ROP_GLTF::ROP_GLTF_ErrorManager::AddError(int code, const char *msg) const
{
    myNode.addError(code, msg);
}

void
ROP_GLTF::ROP_GLTF_ErrorManager::AddWarning(int code, const char *msg) const
{
    myNode.addWarning(code, msg);
}

void
newDriverOperator(OP_OperatorTable *table)
{
    OP_TemplatePair pair(gltf_templates);
    OP_TemplatePair templatepair(ROP_Node::getROPbaseTemplate(), &pair);
    OP_VariablePair vp(ROP_Node::myVariableList);
    OP_Operator *gltf_op =
        new OP_Operator(
                        CUSTOM_GLTF_TOKEN_PREFIX "gltf", 
                        CUSTOM_GLTF_LABEL_PREFIX "GLTF", 
                        ROP_GLTF::myConstructor, &templatepair,
                        0, 9999, &vp, OP_FLAG_UNORDERED | OP_FLAG_GENERATOR);
    gltf_op->setIconName("OBJ_gltf_hierarchy");
    table->addOperator(gltf_op);
}

void
newSopOperator(OP_OperatorTable *table)
{
    OP_TemplatePair pair(gltf_templates);
    OP_TemplatePair templatepair(ROP_Node::getROPbaseTemplate(), &pair);
    OP_VariablePair vp(ROP_Node::myVariableList);
    OP_Operator *gltf_op = new OP_Operator(
        CUSTOM_GLTF_TOKEN_PREFIX "rop_gltf", 
        CUSTOM_GLTF_LABEL_PREFIX "ROP GLTF Output", 
        ROP_GLTF::myConstructor, &templatepair,
        0, 1, &vp, OP_FLAG_GENERATOR | OP_FLAG_MANAGER);
    gltf_op->setIconName("OBJ_gltf_hierarchy");
    table->addOperator(gltf_op);
}
