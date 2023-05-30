## 1. Basic
.DEFAULT_GOAL=kernel
PLATFORM ?= qemu
BUILD=build

# debug options
LOCKTRACE ?= 0
DEBUG_PROC ?= 0
DEBUG_FS ?= 0
STRACE ?= 0

FSIMG = fsimg
ROOT=$(shell pwd)
SCRIPTS = $(ROOT)/scripts
GENINC=include/syscall_gen
$(shell mkdir -p $(GENINC))
MNT_DIR=build/mnt
$(shell mkdir -p $(MNT_DIR))

User=user
oscompU=oscomp_user

# Initial File in Directory
TEST=user_test kalloctest mmaptest \
	clock_gettime_test signal_test \
	writev_test readv_test lseek_test \
	sendfile_test

OSCOMP=chdir close dup2 dup \
    fstat getcwd mkdir_ write \
    openat open read test_echo \
	getdents unlink pipe \
    brk clone execve exit fork \
    getpid getppid sleep times \
    gettimeofday mmap munmap \
    uname wait waitpid yield \
    mount umount text.txt run-all.sh mnt

BIN=ls echo cat mkdir rawcwd rm shutdown wc kill grep sh sysinfo
BOOT=init
BUSYBOX=busybox

TESTFILE = $(addprefix $(FSIMG)/, $(TEST))
OSCOMPFILE = $(addprefix $(FSIMG)/, $(OSCOMP))
BINFILE = $(addprefix $(FSIMG)/, $(BIN))
BOOTFILE = $(addprefix $(FSIMG)/, $(BOOT))
BUSYBOXFILE = $(addprefix $(FSMIG)/, $(BUSYBOX))
$(shell mkdir -p $(FSIMG)/test)
$(shell mkdir -p $(FSIMG)/oscomp)
$(shell mkdir -p $(FSIMG)/bin)
$(shell mkdir -p $(FSIMG)/dev)
$(shell mkdir -p $(FSIMG)/boot)
$(shell mkdir -p $(FSIMG)/busybox)

## 2. Compilation Flags 

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

LDFLAGS = -z max-page-size=4096
CFLAGS = -Wall -Werror -O0 -fno-omit-frame-pointer -ggdb -gdwarf-2

ifdef KCSAN
CFLAGS += -DKCSAN
KCSANFLAG += -fsanitize=thread -fno-inline
OBJS_KCSAN = \
  build/src/driver/console.o \
  build/src/driver/uart.o \
  build/src/lib/printf.o \
  build/src/atomic/spinlock.o \
  build/src/lib/kcsan.o
endif

ifeq ($(STRACE), 1)
CFLAGS += -D__STRACE__
endif
ifeq ($(LOCKTRACE), 1)
CFLAGS += -D__LOCKTRACE__
endif
ifeq ($(DEBUG_PROC), 1)
CFLAGS += -D__DEBUG_PROC__
endif
ifeq ($(DEBUG_FS), 1)
CFLAGS += -D__DEBUG_FS__
endif

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

## 3. File List

# Include all filelist.mk to merge file lists
FILELIST_MK = $(shell find ./src -name "filelist.mk")
include $(FILELIST_MK)

# Filter out directories and files in blacklist to obtain the final set of source files
DIRS-BLACKLIST-y += $(DIRS-BLACKLIST)
SRCS-BLACKLIST-y += $(SRCS-BLACKLIST) $(shell find $(DIRS-BLACKLIST-y) -name "*.c" -o -name "*.S")
SRCS-y += $(shell find $(DIRS-y) -name "*.c" -o -name "*.S")
SRCS = $(filter-out $(SRCS-BLACKLIST-y),$(SRCS-y))

## 4. QEMU Configuration
ifndef CPUS
CPUS := 4
endif

QEMUOPTS = -machine virt -bios bootloader/sbi-qemu -kernel kernel-qemu -m 130M -smp $(CPUS) -nographic
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

## 5. Targets

format:
	clang-format -i $(filter %.c, $(SRCS)) $(shell find include -name "*.c" -o -name "*.h")

all: kernel-qemu image
	$(QEMU) $(QEMUOPTS)

kernel: kernel-qemu
	$(QEMU) $(QEMUOPTS)

gdb: kernel-qemu .gdbinit
	@echo "*** Please make sure to execute 'make image' before attempting to run gdb!!!" 1>&2
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

export CC AS LD OBJCOPY OBJDUMP CFLAGS ASFLAGS LDFLAGS ROOT SCRIPTS User

image: user fat32.img

user: oscomp busybox
# user: busybox
	@echo "$(YELLOW)build user:$(RESET)"
	@cp README.md $(FSIMG)/
	@make -C $(User)
	@cp -r $(addprefix $(oscompU)/build/riscv64/, $(shell ls ./$(oscompU)/build/riscv64/)) $(FSIMG)/oscomp/
	@mv $(BINFILE) $(FSIMG)/bin/
	@mv $(BOOTFILE) $(FSIMG)/boot/
	@mv $(TESTFILE) $(FSIMG)/test/

SRCSFILE = $(addprefix busybox/, $(BUSYBOX))

busybox:
	cp $(SRCSFILE) $(FSIMG)/busybox

oscomp:
	@make -C $(oscompU) -e all CHAPTER=7


fat32.img: dep
	@dd if=/dev/zero of=$@ bs=1M count=128
	@mkfs.vfat -F 32 -s 2 -a $@ 
	@sudo mount -t vfat $@ $(MNT_DIR)
	@sudo cp -r $(FSIMG)/* $(MNT_DIR)/
	@sync $(MNT_DIR) && sudo umount -v $(MNT_DIR)

clean-all: clean
	-@make -C $(User)/ clean
	-@make -C $(oscompU)/ clean
	-rm $(SCRIPTS)/mkfs fs.img fat32.img $(FSIMG)/* -rf

clean: 
	-rm build/* kernel-qemu $(GENINC) -rf 

.PHONY: qemu clean user clean-all format test oscomp dep image busybox

## 6. Build Kernel
include $(SCRIPTS)/build.mk

## 7. misc
include $(SCRIPTS)/colors.mk
# test:
#     $(foreach var,$(.VARIABLES),$(info $(var) = $($(var))))
