CC = gcc
CFLAGS = -O2 -g -Wall -DSDL
LDFLAGS =

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null || echo "")
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2 2>/dev/null || echo "-lSDL2")

CFLAGS += $(SDL_CFLAGS)
LIBS = $(SDL_LIBS) -lm

TARGET = quake

CORE_SRCS = \
	src/cd_sdl.c \
	src/chase.c \
	src/cl_demo.c \
	src/cl_input.c \
	src/cl_main.c \
	src/cl_parse.c \
	src/cl_tent.c \
	src/cmd.c \
	src/common.c \
	src/console.c \
	src/crc.c \
	src/cvar.c \
	src/d_edge.c \
	src/d_fill.c \
	src/d_init.c \
	src/d_modech.c \
	src/d_part.c \
	src/d_polyse.c \
	src/d_scan.c \
	src/d_sky.c \
	src/d_sprite.c \
	src/d_surf.c \
	src/d_vars.c \
	src/d_zpoint.c \
	src/draw.c \
	src/host.c \
	src/host_cmd.c \
	src/keys.c \
	src/mathlib.c \
	src/menu.c \
	src/model.c \
	src/net_bsd.c \
	src/net_dgrm.c \
	src/net_loop.c \
	src/net_main.c \
	src/net_udp.c \
	src/net_vcr.c \
	src/net_wso.c \
	src/nonintel.c \
	src/pr_cmds.c \
	src/pr_edict.c \
	src/pr_exec.c \
	src/r_aclip.c \
	src/r_alias.c \
	src/r_bsp.c \
	src/r_draw.c \
	src/r_edge.c \
	src/r_efrag.c \
	src/r_light.c \
	src/r_main.c \
	src/r_misc.c \
	src/r_part.c \
	src/r_sky.c \
	src/r_sprite.c \
	src/r_surf.c \
	src/r_vars.c \
	src/sbar.c \
	src/screen.c \
	src/snd_dma.c \
	src/snd_mem.c \
	src/snd_mix.c \
	src/snd_sdl.c \
	src/sv_main.c \
	src/sv_move.c \
	src/sv_phys.c \
	src/sv_user.c \
	src/sys_sdl.c \
	src/vid_sdl.c \
	src/view.c \
	src/wad.c \
	src/world.c \
	src/zone.c

OBJS = $(CORE_SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

$(OBJS): src/quakedef.h src/common.h

.PHONY: all clean
