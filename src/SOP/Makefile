# The HFS Environment variable needs to be set before calling make
# Windows users should also define their MSVCDir environnment variable

include ../CustomGLTF.global

DSONAME = $(SOPLIB).$(EXT)

# Custom GLTF library
GHEADER = "../GLTF"
GLIB = "../GLTF"

SOURCES = SOP_GLTF.C

INCDIRS = \
    -I$(GHEADER) \
    -I$(HFS)/toolkit/include

ifdef WINDOWS
LIBDIRS += -LIBPATH:$(GLIB)
LIBS += lib$(GLTFLIB).lib
else
LIBDIRS += -L$(GLIB)
LIBS += -l$(GLTFLIB)
endif

include $(HFS)/toolkit/makefiles/Makefile.gnu

HDEFINES += \
	-DGLTF_EXPORTS \
	-DGLTF_NAMESPACE=$(GLTFNAMESPACE) \
	-DCUSTOM_GLTF_TOKEN_PREFIX='$(TOKEN_PREFIX)' \
	-DCUSTOM_GLTF_LABEL_PREFIX='$(LABEL_PREFIX)'
