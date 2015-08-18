.PHONY: all install uninstall clean

-include rules.make

INCS     ?= -I$(TGTFS_PATH)/usr/include -I$(TGTFS_PATH)/usr/include/drm

LIB_PATH ?= -L$(TGTFS_PATH)/usr/lib
RPATH ?= -Wl,-rpath-link $(TGTFS_PATH)/usr/lib

INST_DEST    ?= $(TGTFS_PATH)/usr/bin

CFLAGS   := -W -Wall -O2 $(INCS)
LIBS     := -lEGL -ldrm -lgbm -lGLESv2 -ludev -lm
LDFLAGS  := $(LIB_PATH) $(LIBS) $(RPATH)

TARGET = pixmapTriangle

SOURCES = main.c
OBJFILES = $(SOURCES:%.c=%.o)

all:   $(TARGET)

$(TARGET):     $(OBJFILES)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJFILES):   %.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

install:       $(TARGET)
	install -m 0755 $^ $(INST_DEST)

uninstall:
	cd $(INST_DEST) && rm -f $(TARGET)

.PHONY: clean
clean:
	-rm -f $(OBJFILES) $(TARGET)

