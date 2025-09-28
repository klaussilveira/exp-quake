.PHONY: help run run-debug run-valgrind clean report
.DEFAULT_GOAL := all
PROJECT := expquake
VERSION := $(shell git show -s --format=%h)

help: # Display the application manual
	@echo "$(PROJECT) version \033[33m$(VERSION)\n\e[0m"
	@echo "\033[1;37mUSAGE\e[0m"
	@echo "  \e[4mmake\e[0m <command> [<arg1>] ... [<argN>]\n"
	@echo "\033[1;37mAVAILABLE COMMANDS\e[0m"
	@echo "  \033[32mall\033[0m                  Build release version (default)"
	@echo "  \033[32mrelease\033[0m              Build release version"
	@echo "  \033[32mdebug\033[0m                Build debug version"
	@grep -E '^[a-zA-Z_-]+:.*?# .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?# "}; {printf "  \033[32m%-20s\033[0m %s\n", $$1, $$2}'

# Directories
SRCDIR = src
BUILDDIR = build
BINDIR = bin
DEPDIR = $(BUILDDIR)/.deps

# Compiler and base flags
CC = gcc
STRIP = strip
WARNINGS = -Wall -Wextra -pedantic
COMMON_CFLAGS = $(WARNINGS) -DSDL $(EXTRA_CFLAGS)
COMMON_LDFLAGS = $(EXTRA_LDFLAGS)

# Build configurations
DEBUG_CFLAGS = -O0 -g3 -DDEBUG -fno-omit-frame-pointer
RELEASE_CFLAGS = -O2 -g -DNDEBUG -fomit-frame-pointer
BUILD_TYPE ?= release

# SDL configuration
SDL_CFLAGS := $(shell sdl-config2 --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null || echo "")
SDL_LIBS := $(shell sdl-config2 --libs 2>/dev/null || pkg-config --libs sdl2 2>/dev/null || echo "-lSDL2")

# Libraries
LIBS = $(SDL_LIBS) -lm $(EXTRA_LIBS)

# Sources
CORE_SRCS = \
	cd_sdl.c \
	chase.c \
	cl_demo.c \
	cl_input.c \
	cl_main.c \
	cl_parse.c \
	cl_tent.c \
	cmd.c \
	common.c \
	console.c \
	crc.c \
	cvar.c \
	d_edge.c \
	d_fill.c \
	d_init.c \
	d_modech.c \
	d_part.c \
	d_polyse.c \
	d_scan.c \
	d_sky.c \
	d_sprite.c \
	d_surf.c \
	d_vars.c \
	d_zpoint.c \
	draw.c \
	host.c \
	host_cmd.c \
	keys.c \
	mathlib.c \
	menu.c \
	model.c \
	net_bsd.c \
	net_dgrm.c \
	net_loop.c \
	net_main.c \
	net_udp.c \
	net_vcr.c \
	net_wso.c \
	nonintel.c \
	pr_cmds.c \
	pr_edict.c \
	pr_exec.c \
	r_aclip.c \
	r_alias.c \
	r_bsp.c \
	r_draw.c \
	r_edge.c \
	r_efrag.c \
	r_light.c \
	r_main.c \
	r_misc.c \
	r_part.c \
	r_sky.c \
	r_sprite.c \
	r_surf.c \
	r_vars.c \
	sbar.c \
	screen.c \
	snd_dma.c \
	snd_mem.c \
	snd_mix.c \
	snd_sdl.c \
	sv_main.c \
	sv_move.c \
	sv_phys.c \
	sv_user.c \
	sys_sdl.c \
	vid_sdl.c \
	view.c \
	wad.c \
	world.c \
	zone.c

# Set build-specific variables
ifeq ($(BUILD_TYPE),debug)
    CFLAGS = $(COMMON_CFLAGS) $(DEBUG_CFLAGS) $(SDL_CFLAGS)
    LDFLAGS = $(COMMON_LDFLAGS) -g
    ifdef USE_ASAN
        CFLAGS += -fsanitize=address -fno-omit-frame-pointer
        LDFLAGS += -fsanitize=address
    endif
    ifdef USE_TSAN
        CFLAGS += -fsanitize=thread
        LDFLAGS += -fsanitize=thread
    endif
    BUILD_SUFFIX = -debug
    TARGET_DIR = $(BINDIR)/debug
else
    CFLAGS = $(COMMON_CFLAGS) $(RELEASE_CFLAGS) $(SDL_CFLAGS)
    LDFLAGS = $(COMMON_LDFLAGS)
    ifdef USE_LTO
        CFLAGS += -flto
        LDFLAGS += -flto
    endif
    ifdef USE_AGGRESSIVE_OPTS
        CFLAGS += -march=native -mtune=native -ffast-math
    endif
    BUILD_SUFFIX =
    TARGET_DIR = $(BINDIR)/release
endif

OBJ_DIR = $(BUILDDIR)/$(BUILD_TYPE)
TARGET = $(TARGET_DIR)/$(PROJECT)$(BUILD_SUFFIX)$(EXE_EXT)
OBJS = $(addprefix $(OBJ_DIR)/,$(CORE_SRCS:.c=.o))
DEPS = $(OBJS:.o=.d)

all: release

release:
	@$(MAKE) build BUILD_TYPE=release

debug:
	@$(MAKE) build BUILD_TYPE=debug

build: $(TARGET)
	@echo "Build complete: $(TARGET)"
	@size $(TARGET) 2>/dev/null || true

# Create directories
$(OBJ_DIR) $(TARGET_DIR) $(DEPDIR):
	@mkdir -p $@

# Link executable
$(TARGET): $(OBJS) | $(TARGET_DIR)
	@echo "  LINK    $@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
ifeq ($(BUILD_TYPE),release)
	@echo "  STRIP   $@"
	@$(STRIP) $@ 2>/dev/null || true
endif

# Compile sources
$(OBJ_DIR)/%.o: $(SRCDIR)/%.c | $(OBJ_DIR) $(DEPDIR)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -MMD -MP -MF $(DEPDIR)/$*.d -c -o $@ $<

run: $(TARGET) # Run game
	@echo "Running $(PROJECT)..."
	@./$(dir $(TARGET))$(notdir $(TARGET))

run-debug: debug # Run with GDB
	@echo "Running $(PROJECT) in GDB..."
	@gdb ./$(dir $(TARGET))$(notdir $(TARGET))

run-valgrind: debug # Run with Valgrind
	@echo "Running $(PROJECT) with Valgrind..."
	@valgrind --leak-check=full --show-leak-kinds=all ./$(dir $(TARGET))$(notdir $(TARGET))

clean: # Clean build artifacts
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILDDIR) $(BINDIR)

report: # Report build configuration
	@echo "Build Configuration"
	@echo "==================="
	@echo "Project:      $(PROJECT) v$(VERSION)"
	@echo "Build type:   $(BUILD_TYPE)"
	@echo "Compiler:     $(CC)"
	@echo "Target:       $(TARGET)"
	@echo ""
	@echo "Directories:"
	@echo "  Source:     $(SRCDIR)"
	@echo "  Build:      $(OBJ_DIR)"
	@echo "  Binary:     $(TARGET_DIR)"
	@echo ""
	@echo "Flags:"
	@echo "  CFLAGS:     $(CFLAGS)"
	@echo "  LDFLAGS:    $(LDFLAGS)"
	@echo "  LIBS:       $(LIBS)"
