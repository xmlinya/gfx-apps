# Makefile

# Set the cross compiler and target file system path
CROSS_COMPILE=arm-linux-gnueabihf-

## No need to change below this

COMMON_INCLUDES = -I$(FSDIR)/usr/include
COMMON_LFLAGS = -L$(FSDIR)/usr/lib -Wl,--rpath-link,$(FSDIR)/usr/lib

PLAT_CPP = $(CROSS_COMPILE)gcc

PLAT_CFLAGS   = $(COMMON_INCLUDES) -g
PLAT_LINK =  $(COMMON_LFLAGS) -lEGL -lGLESv2 -ludev -lpthread -lm -lrt

SRCNAME = esTransform.c \
	gl_kmscube.c \
	main.c \
	render_thread.c \

BASE_OUTNAME = egl_multi_layer

ifeq ($(BUILD_WAYLAND), yes)
SRCNAME += wayland-window.c \

PLAT_CFLAGS += -DUSE_WAYLAND
PLAT_LINK += -lwayland-client -lpvr_wlegl
OUTNAME = $(BASE_OUTNAME)_wayland
else
SRCNAME += drm_gbm.c \

PLAT_CFLAGS += -I$(FSDIR)/usr/include/libdrm -I$(FSDIR)/usr/include/gbm
PLAT_LINK += -lgbm -ldrm
OUTNAME = $(BASE_OUTNAME)_drm
endif

OBJECTS = $(SRCNAME:.c=.o)

$(OUTNAME): $(SRCNAME)
	$(PLAT_CPP) -o $@ $^ $(PLAT_CFLAGS) $(LINK) $(PLAT_LINK)

install:
	cp $(OUTNAME) $(FSDIR)/home/root

clean:
	rm $(OUTNAME)
