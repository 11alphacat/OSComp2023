BUILD=build

# to make sure the entry.o is the first one
_OBJS+=$(filter-out build/src/kernel/asm/entry.o, $(addprefix $(BUILD)/, $(addsuffix .o, $(basename $(SRCS)))))
OBJS=$(BUILD)/src/kernel/asm/entry.o
OBJS+=$(_OBJS)

include $(SCRIPTS)/rules.mk

_kernel: $(OBJS) $(SCRIPTS)/kernel.ld
	@$(LD) $(LDFLAGS) -T $(SCRIPTS)/kernel.ld -o _kernel $(OBJS) -e _entry 
	@$(OBJDUMP) -S _kernel > $(BUILD)/kernel.asm
	@$(OBJDUMP) -t _kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BUILD)/kernel.sym
