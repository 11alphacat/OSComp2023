.DEFAULT_GOAL=qemu
PLATFORM ?= qemu
DEBUG ?= 0
FSIMG = fsimg
ROOT=$(shell pwd)
SCRIPTS = $(ROOT)/scripts

GENINC=include/syscall_gen
$(shell mkdir -p $(GENINC))

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gcc
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

ifeq ($(DEBUG), 1)
CFLAGS += -D__DEBUG__
endif

# CFLAGS = -Wall -Werror -O0 -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS = -Wall -O0 -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I$(ROOT)/include
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
ASFLAGS=$(CFLAGS)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096


# file list===============================================================================

# Include all filelist.mk to merge file lists
FILELIST_MK = $(shell find ./src -name "filelist.mk")
include $(FILELIST_MK)

# Filter out directories and files in blacklist to obtain the final set of source files
DIRS-BLACKLIST-y += $(DIRS-BLACKLIST)
SRCS-BLACKLIST-y += $(SRCS-BLACKLIST) $(shell find $(DIRS-BLACKLIST-y) -name "*.c" -o -name "*.S")
SRCS-y += $(shell find $(DIRS-y) -name "*.c" -o -name "*.S")
SRCS = $(filter-out $(SRCS-BLACKLIST-y),$(SRCS-y))

# QEMU configuration======================================================================
ifndef CPUS
CPUS := 3
endif

QEMUOPTS = -machine virt -bios bootloader/sbi-qemu -kernel _kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
# QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -drive file=fat32.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

# qemu-gdb: _kernel .gdbinit fs.img
# 	@echo "*** Now run 'gdb' in another window." 1>&2
# 	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)


qemu-gdb: _kernel .gdbinit
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)


# target =================================================================================
format:
	clang-format -i $(filter %.c, $(SRCS)) $(shell find include -name "*.c" -o -name "*.h")


# qemu: _kernel fs.img
# 	$(QEMU) $(QEMUOPTS)

qemu: _kernel fat32.img
	$(QEMU) $(QEMUOPTS)

# fs.img: $(SCRIPTS)/mkfs user README.md
# 	@$(SCRIPTS)/mkfs fs.img README.md $(addprefix $(FSIMG)/, $(shell ls ./$(FSIMG)))

# $(SCRIPTS)/mkfs: $(SCRIPTS)/mkfs.c include/fs/inode/fs.h include/fs/stat.h include/param.h
# 	@gcc -Werror -Wall -o $(SCRIPTS)/mkfs $(SCRIPTS)/mkfs.c

export CC AS LD OBJCOPY OBJDUMP CFLAGS ASFLAGS LDFLAGS ROOT SCRIPTS xv6U
xv6U=xv6_user
oscompU=user
FILE=brk
TESTFILE=$(addprefix $(oscompU)/build/riscv64/, $(FILE))
# user: oscomp
# 	@echo "$(YELLOW)build user:$(RESET)"
# 	@make -C $(xv6U)

user: 
	@echo "$(YELLOW)build user:$(RESET)"
	@make -C $(xv6U)


oscomp:
	@make -C $(oscompU) -e all CHAPTER=7
	@cp $(TESTFILE) $(FSIMG)/

clean-all: clean
	-@make -C $(xv6U)/ clean
	-@make -C $(oscompU)/ clean

clean: 
	-rm build/* $(SCRIPTS)/mkfs _kernel fs.img $(GENINC) -rf $(FSIMG)/*

MNT_DIR=build/mnt
$(shell mkdir -p $(MNT_DIR))
fat32.img: _kernel user
	@dd if=/dev/zero of=$@ bs=1M count=128
	@mkfs.vfat -F 32 -s 2 $@
	@sudo mount $@ $(MNT_DIR)
	@sudo cp -r $(FSIMG)/* $(MNT_DIR)/
	@sudo umount $(MNT_DIR)

.PHONY: qemu clean user clean-all format test oscomp

include $(SCRIPTS)/build.mk
include $(SCRIPTS)/colors.mk
# test:
#     $(foreach var,$(.VARIABLES),$(info $(var) = $($(var))))
