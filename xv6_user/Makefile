BUILD=build
FSIMG=$(ROOT)/fsimg
$(shell mkdir -p $(BUILD))
$(shell mkdir -p $(FSIMG))

# SRCS=$(shell find src -name "*.c" -o -name "*.S")
SRCS=src/sh.c src/init.c src/echo.c src/cowtest.c src/kalloctest.c src/mmaptest.c src/mkdir.c src/user_test.c src/ls.c src/rawcwd.c src/cat.c \
	 src/kill.c
OBJS=$(addprefix $(BUILD)/, $(addsuffix .o, $(basename $(SRCS))))
TARGET=$(addprefix $(FSIMG)/, $(basename $(notdir $(SRCS))))
ARCHIVE=lib/lib.a
CFLAGS+=-I$(ROOT)/$(xv6U)/include

all: $(OBJS) $(TARGET)

export CFLAGS CC AS ASFLAGS OBJCOPY OBJDUMP LDFLAGS LD SCRIPTS
$(ARCHIVE): lib
lib:
	@echo "$(YELLOW)build user lib:$(RESET)"
	@make -C lib

# forktest has less library code linked in - needs to be small
# in order to be able to max out the proc table.
# $(FSIMG)/forktest: $(BUILD)/src/forktest.o $(ARCHIVE)
# 	@$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSIMG)/forktest $(BUILD)/src/forktest.o $(ARCHIVE)
# 	@$(OBJDUMP) -S $(FSIMG)/forktest > $(BUILD)/src/forktest.asm

include $(SCRIPTS)/rules.mk
include $(SCRIPTS)/colors.mk


$(FSIMG)/%: $(BUILD)/src/%.o $(ARCHIVE) 
	@$(LD) $(LDFLAGS) -T src/user.ld -o $@ $^
	@$(OBJDUMP) -S $@ > $(BUILD)/src/$*.asm
	@$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(BUILD)/src/$*.sym

clean:
	@-rm build/* -rf
	-@make -C lib clean

.PHONY: lib clean
