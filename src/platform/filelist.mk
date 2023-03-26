ifeq ($(PLATFORM), qemu)
DIRS-y += src/platform/$(PLATFORM)
SRCS-BLACKLIST-y += src/platform/$(PLATFORM)/ramdisk.c
endif