BUILD=../build/lib
ULIB = $(BUILD)/ulib.o $(BUILD)/usys.o $(BUILD)/printf.o $(BUILD)/umalloc.o
OBJS=$(ULIB)
ARCHIVE=lib.a

all: $(ARCHIVE)

$(BUILD)/usys.S : usys.pl
	@perl usys.pl > $(BUILD)/usys.S
$(BUILD)/usys.o : $(BUILD)/usys.S
	@$(CC) $(CFLAGS) -c -o $(BUILD)/usys.o $(BUILD)/usys.S

include $(SCRIPTS)/rules.mk

clean:
	@-rm lib.a -f

### Rule (archive): objects (`*.o`) -> `ARCHIVE.a` (ar)
$(ARCHIVE): $(ULIB)
	@echo + AR "->" $(shell realpath $@ --relative-to .)
	@ar rcsT $@ $(ULIB)

# test:
#     $(foreach var,$(.VARIABLES),$(info $(var) = $($(var))))