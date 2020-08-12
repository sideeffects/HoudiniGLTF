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

#ifndef __ROP_GLTF_IMAGE_h__
#define __ROP_GLTF_IMAGE_h__

#include <SYS/SYS_Types.h>
#include <UT/UT_Array.h>
#include <UT/UT_ArraySet.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_UniquePtr.h>
#include <UT/UT_SharedPtr.h>
#include <iosfwd>

class IMG_Format;
class COP2_Node;
class UT_String;
class PXL_Raster;
class IMG_Stat;
class ROP_GLTF_BaseErrorManager;

struct ROP_GLTF_ChannelMapping
{
    // This implemented so we can obtain a lexiographic ordering
    // of ChannelMaps in order to cache them in a map
    bool operator<(const ROP_GLTF_ChannelMapping &r) const;

    UT_StringHolder path;
    exint from_channel;
    exint to_channel;
};

struct ROP_GLTF_ImgExportParms
{
    bool roundUpPowerOfTwo = false;
    // 0 indicates that there is no max raster size
    exint maxRasterSize = 0;
    exint quality = 90;
    exint max_res = 0;
    bool flipGreen = false;
};

///
/// Utility functions for importing, exporting and manipulating images in
/// the context of GLTF textures.
///

class ROP_GLTF_Image
{
public:
    ///
    /// Takes a list of images and associated channels, packs them into
    /// a single image file, preprocesses the image and outputs it to &os
    ///
    static bool
    CreateMappedTexture(const UT_Array<ROP_GLTF_ChannelMapping> &mappings,
                        std::ostream &os, const IMG_Format *format, uint32 time,
                        const ROP_GLTF_ImgExportParms &parms,
                        ROP_GLTF_BaseErrorManager &errormgr);

    ///
    /// Converts the file format for the given images, processes it for GLTF
    /// and outputs it to the output stream &os.
    ///
    static bool OutputImage(const UT_String &filename, const IMG_Format *format,
                            std::ostream &os, fpreal time,
                            const ROP_GLTF_ImgExportParms &parms,
                            ROP_GLTF_BaseErrorManager &errormgr);

private:
    //
    // Gets the image rasters from the given File or COP.  Any methods
    // with "include_alpha" will return RGBA if the flag is on, or otherwise
    // RGB.  This is to avoid handling other packings.
    //
    static bool
    GetImageRasters(const UT_StringHolder &filename, const uint32 time,
                    UT_Array<UT_SharedPtr<PXL_Raster>> &rasters,
                    IMG_Stat &stat, bool include_alpha);

    static exint NextPowerOfTwo(exint num);

    static bool 
    OutputCopToStream(COP2_Node *node, const IMG_Format *format,
                                  std::ostream &os, fpreal time,
                                  const ROP_GLTF_ImgExportParms &parms);

    static bool
    OutputImageToStream(const UT_String &filename,
                        const IMG_Format *file_format, std::ostream &os,
                        fpreal time, const ROP_GLTF_ImgExportParms &parms);

    static void
    ApplyTransformations(IMG_Stat &stat,
                         UT_Array<UT_SharedPtr<PXL_Raster>> &rasters,
                         const ROP_GLTF_ImgExportParms &parms);

    static bool
    GetImageRastersFromFile(const UT_StringHolder &filename, const uint32 time,
                            UT_Array<UT_SharedPtr<PXL_Raster>> &rasters,
                            IMG_Stat &stat, bool include_alpha);

    static bool
    GetImageRastersFromCOP(COP2_Node *node, const uint32 time,
                           UT_Array<UT_SharedPtr<PXL_Raster>> &rasters,
                           IMG_Stat &stat, bool include_alpha);
};

#endif
