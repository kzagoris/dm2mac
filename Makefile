# The Dungeon Master II port: the game, the test library, and the Windows build.
# The compiler is clang; a machine that has only a versioned clang (clang-18) uses the newest one. CC and CXX of the
# environment or of the command line win: make CC=clang-17 CXX=clang++-17
versioned = $(notdir $(lastword $(sort $(wildcard /usr/bin/$(1)-[0-9]*))))
ifeq ($(origin CC),default)
CC         := $(if $(shell command -v clang),clang,$(or $(call versioned,clang),clang))
endif
ifeq ($(origin CXX),default)
CXX        := $(if $(shell command -v clang++),clang++,$(or $(call versioned,clang++),clang++))
endif
BASE_CFLAGS = -std=c11 -Wall -Wextra -Werror -Wshadow -Wmissing-prototypes -Wstrict-prototypes -Wvla \
              -Wimplicit-fallthrough -O2 -g -fwrapv
# xBRZ uses C++20 attributes and aliases its 4x4 neighbourhood as a 3x3 view.
BASE_CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -fno-strict-aliasing -O2 -g
CFLAGS      = $(BASE_CFLAGS)
CXXFLAGS    = $(BASE_CXXFLAGS)
CPPFLAGS    = -isystem src/third_party -MMD -MP
# The include path of a layer: its own public headers and those of the layers before it. A private *_internal.h
# stands next to its .c file. A wrong layer include does not compile.
INC_data     = -Isrc/data/include
INC_engine   = $(INC_data) -Isrc/engine/include
INC_ui       = $(INC_engine) -Isrc/ui/include
INC_control  = $(INC_ui) -Isrc/control/include
INC_platform = $(INC_control) -Isrc/platform/include
layer_inc    = $(INC_$(word 2,$(subst /, ,$(1))))
LDFLAGS     =
ifeq ($(SANITIZE),1)
CFLAGS  += -fsanitize=address,undefined
CXXFLAGS += -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
endif
# Extra flags for one build, for example a developer check: make lib CFLAGS_EXTRA=-DX
CFLAGS += $(CFLAGS_EXTRA)
CXXFLAGS += $(CFLAGS_EXTRA)

# SDL3 from Homebrew. Recursively expanded: only the targets that compile or link SDL code run brew.
SDL_PREFIX ?= $(shell pkg-config --variable=prefix sdl3 2>/dev/null || brew --prefix sdl3)
SHARED_FLAGS = -dynamiclib
ifeq ($(shell uname -s),Linux)
CFLAGS += -fPIC -D_XOPEN_SOURCE=700
CXXFLAGS += -fPIC
SHARED_FLAGS = -shared
LDLIBS += -lm
endif

B   = build
OBJ = $(B)/obj

SOURCES      = $(wildcard src/*/*.c src/third_party/*/*.c)
CXX_SOURCES  = $(wildcard src/third_party/*/*.cpp)
SDL_SOURCES  = src/platform/main.c src/platform/platform.c
WINDOWS_ONLY = src/platform/transport_none.c
POSIX_ONLY   = src/platform/transport.c
GAME_SOURCES = $(filter-out $(WINDOWS_ONLY),$(SOURCES))
LIB_SOURCES  = $(filter-out $(SDL_SOURCES) $(WINDOWS_ONLY),$(SOURCES))
WIN_SOURCES  = $(filter-out $(POSIX_ONLY),$(SOURCES))
C_FILES      = $(wildcard src/*/*.[ch] src/*/include/*.h)

objects = $(patsubst src/%.c,$(1)/%.o,$(2))
cxx_objects = $(patsubst src/%.cpp,$(1)/%.o,$(CXX_SOURCES))

all: $(B)/dm2 $(B)/libdm2.dylib

lib: $(B)/libdm2.dylib

# The stamp of the compile flags. When the flags change (SANITIZE, CFLAGS_EXTRA), the stamp deletes the objects,
# so all recompile. Simply expanded: the SDL include path of two objects stays out.
OBJ_FLAGS := $(strip $(CC) $(CPPFLAGS) $(CFLAGS) $(CXX) $(CXXFLAGS))
stamp = $(strip $(shell cat $(1) 2>/dev/null))
write_flags = @mkdir -p $(@D); rm -f $(@D)/*/*.o $(@D)/*/*/*.o; echo '$(1)' > $@

ifneq ($(OBJ_FLAGS),$(call stamp,$(OBJ)/flags.txt))
$(OBJ)/flags.txt: FORCE
endif
$(OBJ)/flags.txt:
	$(call write_flags,$(OBJ_FLAGS))

$(OBJ)/%.o: src/%.c Makefile $(OBJ)/flags.txt
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(call layer_inc,$<) $(CFLAGS) -c -o $@ $<

$(call objects,$(OBJ),$(SDL_SOURCES)): CPPFLAGS += -I$(SDL_PREFIX)/include

$(OBJ)/%.o: src/%.cpp Makefile $(OBJ)/flags.txt
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

$(OBJ)/third_party/xbrz/xbrz.o: CXXFLAGS += -Wno-unused-parameter -Wno-unused-function

$(B)/dm2: $(call objects,$(OBJ),$(GAME_SOURCES)) $(call cxx_objects,$(OBJ))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ -L$(SDL_PREFIX)/lib -lSDL3 $(LDLIBS)

$(B)/libdm2.dylib: $(call objects,$(OBJ),$(LIB_SOURCES)) $(call cxx_objects,$(OBJ))
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SHARED_FLAGS) -o $@ $^ $(LDLIBS)

