This contains the source code for the glTF importer and exporter for Houdini.

This project has been setup to produce custom glTF libraries with its own 
namespace and node operator names so that it can be used alongside the default
Houdini glTF libarries.

Optionally, you can modify the custom library namespace and node operator names
in CustomGLTF.global.

This project has been setup to build with make. See
https://www.sidefx.com/docs/hdk/_h_d_k__intro__compiling.html#HDK_Intro_Compiling_Makefiles

-------------------------------------------------------------------------------
Libraries:
-------------------------------------------------------------------------------

- GLTF/
    Source code for the core glTF import and export library which contains most 
    of the parsing and writing code. The generated libCustomGLTF.so/.dll should be
    placed in a path pointed to by LD_LIBRARY_PATH or PATH (on Windows).

- SOP/
    Source code for the glTF SOP importer node. The generated SOP_CustomGLTF.so/.dll
    should be placed in %HOME%/houdiniX.X/dso or a path pointed to by HOUDINI_DSO_PATH.

- ROP/
    Source code for the glTF ROP exporter node. The generated ROP_CustomGLTF.so/.dll
    should be placed in %HOME%/houdiniX.X/dso or a path pointed to by HOUDINI_DSO_PATH.

- HOM/
    Source code for the glTF HOM module. The generated HOM_CustomGLTF.so/.dll
    should be placed in %HOME%/houdiniX.X/dso or a path pointed to by HOUDINI_DSO_PATH.

-------------------------------------------------------------------------------
Building the glTF libraries on Windows using Cygwin:
-------------------------------------------------------------------------------

To build the glTF libraries on Windows the HFS environment variable needs to be 
set to the Houdini install directory, and the MSVCDir environment must be set 
to the VC subdirectory of your Visual Studio installation.

To do so, first open a cygwin shell and go to the Houdini X.Y.ZZZ directory and 
source the Houdini environment:

cd C:/Program\ Files/Side\ Effects\ Software/Houdini\ X.Y.ZZZ
source ./houdini_setup

Then set the MSVC dir.
export MSVCDir="C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Tools/MSVC/14.16.27023"

Then go to the glTF source folder.

- To build all libs: make WINDOWS=1 all

- To build a specific lib (such as libCustomGLTF.dll): make WINDOWS=1 gltf

- To clean all: make WINDOWS=1 clean

Note that libCustomGLTF.dll must be built first. The other libs depend on it.

Once the libraries are built, add the location of libCustomGLTF.dll to PATH:

set PATH=%PATH%;C:\customgltf\src\GLTF

Then copy over the other built .dlls to %HOME%/houdiniX.X/dso (i.e. Houdini home directory).

-------------------------------------------------------------------------------
Building the glTF libraries on Linux or macOS:
-------------------------------------------------------------------------------

First, make sure the HFS environment variable is properly set to your Houdini 
install directory.

    Linux: export HFS=/opt/hfs18.0.300
    macOS: export HFS=/Applications/Houdini/Houdini18.5.301/Framedworks/Houdini.framework/Versions/18.5/Resources

Then go to the glTF source folder.

    Linux: make all
    macOS: make MBSD=1 all

Note that libCustomGLTF.so/.dylib must be built first. The other libs depend on it.

Once the libraries are built, add the location of libCustomGLTF.so/.dylib to LD_LIBRARY_PATH:

     Linux: export LD_LIBRARY_PATH=/dev/customgltf/src/GLTF
     macOS: export DYLD_LIBRARY_PATH=/dev/customgltf/src/GLTF

Then copy over the other built .so/.dylib libs to %HOME%/houdiniX.X/dso 
(i.e. Houdini home directory).

On macOS, Houdini home directory is located at ~/Library/Preferences/houdini/X.X/