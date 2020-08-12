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

#include "ROP_GLTF_Image.h"

#include "ROP_GLTF.h"
#include <COP2/COP2_ImageSource.h>
#include <COP2/COP2_Node.h>
#include <IMG/IMG_File.h>
#include <IMG/IMG_Format.h>
#include <OP/OP_Director.h>
#include <PXL/PXL_Fill.h>
#include <PXL/PXL_Raster.h>
#include <TIL/TIL_Raster.h>
#include <TIL/TIL_Sequence.h>
#include <UT/UT_OStream.h>

#include <ostream>

using namespace GLTF_NAMESPACE;

// We use a set internal format when processing the pixels as all merged
// rasters must have the same format.  As jpeg & png both only support 8 bits
// per color per pixel, we default to using RGB8.
class theWorkFormat
{
public:
    static const PXL_DataFormat px_data_format = PXL_INT8;
    static const IMG_DataType img_data_format = IMG_INT8;
    static const IMG_ComponentOrder img_component_order = IMG_COMPONENT_RGBA;
};

bool
ROP_GLTF_Image::OutputImageToStream(const UT_String &filename,
                                    const IMG_Format *format, std::ostream &os,
                                    fpreal time,
                                    const ROP_GLTF_ImgExportParms &parms)
{
    UT_Array<UT_SharedPtr<PXL_Raster>> rasters;
    IMG_Stat stat;

    if (!GetImageRasters(filename, time, rasters, stat, true))
        return false;

    if (rasters.size() == 0)
        return false;

    ApplyTransformations(stat, rasters, parms);

    IMG_FileParms file_parms;
    file_parms.setColorModel(IMG_RGBA);
    file_parms.setComponentOrder(theWorkFormat::img_component_order);
    file_parms.setDataType(theWorkFormat::img_data_format);
    file_parms.setOption("quality", std::to_string(parms.quality).c_str());

    IMG_File *file = IMG_File::create(os, stat, &file_parms, format);

    if (!file)
        return false;

    UT_Array<PXL_Raster *> ptr_rasters;
    for (auto raster : rasters)
        ptr_rasters.append(raster.get());

    if (!file->writeImages(ptr_rasters))
    {
        file->close();
        return false;
    }

    file->close();
    return true;
}

bool
ROP_GLTF_Image::CreateMappedTexture(
    const UT_Array<ROP_GLTF_ChannelMapping> &mappings, std::ostream &os,
    const IMG_Format *format, uint32 time, const ROP_GLTF_ImgExportParms &parms,
    ROP_GLTF_BaseErrorManager &errormgr)
{
    if (mappings.size() == 0)
        return false;

    UT_Array<UT_Array<UT_SharedPtr<PXL_Raster>>> rasters;

    for (const ROP_GLTF_ChannelMapping &mapping : mappings)
    {
        UT_Array<UT_SharedPtr<PXL_Raster>> curmap_rasters;

        IMG_Stat stat;
        if (GetImageRasters(mapping.path, time, curmap_rasters, stat, false))
        {
            rasters.append(std::move(curmap_rasters));
        }
        else if(mapping.path != "")
        {
            UT_String error("Invalid texture specified.");
            error.append(mapping.path);
            errormgr.AddWarning(UT_ERROR_MESSAGE, error);
        }
    }

    if (rasters.size() == 0)
        return false;

    // Find the size of the largest raster
    int xres = 0;
    int yres = 0;
    for (auto it = rasters.begin(); it != rasters.end(); it++)
    {
        for (UT_SharedPtr<PXL_Raster> raster : *it)
        {
            xres = std::max(static_cast<int>(raster->getXres()), xres);
            yres = std::max(static_cast<int>(raster->getYres()), yres);
        }
    }

    auto new_raster = UT_SharedPtr<PXL_Raster>(
        new PXL_Raster(PACK_RGB,
                       theWorkFormat::px_data_format, xres, yres));

    new_raster->clear();

    PXL_FillParms fill_parms;
    fill_parms.setSourceType(theWorkFormat::px_data_format);
    fill_parms.setDestType(theWorkFormat::px_data_format);
    fill_parms.setDestArea(0, 0, xres - 1, yres - 1);
    fill_parms.setSourceArea(0, 0, xres - 1, yres - 1);
    fill_parms.mySInc = 3;
    fill_parms.myDInc = 3;

    exint idx = 0;
    exint num_channels = 0;

    for (auto it = rasters.begin(); it != rasters.end(); it++)
    {
        const UT_Array<UT_SharedPtr<PXL_Raster>> &planes = *it;
	if (planes.size() == 0 ||
	    planes[0]->getPacking() != PACK_RGB)
	{
	    continue;
	}

	auto from_channel = mappings[idx].from_channel;
	auto to_channel = mappings[idx].to_channel;

        fill_parms.mySource = planes[0]->getPixel(0, 0, from_channel);
        fill_parms.myDest = new_raster->getPixel(0, 0, to_channel);

        PXL_Fill::fill(fill_parms);

        num_channels++;
        idx++;
    }

    if (num_channels == 0)
        return false;

    IMG_Stat stat(xres, yres, theWorkFormat::img_data_format, IMG_RGB);

    // Apply transformations
    UT_Array<UT_SharedPtr<PXL_Raster>> planes { new_raster };

    ApplyTransformations(stat, planes, parms);

    new_raster = planes[0];

    // Save the file to our stream
    IMG_File *file;

    IMG_FileParms file_parms;
    file_parms.setColorModel(IMG_RGB);
    file_parms.setComponentOrder(theWorkFormat::img_component_order);
    file_parms.orientImage(IMG_ORIENT_LEFT_FIRST, IMG_ORIENT_BOTTOM_FIRST);
    file_parms.setInterleaved(IMG_INTERLEAVED);
    file_parms.setDataType(theWorkFormat::img_data_format);
    file_parms.setOption("quality", std::to_string(parms.quality).c_str());

    file = IMG_File::create(os, stat, &file_parms, format);

    UT_Array<PXL_Raster *> new_raster_arr {new_raster.get()};
    if (!file->writeImages(new_raster_arr))
    {
        file->close();
        return false;
    }
    file->close();

    return true;
}