# Windows: cross-build with MinGW-w64 (brew install mingw-w64) and the SDL3 MinGW package, copied into build/ from
# the dm port. Windows has no control socket: transport_none.c replaces transport.c. -mwindows: no console window; the
# messages go to dm2.log in the game folder.
WIN_CC            = x86_64-w64-mingw32-gcc
WIN_CXX           = x86_64-w64-mingw32-g++
WIN_OBJ           = $(B)/obj-win
DM_DIR           ?= ../dm
SDL_MINGW_VERSION = 3.4.18
SDL_MINGW         = $(B)/sdl-mingw/SDL3-$(SDL_MINGW_VERSION)/x86_64-w64-mingw32
WIN_OBJ_FLAGS := $(strip $(WIN_CC) $(CPPFLAGS) $(BASE_CFLAGS) $(WIN_CXX) $(BASE_CXXFLAGS))

windows: $(B)/dm2.exe

ifneq ($(WIN_OBJ_FLAGS),$(call stamp,$(WIN_OBJ)/flags.txt))
$(WIN_OBJ)/flags.txt: FORCE
endif
$(WIN_OBJ)/flags.txt:
	$(call write_flags,$(WIN_OBJ_FLAGS))

$(WIN_OBJ)/%.o: src/%.c Makefile $(WIN_OBJ)/flags.txt | $(SDL_MINGW)
	@mkdir -p $(@D)
	$(WIN_CC) $(CPPFLAGS) $(call layer_inc,$<) -I$(SDL_MINGW)/include $(BASE_CFLAGS) -c -o $@ $<

$(WIN_OBJ)/%.o: src/%.cpp Makefile $(WIN_OBJ)/flags.txt
	@mkdir -p $(@D)
	$(WIN_CXX) $(CPPFLAGS) $(BASE_CXXFLAGS) -c -o $@ $<

$(WIN_OBJ)/third_party/xbrz/xbrz.o: BASE_CXXFLAGS += -Wno-unused-parameter

# Keep the C++ runtime static: the release still needs only SDL3.dll beside dm2.exe.
$(B)/dm2.exe: $(call objects,$(WIN_OBJ),$(WIN_SOURCES)) $(call cxx_objects,$(WIN_OBJ))
	$(WIN_CXX) $(BASE_CXXFLAGS) -static -mwindows -o $@ $^ -L$(SDL_MINGW)/lib -Wl,-Bdynamic -lSDL3 -Wl,-Bstatic $(LDLIBS)

$(SDL_MINGW):
	@test -d $(DM_DIR)/build/sdl-mingw || { echo "copy the SDL3 MinGW package to build/sdl-mingw"; exit 1; }
	@mkdir -p $(B)
	cp -R $(DM_DIR)/build/sdl-mingw $(B)/

test: ; tools/test-suite

# Release zips: dist (macOS arm64), dist-win (Windows x64; DM_DIR names the dm folder).
dist: ; tools/dist/make-dist.sh
dist-win: ; tools/dist/make-dist-win.sh

# The quick check after an edit, without a build: formats the C files, then the lint and the coding standard check.
check-fast: format
	uv run ruff check tests tools/dm2tools
	uv run pytest -q tests/standard

# Lint, the coding standard check, then the full suite.
check:
	uv run ruff check tests tools/dm2tools
	uv run pytest -q tests/standard
	$(MAKE) test

clean:
	rm -rf $(B)

-include $(wildcard $(OBJ)/*/*.d $(OBJ)/*/*/*.d $(WIN_OBJ)/*/*.d $(WIN_OBJ)/*/*/*.d)

FORCE:

.PHONY: all lib windows dist dist-win test check check-fast clean format FORCE

format:
	uv run clang-format -i $(C_FILES)
