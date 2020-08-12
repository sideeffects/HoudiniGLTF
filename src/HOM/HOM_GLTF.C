/*
 * Copyright (c) COPYRIGHTYEAR
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
#include <Python.h>

#include <PY/PY_CPythonAPI.h>
// This file contains functions that will run arbitrary Python code
#include <PY/PY_InterpreterAutoLock.h>
#include <PY/PY_Python.h>

#include <HOM/HOM_Module.h>
#include <UT/UT_DSOVersion.h>

#include <GLTF/GLTF_Cache.h>
#include <GLTF/GLTF_Util.h>

static const char *Doc_GLTFClearCache = "Doc_GLTFClearCache(gltfPath)\n";

using namespace GLTF_NAMESPACE;

static PY_PyObject *
Py_ClearGLTFCache(PY_PyObject *self, PY_PyObject *file)
{
    const char *file_str;
    if (!PY_PyArg_ParseTuple(file, "s", &file_str))
    {
        PY_Py_RETURN_NONE;
    }

    GLTF_Cache::GetInstance().EvictLoader(file_str);
    PY_Py_RETURN_NONE;
}

static const char *Doc_GLTFGetSceneList = "Doc_GLTFGetSceneNames(filename)\n"
                                          "\n";

PY_PyObject *
Py_GLTFGetSceneList(PY_PyObject *self, PY_PyObject *file)
{
    const char *file_str;
    if (!PY_PyArg_ParseTuple(file, "s", &file_str))
    {
        PY_Py_RETURN_NONE;
    }

    UT_Array<UT_String> scene_list = GLTF_Util::getSceneList(file_str);

    exint num_scenes = scene_list.size();
    PY_PyObject *result = PY_PyList_New(num_scenes * 2);

    for (exint i = 0; i < num_scenes; ++i)
    {
        std::string scene_name = scene_list[i].c_str();
        if (scene_name == "")
        {
            scene_name = "Scene " + std::to_string(i + 1);
        }

        std::string index_string = std::to_string(i);
        PY_PyObject *pyname = PY_PyString_FromString(scene_name.c_str());
        PY_PyObject *py_scene_idx = PY_PyString_FromString(index_string.c_str());
        PY_PyList_SetItem(result, 2 * i, py_scene_idx);
        PY_PyList_SetItem(result, 2 * i + 1, pyname);
    }

    return result;
}

#if defined(WIN32)
PyMODINIT_FUNC
#elif PY_MAJOR_VERSION >= 3
// This is a replacement of PyMODINIT_FUNC but with the default visibility
// attribute declaration injected in the middle.
extern "C" __attribute__((visibility("default"))) PyObject*
#else
PyMODINIT_FUNC __attribute__((visibility("default")))
#endif

#if PY_MAJOR_VERSION >= 3
PyInit__gltf_hom_extensions(void)
#else
init_gltf_hom_extensions(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PY_PyObject *module = nullptr;
#endif

    {
        // A PY_InterpreterAutoLock will grab the Python global interpreter
        // lock (GIL).  It's important that we have the GIL before making
        // any calls into the Python API.
        PY_InterpreterAutoLock interpreter_auto_lock;

        static PY_PyMethodDef gltf_hom_extension_methods[] = {
            {"gltfClearCache", Py_ClearGLTFCache, PY_METH_VARARGS(),
             Doc_GLTFClearCache},

            {"gltfGetSceneList", Py_GLTFGetSceneList, PY_METH_VARARGS(),
             Doc_GLTFGetSceneList},

            {NULL, NULL, 0, NULL}};

#if PY_MAJOR_VERSION >= 3
	module =
#endif
	PY_Py_InitModule("_gltf_hom_extensions", gltf_hom_extension_methods);
    }

#if PY_MAJOR_VERSION >= 3
    return reinterpret_cast<PyObject *>(module);
#endif
}
