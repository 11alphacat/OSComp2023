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

libc-bench:
	make -C libc-bench CC="riscv64-linux-gnu-gcc -static" -j4
	riscv64-linux-gnu-objdump -S ./libc-bench/libc-bench > ./libc-bench/libc-bench.asm	

iozone:
	make -C iozone linux CC="riscv64-linux-gnu-gcc -static" -j4
	riscv64-linux-gnu-objdump -S ./iozone/iozone > ./iozone/iozone.asm	

time-test: 
	make -C time-test CC="riscv64-linux-gnu-gcc"
	riscv64-linux-gnu-objdump -S ./time-test/time-test > ./time-test/time-test.asm	

lua:
	make -C lua CC="riscv64-linux-gnu-gcc -static" -j4
	riscv64-linux-gnu-objdump -S ./lua/src/lua > ./lua/lua.asm

lmbench:
	make -C lmbench CC="riscv64-linux-gnu-gcc -static" OS=riscv64 -j4

unix-bench:
	make -C UnixBench CC="riscv64-linux-gnu-gcc" -j4 all

cyclictest:
	make -C rt-tests cyclictest hackbench
	

clean:
	make -C musl-1.2.4 clean
	make -C busybox clean
	make -C libc-test clean
	# make -C lmbench clean
	make -C time-test clean
	make -C libc-bench clean
	make -C unix-bench clean
	make -C iozone clean
	make -C cyclictest clean
	make -C lua clean

configure:	
	echo TODO

.PHONY: all musl busybox clean configure lmbench time-test libc-bench unix-bench iozone cyclictest lua