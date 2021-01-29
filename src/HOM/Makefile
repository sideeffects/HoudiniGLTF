# The HFS Environment variable needs to be set before calling make
# Windows users should also define their MSVCDir environnment variable

include ../CustomGLTF.global

DSONAME = $(HOM_LIB).$(EXT)

# Custom GLTF library
CUSTOM_GLTF = ".."

SOURCES = HOM_GLTF.C

INCDIRS = \
	-I$(CUSTOM_GLTF) \
	-I$(HFS)/toolkit/include \
    -I$(HFS)/python27/include

ifdef WINDOWS
LIBDIRS += -LIBPATH:$(CUSTOM_GLTF)/GLTF -LIBPATH:$(HFS)/python27/libs
LIBS += lib$(GLTFLIB).lib
else
LIBDIRS += -L$(CUSTOM_GLTF)/GLTF -L$(HFS)/python27/libs
LIBS += -l$(GLTFLIB)
endif

include $(HFS)/toolkit/makefiles/Makefile.gnu

HDEFINES += \
	-DGLTF_EXPORTS \
	-DGLTF_NAMESPACE=$(GLTFNAMESPACE)
