# The HFS Environment variable needs to be set before calling make
# Windows users should also define their MSVCDir environnment variable

include ../CustomGLTF.global

DSONAME = lib$(GLTFLIB).$(EXT)

SOURCES = \
    GLTF_Cache.C \
    GLTF_Loader.C \
    GLTF_GeoLoader.C \
    GLTF_Types.C \
    GLTF_Util.C

INCDIRS = \
	-I$(HFS)/toolkit/include

include $(HFS)/toolkit/makefiles/Makefile.gnu

HDEFINES += \
	-DGLTF_EXPORTS \
	-DGLTF_NAMESPACE=$(GLTFNAMESPACE)

ifndef WINDOWS
# Additional Houdini libs
LIBDIRS += -L$(HFS)/dsolib
LIBS += -lHoudiniGEO -lHoudiniUT
endif

# Change from -bundle for core lib
ifdef MBSD
SHAREDFLAG = -dynamiclib
endif
