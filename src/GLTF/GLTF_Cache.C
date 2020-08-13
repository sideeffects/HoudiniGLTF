/*
* Copyright (c) 2020
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
#include "GLTF_Cache.h"
#include "GLTF_Loader.h"

using namespace GLTF_NAMESPACE;

static UT_Lock theThreadLock;
const static exint MAX_CACHE_FILES = 5;

GLTF_Cache&
GLTF_Cache::GetInstance()
{
    static GLTF_Cache theCache;

    return(theCache);
}

const UT_SharedPtr<const GLTF_Loader>
GLTF_Cache::GetLoader(const UT_StringHolder &path)
{
    UT_AutoLock lock(theThreadLock);

    auto loader = myLoaderMap.find(path);
    if (loader == myLoaderMap.end())
    {
        return UT_SharedPtr<GLTF_Loader>(nullptr);
    }
    return loader->second;
}

void
GLTF_Cache::AutomaticEvict()
{
    UT_AutoLock lock(theThreadLock);

    if (myLoaderMap.size() == 0)
        return;

    // todo: replace this with a priority queue for LRU eviction
    myLoaderMap.erase(myLoaderMap.begin());
}

bool GLTF_Cache::EvictLoader(const UT_StringHolder& path)
{
    UT_AutoLock lock(theThreadLock);

    return (myLoaderMap.erase(path) > 0);
}

const UT_SharedPtr<const GLTF_Loader>
GLTF_Cache::LoadLoader(const UT_StringHolder &path)
{
    UT_AutoLock lock(theThreadLock);

    auto loader = GetLoader(path);
    if (!loader)
    {
        if (myLoaderMap.size() > MAX_CACHE_FILES)
        {
            AutomaticEvict();
        }

        auto new_loader = UTmakeShared<GLTF_Loader>(UT_String(path));

        // If loading fails, then return an empty object
        if (!new_loader->Load())
        {
            return UT_SharedPtr<GLTF_Loader>(nullptr);
        }
        else
        {
            myLoaderMap.emplace(path, loader);
        }

        return new_loader;
    }

    return loader;
}
