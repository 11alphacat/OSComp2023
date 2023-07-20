make image
riscv64-linux-gnu-objcopy -S -O binary fsimg/submit tmp
xxd tmp > submit
rm tmp

