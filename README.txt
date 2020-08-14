This contains the source code for the glTF importer and exporter for Houdini.

This project has been setup to produce custom glTF libraries with its own 
namespace and node operator names so that it can be used alongside the default
Houdini glTF libarries.

Optionally, you can modify the custom library namespace and node operator names
in src/CustomGLTF.global.

This project has been setup to build with make. See
https://www.sidefx.com/docs/hdk/_h_d_k__intro__compiling.html#HDK_Intro_Compiling_Makefiles

For coding reference, see the Houdini Development Kit: https://www.sidefx.com/docs/hdk/

Requires Houdini 18.0.552 or newer. Does not work with earlier versions of Houdini.

-------------------------------------------------------------------------------
Libraries:
-------------------------------------------------------------------------------

- src/GLTF
    Source code for the core glTF import and export library which contains most 
    of the parsing and writing code. The generated libCustomGLTF.so/.dll should be
    placed in a path pointed to by LD_LIBRARY_PATH (Linux), or DYLD_LIBRARY_PATH
    (macOS) or PATH (Windows).

- src/SOP
    Source code for the glTF SOP importer node. The generated SOP_CustomGLTF.so/.dll
    should be placed in $HOME/houdiniX.X/dso or a path pointed to by HOUDINI_DSO_PATH.

- src/ROP
    Source code for the glTF ROP exporter node. The generated ROP_CustomGLTF.so/.dll
    should be placed in $HOME/houdiniX.X/dso or a path pointed to by HOUDINI_DSO_PATH.

- src/HOM
    Source code for the glTF HOM module. The generated HOM_CustomGLTF.so/.dll
    should be placed in $HOME/houdiniX.X/dso or a path pointed to by HOUDINI_DSO_PATH.

- src/gltf_hierarchy.hda
    The Object node to load in a glTF scene hierarchy, as a Houdini Digital Asset.
    This should be placed $HOME/houdiniX.X/otls.

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

Then set the MSVC dir (change to your version of MSVC).
export MSVCDir="C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional/VC/Tools/MSVC/14.16.27023"

The following steps assume you cloned this repo to C:\HoudiniGLTF

Go to the HoudiniGLTF\src folder. Then do one of the following:

    To build all libs: make WINDOWS=1 all

Note that libCustomGLTF.dll must be built first. The other libs depend on it.

Once the libraries are built, add the location of libCustomGLTF.dll to PATH:

set PATH=%PATH%;C:\HoudiniGLTF\src\GLTF

Then copy over the built SOP, ROP, and HOM .dlls to %HOME%/houdiniX.X/dso 
(i.e. Houdini home directory).

Run Houdini and you should see the Custom GLTF SOP and ROP nodes.

Notes:

To build a specific lib (such as libCustomGLTF.dll): make WINDOWS=1 gltf

To clean the built files: make WINDOWS=1 clean

Troubleshooting:

If you get compiler errors about Windows libraries such as 'corecrt.h', change 
the WIN32_KIT_VERSION entry in Houdini 18.0.532\toolkit\makefiles\Makefile.win 
to the one installed on your system in C:\Program Files (x86)\Windows Kits\10\Lib. 
For example: WIN32_KIT_VERSION = 10.0.17763.0

If either the SOP or ROP libraries are not loading or the custom nodes are not 
showing up, set HOUDINI_DSO_ERROR=3 environment variable, and launch Houdini. 
There should be log entries when these libraries are loaded.

-------------------------------------------------------------------------------
Building the glTF libraries on Linux or macOS:
-------------------------------------------------------------------------------

First, make sure the HFS environment variable is properly set to your Houdini 
install directory.

    Linux: export HFS=/opt/hfsX.Y.ZZZ
    macOS: export HFS=/Applications/Houdini/HoudiniX.Y.ZZZ/Frameworks/Houdini.framework/Versions/X.Y/Resources

The following steps assume you closed this repo to /dev/HoudiniGLTF

Go to the HoudiniGLTF/src/ folder. And execute the following:

    Linux: make all
    macOS: make MBSD=1 all

Note that libCustomGLTF.so/.dylib must be built first. The other libs depend on it.

Once the libraries are built, add the location of libCustomGLTF.so/.dylib to LD_LIBRARY_PATH:

     Linux: export LD_LIBRARY_PATH=/dev/HoudiniGLTF/src/GLTF
     macOS: export DYLD_LIBRARY_PATH=/dev/HoudiniGLTF/src/GLTF

Then copy over the built SOP, ROP, and HOM .so/.dylib libs to $HOME/houdiniX.X/dso 
(i.e. Houdini home directory).

On macOS, Houdini home directory is located at ~/Library/Preferences/houdini/X.Y/

If either the SOP or ROP libraries are not loading or the custom nodes are not 
showing up, set HOUDINI_DSO_ERROR=3 environment variable, and launch Houdini. 
There should be log entries when these libraries are loaded.