# Jump King PS2 - port homebrew (PS2SDK + gsKit). Compilar (WSL): make
EE_BIN  = jumpking_ps2.elf
EE_OBJS = src/main.o

EE_INCS    += -Isrc -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS    += -lgskit -ldmakit -ldma -lpad -lpatches -lm
EE_CFLAGS  += -O2 -Wall -Wextra -Wno-unused-parameter

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