bool
ROP_GLTF_Image::OutputImage(const UT_String &filename, const IMG_Format *format,
                            std::ostream &os, fpreal time,
                            const ROP_GLTF_ImgExportParms &parms,
                            ROP_GLTF_BaseErrorManager &errormgr)
{
    // Special case for not creating errors, since some HDAs may 
    // enable the texture parameter but not actually specify a texture
    if (filename == "")
        return false;

    if (filename.startsWith("op:"))
    {
        OP_Node *node = OPgetDirector()->findNode(filename);
        if (!node)
            return false;

        COP2_Node *copnode = node->castToCOP2Node();

        if (!copnode)
            return false;

        return OutputCopToStream(copnode, format, os, time, parms);
    }

    return OutputImageToStream(filename, format, os, time, parms);
}

bool
ROP_GLTF_Image::GetImageRasters(const UT_StringHolder &filename,
                                const uint32 time,
                                UT_Array<UT_SharedPtr<PXL_Raster>> &rasters,
                                IMG_Stat &stat, bool include_alpha)
{
    if (filename.isEmpty())
        return false;

    if (filename.startsWith("op:"))
    {
        OP_Node *node = OPgetDirector()->findNode(filename);
        if (!node)
            return false;

        COP2_Node *copnode = node->castToCOP2Node();

        if (!copnode)
            return false;

        if (!GetImageRastersFromCOP(copnode, time, rasters, stat, include_alpha))
            return false;

        return true;
    }

    if (!GetImageRastersFromFile(filename, time, rasters, stat, include_alpha))
        return false;

    return true;
}

bool
ROP_GLTF_Image::GetImageRastersFromFile(
    const UT_StringHolder &filename, const uint32 time,
    UT_Array<UT_SharedPtr<PXL_Raster>> &rasters, IMG_Stat &stat, 
    bool include_alpha)
{
    IMG_ColorModel img_model = IMG_RGB;
    if (include_alpha)
    {
        img_model = IMG_RGBA;
    }


    IMG_FileParms img_parms;
    img_parms.setColorModel(img_model);
    img_parms.setComponentOrder(theWorkFormat::img_component_order);
    img_parms.setInterleaved(IMG_INTERLEAVED);
    img_parms.setDataType(theWorkFormat::img_data_format);
    img_parms.selectPlaneNames("C");

    IMG_File *file = IMG_File::open(filename, &img_parms);

    if (file == nullptr)
        return false;

    UT_Array<PXL_Raster *> ptr_rasters;
    if (!file->readImages(ptr_rasters))
    {
        file->close();
        return false;
    }

    file->close();

    for (auto raster : ptr_rasters)
        rasters.append(UT_SharedPtr<PXL_Raster>(raster));

    stat = file->getStat();

    return true;
}

bool
ROP_GLTF_Image::GetImageRastersFromCOP(
    COP2_Node *node, const uint32 time,
    UT_Array<UT_SharedPtr<PXL_Raster>> &rasters,
    IMG_Stat &stat, 
    bool include_alpha)
{
    IMG_ColorModel img_model = IMG_RGB;
    PXL_Packing pxl_model = PACK_RGB;
    if (include_alpha)
    {
        img_model = IMG_RGBA;
        pxl_model = PACK_RGBA;
    }


    COP2_ImageSource *is = node->getImageSource();
    OP_Context context(time);

    if (!is)
        return false;

    short key;
    if (!is->open(key))
        return false;

    // Automatically close the image fstream when exiting the function
    std::unique_ptr<COP2_ImageSource, std::function<void(COP2_ImageSource *)>>
        image_source(std::move(is), [&](COP2_ImageSource *s) { s->close(key); });

    //  Handle color plane
    const TIL_Sequence *image_seq = image_source->getSequence(0.f);
    if (!image_seq)
        return false;

    TIL_Sequence seq = *image_seq;
    seq.setSingleImage(1);
    seq.setStart(1);
    seq.setLength(1);

    int xres, yres;
    seq.getRes(xres, yres);
    context.setRes(xres, yres);

    // Handle colour plane
    const TIL_Plane *const_color_plane =
        seq.getPlane(COP2_Node::getColorPlaneName());

    UT_SharedPtr<TIL_Plane> color_plane;
    if (const_color_plane)
    {
        color_plane = UT_SharedPtr<TIL_Plane>(new TIL_Plane(*const_color_plane));
        color_plane->setScoped(1);
    }

    UT_SharedPtr<TIL_Raster> color_raster;

    stat = IMG_Stat(xres, yres, theWorkFormat::img_data_format, img_model);

    if (color_plane)
    {
        color_raster = UT_SharedPtr<TIL_Raster>(
	    new TIL_Raster(pxl_model,
                           theWorkFormat::px_data_format, xres, yres),
			   [=](TIL_Raster *r){}
	    );

	// Pack both A and C planes into a single raster
	if (!is->getImage(color_raster.get(), time, xres, yres, *color_plane, 0,
	    0, 0, xres-1 , yres -1, 1.0, true))
	{
	    return false;
	}
	
	rasters.append(color_raster);
    }

    return true;
}

