PROJECT_NAME := collectionui

WINDOWS_SHIM := wine

CONFIG ?= Debug

BUILD_DIR := build
OUT_DIR := $(BUILD_DIR)/$(CONFIG)/bin
INT_DIR := $(BUILD_DIR)/$(CONFIG)/obj

SRCS_CPP := $(shell find src vendor -name '*.cpp') main.cpp
SRCS_C   := $(shell find src vendor -name '*.c')
OBJS     := $(SRCS_CPP:%.cpp=$(INT_DIR)/%.obj) $(SRCS_C:%.c=$(INT_DIR)/%.obj)

INCLUDES := -I"src" -I"vendor/sqlite" -I"vendor/atg"

XDK_BIN_DIR := $(XEDK)/bin/win32
CXX := "$(XDK_BIN_DIR)/cl.exe"
LD := "$(XDK_BIN_DIR)/link.exe"
IMAGEXEX := "$(XDK_BIN_DIR)/imagexex.exe"
XDK_INC_DIR := $(XEDK)/include/xbox
XDK_CRT_DIR := $(XEDK)/include/crt
XDK_LIB_DIR := "$(XEDK)/lib/xbox"

COMUM := -c -Zi -nologo -MP -D _XBOX -Gm- -GS -fp:fast -fp:except- \
		 -Zc:wchar_t -Zc:forScope -openmp- -Fd"$(INT_DIR)/vc100.pdb" $(INCLUDES) \
		 -FI"$(XDK_INC_DIR)/xbox_intellisense_platform.h"

CXX_FLAGS := $(COMUM) -W4 -EHsc -GR- -TP

# O sqlite e' C, compila como C (-TC) e com o aviso baixado: e codigo de terceiro,
# nao adianta exigir dele o nosso nivel de aviso.
C_FLAGS := $(COMUM) -W3 -TC \
		   -D SQLITE_OS_OTHER=1 -D SQLITE_THREADSAFE=0 -D SQLITE_OMIT_WAL \
		   -D SQLITE_OMIT_LOAD_EXTENSION -D SQLITE_TEMP_STORE=3 \
		   -D SQLITE_DEFAULT_MEMSTATUS=0 -D SQLITE_OMIT_DEPRECATED \
		   -D SQLITE_DISABLE_INTRINSIC \
		   -D SQLITE_PTRSIZE=4 -D "INFINITY=HUGE_VAL"

LD_FLAGS := -ERRORREPORT:QUEUE -NOLOGO -MANIFESTUAC:"level='asInvoker' uiAccess='false'" \
			-DEBUG -PDB:"$(OUT_DIR)/$(PROJECT_NAME).pdb" -STACK:"262144","262144" \
			-TLBID:1 -RELEASE -IMPLIB:"$(OUT_DIR)/$(PROJECT_NAME).pdb" -XEX:NO

IMAGEXEX_FLAGS := -nologo

# O xtl.h nao puxa o D3D sozinho: as libs entram explicitamente, e o nome muda
# entre Debug e Release (o sufixo "d").
LIBS_Debug   := d3d9d.lib d3dx9d.lib xgraphicsd.lib
LIBS_Release := d3d9.lib d3dx9.lib xgraphics.lib

ifeq ($(CONFIG),Debug)
	CXX_FLAGS += -WX- -Od -D _DEBUG -MTd -Gy- -GF-
	C_FLAGS   += -WX- -Od -D _DEBUG -MTd -Gy- -GF-
else ifeq ($(CONFIG),Release)
	CXX_FLAGS += -WX -Ox -Oi -Os -D NDEBUG -MT -Gy -GF
	C_FLAGS   += -Ox -Oi -Os -D NDEBUG -MT -Gy -GF
	LD_FLAGS += -OPT:REF
else
	$(error Unknown CONFIG=$(CONFIG))
endif

all: $(OUT_DIR)/$(PROJECT_NAME).xex

$(OUT_DIR)/$(PROJECT_NAME).xex: $(OUT_DIR)/$(PROJECT_NAME).exe
	@echo "Creating XEX..."
	@mkdir -p $(@D)
	@$(WINDOWS_SHIM) $(IMAGEXEX) $(IMAGEXEX_FLAGS) -out:"$@" "$^"

$(OUT_DIR)/$(PROJECT_NAME).exe: $(OBJS)
	@echo "Linking..."
	@mkdir -p $(@D)
	@LIB=$(XDK_LIB_DIR) $(WINDOWS_SHIM) $(LD) $(LD_FLAGS) -OUT:"$@" $^ $(LIBS_$(CONFIG))

$(INT_DIR)/%.obj: %.cpp
	@mkdir -p $(@D)
	@INCLUDE="$(XDK_INC_DIR);$(XDK_CRT_DIR)" $(WINDOWS_SHIM) $(CXX) $(CXX_FLAGS) -Fo"$@" $<

$(INT_DIR)/%.obj: %.c
	@echo "$< (C)"
	@mkdir -p $(@D)
	@INCLUDE="$(XDK_INC_DIR);$(XDK_CRT_DIR)" $(WINDOWS_SHIM) $(CXX) $(C_FLAGS) -Fo"$@" $<

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
