/*
 * Copyright (c) COPYRIGHTYEAR
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
 *
 *----------------------------------------------------------------------------
 */

#ifndef __SOP_GLTFCACHE_H__
#define __SOP_GLTFCACHE_H__

#include "GLTF_API.h"

#include <UT/UT_Lock.h>
#include <UT/UT_Map.h>
#include <UT/UT_SharedPtr.h>

namespace GLTF_NAMESPACE
{

class GLTF_Loader;

///
/// A singleton responsible for storing a cached GLTF_Loader.
///
class GLTF_API GLTF_Cache
{
public:
    static GLTF_Cache &GetInstance();

    ///
    /// Creates a new loader with the given filepath, calls Load()
    /// and returns a pointer.  If we were unable to load, then
    /// the loader is evicted and null is returned
    ///
    const UT_SharedPtr<const GLTF_Loader> LoadLoader(const UT_StringHolder &path);

    // Removes the loader from the cache, does not destroy any existing
    // instances as loaders are UT_SharedPtr's
    bool EvictLoader(const UT_StringHolder &path);

private:
    // Gets an existing loader from the cache, returns false
    // if the loader does not exist
    const UT_SharedPtr<const GLTF_Loader> GetLoader(const UT_StringHolder &path);


    void AutomaticEvict();

    UT_Map<UT_StringHolder, UT_SharedPtr<const GLTF_Loader>> myLoaderMap;
};

} // end GLTF_NAMESPACE

#endif
