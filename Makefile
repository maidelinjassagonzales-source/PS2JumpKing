# Jump King PS2 - port homebrew (PS2SDK + gsKit). Compilar (WSL): make
EE_BIN  = jumpking_ps2.elf
EE_OBJS = src/main.o src/assets.o src/audio.o src/npc.o src/ui.o src/font.o src/save.o src/walls.o src/props.o src/items.o src/ending.o src/scroll.o src/weather.o src/audsrv_irx.o src/icon_sys.o src/icon_ico.o src/jk/values.o src/jk/level.o src/jk/body.o src/jk/king.o src/jk/camera.o

EE_INCS    += -Isrc -I$(GSKIT)/include
EE_LDFLAGS += -L$(GSKIT)/lib
EE_LIBS    += -lgskit -ldmakit -ldma -lpad -laudsrv -lmc -lpatches -lm
EE_CFLAGS  += -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers

# Assets: ILSpy de linea de comandos y el idioma de tu instalacion (es-ES, en-US, ...)
ILSPY ?= ilspycmd
JKLANG ?= es-ES

all: $(EE_BIN)

# Memory card icon (original character, tools/make_icon.py) embedded in the ELF.
src/icon_sys.c: src/icon/icon.sys
	bin2c $< $@ icon_sys
src/icon_ico.c: src/icon/jkps2.ico
	bin2c $< $@ icon_ico

# El driver de sonido del IOP va incrustado en el ELF.
src/audsrv_irx.c: $(PS2SDK)/iop/irx/audsrv.irx
	bin2c $< $@ audsrv_irx

clean:
	rm -f $(EE_BIN) $(EE_OBJS) src/audsrv_irx.c src/icon_sys.c src/icon_ico.c

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
	python3 -I tools/build_audio.py "$(GAME)" assets_ps2
	$(ILSPY) -p -o _ref/decomp "$(GAME)/JumpKing.exe"
	$(ILSPY) -p -o _ref/lang "$(GAME)/$(JKLANG)/LanguageJK.resources.dll"
	python3 -I tools/build_npcs.py "$(GAME)" assets_ps2 _ref/lang/LanguageJK.language.$(JKLANG).resx
	python3 -I tools/build_weather.py "$(GAME)" assets_ps2
	python3 -I tools/build_scroll.py "$(GAME)" assets_ps2
	python3 -I tools/build_ending.py "$(GAME)" assets_ps2
	python3 -I tools/build_items.py "$(GAME)" assets_ps2 _ref/lang/LanguageJK.language.$(JKLANG).resx
	python3 -I tools/build_props.py "$(GAME)" assets_ps2
	python3 -I tools/build_walls.py "$(GAME)" assets_ps2
	python3 -I tools/build_ui.py "$(GAME)" assets_ps2 _ref/lang/LanguageJK.language.$(JKLANG).resx
