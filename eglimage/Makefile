# Makefile

# Set the target filesystem directory
CROSS_COMPILE=arm-linux-gnueabihf-
FSDIR=/home1/anand/targetfs/ti-glsdk_dra7xx-evm_7_02_00_02

## No need to change below this

COMMON_INCLUDES = -I$(FSDIR)/usr/include/libdrm -I$(FSDIR)/usr/include -I$(FSDIR)/usr/include/gbm
COMMON_LFLAGS = -L$(FSDIR)/usr/lib -Wl,--rpath-link,$(FSDIR)/usr/lib

PLAT_CPP = $(CROSS_COMPILE)g++

PLAT_CFLAGS   = $(COMMON_INCLUDES) -Wall -O2
PLAT_LINK =  $(COMMON_LFLAGS) -lEGL -lGLESv2 -lgbm -ldrm -ludev

SRCNAME = $(wildcard *.c)
SRCNAME += $(wildcard *.cpp)
OBJECTS = $(SRCNAME:.c=.o)
OBJECTS += $(SRCNAME:.cpp=.o)

OUTNAME = eglimage_sample

$(OUTNAME): $(SRCNAME)
	$(PLAT_CPP) -o $@ $^ $(PLAT_CFLAGS) $(LINK) $(PLAT_LINK)

clean:
	rm $(OUTNAME)
