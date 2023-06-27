.DEFAULT_GOAL=all

all: musl busybox libc-test

musl:
	make -C musl-1.2.4 -j4
	sudo make -C musl-1.2.4 install
	sudo cp /usr/local/musl/bin/musl-gcc /usr/bin/riscv64-musl-gcc
	riscv64-linux-gnu-objdump -S ./musl-1.2.4/lib/libc.so > libc_so.asm

busybox:
	make -C busybox CC="riscv64-musl-gcc" STRIP=riscv64-linux-gnu-strip -j4	
	cp busybox/busybox_unstripped ../busybox/busybox
	riscv64-linux-gnu-objdump -S ../busybox/busybox > ../busybox/busybox.asm

libctest:
	make -C libc-test disk -j4
	riscv64-linux-gnu-objdump -S ./libc-test/disk/dlopen_dso.so > ./libc-test/disk/dlopen_dso_so.asm
	riscv64-linux-gnu-objdump -S ./libc-test/entry-dynamic.exe > ./libc-test/disk/entry-dynamic.asm

clean:
	make -C musl-1.2.4 clean
	make -C busybox clean
	make -C libc-test clean

configure:	
	echo TODO

.PHONY: all musl busybox clean configure