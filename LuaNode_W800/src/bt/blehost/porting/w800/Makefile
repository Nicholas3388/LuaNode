TOP_DIR = ../../../../..
sinclude $(TOP_DIR)/tools/w800/conf.mk

ifndef PDIR
GEN_LIBS = libw800porting$(LIB_EXT)
COMPONENTS_libw800porting = src/libwmw800porting$(LIB_EXT)
endif

#DEFINES +=

sinclude $(TOP_DIR)/tools/w800/rules.mk

INCLUDES := $(INCLUDES) -I ./ -I $(PDIR)include
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile