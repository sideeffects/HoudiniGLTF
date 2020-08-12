# The HFS Environment variable needs to be set before calling make
# Windows users should also define their MSVCDir environnment varaibale

.PHONY: gltf sop rop hom all clean


gltf:
	@$(MAKE) -C GLTF

sop: gltf
	@$(MAKE) -C SOP

rop: gltf
	@$(MAKE) -C ROP

hom: gltf
	@$(MAKE) -C HOM

all: gltf sop rop hom

clean:
	@$(MAKE) -C GLTF clean
	@$(MAKE) -C SOP clean
	@$(MAKE) -C ROP clean
	@$(MAKE) -C HOM clean