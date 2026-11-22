# Jump King PS2 - port homebrew (PS2SDK + gsKit). Compilar (WSL): make
EE_BIN  = jumpking_ps2.elf
EE_OBJS = src/main.o src/assets.o src/jk/values.o src/jk/level.o src/jk/body.o src/jk/king.o src/jk/camera.o

EE_INCS    += -Isrc -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS    += -lgskit -ldmakit -ldma -lpad -lpatches -lm
EE_CFLAGS  += -O2 -Wall -Wextra -Wno-unused-parameter

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

# Pruebas de la fisica en el PC (gcc nativo): make test
JK_SRCS = src/jk/values.c src/jk/level.c src/jk/body.c src/jk/king.c src/jk/camera.c
test:
	gcc -O2 -Wall -Wextra -Isrc -o tests/host_sim $(JK_SRCS) tests/host_sim.c -lm
	./tests/host_sim

# Convierte los datos desde TU copia del juego (no van al repo): make assets GAME="/mnt/c/.../Jump King"
assets:
	python3 -I tools/build_assets.py "$(GAME)" assets_ps2
