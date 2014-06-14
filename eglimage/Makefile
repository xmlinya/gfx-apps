# Makefile

# Set the target filesystem directory
CROSS_COMPILE=arm-linux-gnueabihf-
FSDIR=/home1/anand/targetfs/ti-glsdk_dra7xx-evm_6_04_00_02

## No need to change below this

COMMON_INCLUDES = -I$(FSDIR)/usr/include/libdrm -I$(FSDIR)/usr/include -I./include/gbm
COMMON_LFLAGS = -L$(FSDIR)/usr/lib -Wl,--rpath-link,$(FSDIR)/usr/lib -L$(FSDIR)/lib -Wl,--rpath-link,$(FSDIR)/lib

PLAT_CPP = $(CROSS_COMPILE)g++

PLAT_CFLAGS   = $(COMMON_INCLUDES) -Wall -O2
PLAT_LINK =  $(COMMON_LFLAGS) -lEGL -lGLESv2 -lgbm -ldrm

SRCNAME = $(wildcard *.c)
SRCNAME += $(wildcard *.cpp)
OBJECTS = $(SRCNAME:.c=.o)
OBJECTS += $(SRCNAME:.cpp=.o)

OUTNAME = eglimage_sample

$(OUTNAME): $(SRCNAME)
	$(PLAT_CPP) -o $@ $^ $(PLAT_CFLAGS) $(LINK) $(PLAT_LINK)

clean:
	rm $(OUTNAME)
