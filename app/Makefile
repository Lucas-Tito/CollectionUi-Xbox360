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
			-TLBID:1 -RELEASE -IMPLIB:"$(OUT_DIR)/$(PROJECT_NAME).pdb" -XEX:NO \
			-MAP:"$(OUT_DIR)/$(PROJECT_NAME).map"

IMAGEXEX_FLAGS := -nologo

# O xtl.h nao puxa o D3D sozinho: as libs entram explicitamente, e o nome muda
# entre Debug e Release (o sufixo "d").
LIBS_Debug   := d3d9d.lib d3dx9d.lib xgraphicsd.lib xboxkrnl.lib xaudiod2.lib xmcored.lib xavd.lib
LIBS_Release := d3d9.lib d3dx9.lib xgraphics.lib xboxkrnl.lib xaudio2.lib xmcore.lib xav.lib

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

# DEPENDENCIA DE HEADER, grossa de proposito.
#
# A regra ".obj: .cpp" sozinha so olha o .cpp: mexer num header NAO reconstruia nada.
# Isso ja produziu um binario com DUAS definicoes da mesma struct -- biblioteca.h
# ganhou um campo, so os .cpp editados foram recompilados, e os outros ficaram com o
# sizeof antigo. O vetor era preenchido com um passo e percorrido com outro: fatal
# crash lendo 0xFFFFFFF8. Ver a decisao 140.
#
# A precisao viria do -showIncludes do cl.exe, mas a saida dele atravessa o wine com
# caminho do Windows e teria de ser traduzida de volta a cada arquivo. Para 14 fontes
# nossas nao paga: reconstruir tudo custa poucos minutos e NAO tem como errar.
#
# O vendor nao inclui header nosso, entao so depende dos dele -- senao cada ajuste de
# comentario num .h nosso recompilaria o sqlite inteiro.
HEADERS_NOSSOS := $(shell find src -name '*.h') $(wildcard *.h)
HEADERS_VENDOR := $(shell find vendor -name '*.h')
OBJS_VENDOR    := $(filter $(INT_DIR)/vendor/%,$(OBJS))
OBJS_NOSSOS    := $(filter-out $(OBJS_VENDOR),$(OBJS))

# As regras moram DEPOIS do "all": o primeiro alvo do arquivo e o alvo padrao do
# make, e com elas aqui em cima o padrao virava o primeiro .obj da lista.
all: $(OUT_DIR)/$(PROJECT_NAME).xex

$(OBJS_NOSSOS): $(HEADERS_NOSSOS) $(HEADERS_VENDOR) Makefile
$(OBJS_VENDOR): $(HEADERS_VENDOR) Makefile

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