void
ROP_GLTF_Image::ApplyTransformations(
    IMG_Stat &stat, UT_Array<UT_SharedPtr<PXL_Raster>> &rasters,
    const ROP_GLTF_ImgExportParms &parms)
{
    if (parms.flipGreen)
    {
        for (auto raster : rasters)
        {
            auto packing = raster->getPacking();
            if (packing != PACK_RGB && packing != PACK_RGBA &&
                packing != PACK_RGB_NI && packing != PACK_RGBA_NI)
            {
                // We're not handling a RGB(A) image, time to give up
                continue;
            }

	    exint inc = 1;
            if (packing == PACK_RGB)
                inc = 3;
            else if (packing == PACK_RGBA)
                inc = 4;
	    // Otherwise the raster is interleaved so the increment is 1

            PXL_FillParms fill_parms;
            fill_parms.setDestType(raster->getFormat());
            fill_parms.setDestArea(0, 0, stat.getXres() - 1, stat.getYres() - 1);
            fill_parms.myDInc = inc;
            fill_parms.myDest = raster->getPixel(0, 0, 1);
            fill_parms.myFillColor = 1;
            PXL_Fill::invert(fill_parms);
        }
    }

    auto xres = rasters[0]->getXres();
    auto yres = rasters[0]->getYres();
    if (parms.roundUpPowerOfTwo || xres > parms.max_res || yres > parms.max_res)
    {
        xres = std::min(NextPowerOfTwo(xres), parms.max_res);
	yres = std::min(NextPowerOfTwo(yres), parms.max_res);
        for (exint idx = 0; idx < rasters.size(); idx++)
        {
            auto dest = UT_SharedPtr<PXL_Raster>(new PXL_Raster());
            TIL_Raster::scaleRasterToSize(dest.get(), rasters[idx].get(), xres, yres);
            rasters[idx] = dest;
        }

        stat.setResolution(xres, yres);
    }
}

exint
ROP_GLTF_Image::NextPowerOfTwo(exint num)
{
    return static_cast<exint>(
        SYSpow(2, SYSceil(log2(static_cast<fpreal64>(num)))));
}

bool
ROP_GLTF_Image::OutputCopToStream(COP2_Node *node, const IMG_Format *format,
                                  std::ostream &os, fpreal time,
                                  const ROP_GLTF_ImgExportParms &parms)
{
    UT_Array<UT_SharedPtr<PXL_Raster>> rasters;
    IMG_Stat stat;

    if (!GetImageRastersFromCOP(node, time, rasters, stat, true))
        return false;

    if (rasters.size() == 0)
        return false;

    ApplyTransformations(stat, rasters, parms);

    // glTF specifies that texel alpha values should not be premultiplied
    IMG_FileParms img_parms;
    img_parms.setColorModel(IMG_RGBA);
    img_parms.setComponentOrder(theWorkFormat::img_component_order);
    img_parms.setInterleaved(IMG_INTERLEAVED);
    img_parms.setDataType(theWorkFormat::img_data_format);

    if (!format->formatStoresColorSpace())
        img_parms.adjustGammaForFormat(stat, format, IMG_DT_ANY);

    IMG_File *file = IMG_File::create(os, stat, &img_parms, format, 0, true);

    // Recreate the array of dumb pointers to interface with old code
    UT_Array<PXL_Raster *> ptr_rasters;

    for (auto sp_raster : rasters)
        ptr_rasters.append(sp_raster.get());

    if (!file->writeImages(ptr_rasters))
    {
        file->close();
        return false;
    }

    file->close();
    return true;
}

bool
ROP_GLTF_ChannelMapping::operator<(const ROP_GLTF_ChannelMapping &r) const
{
    if (path < r.path)
        return true;
    if (path > r.path)
        return false;

    if (from_channel < r.from_channel)
        return true;
    if (from_channel > r.from_channel)
        return false;

    if (to_channel < r.to_channel)
        return true;
    if (to_channel > r.to_channel)
        return false;

    return false;
}
