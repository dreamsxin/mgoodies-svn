# Project: ieview

CPP  = g++.exe
CC   = gcc.exe
WINDRES = windres.exe
RM = rm
UPX = upx -9 -q --compress-icons=0
SRC = IEView.cpp ieview_services.cpp ieview_main.cpp HTMLBuilder.cpp ChatHTMLBuilder.cpp MUCCHTMLBuilder.cpp SRMMHTMLBuilder.cpp ScriverHTMLBuilder.cpp TabSRMMHTMLBuilder.cpp Smiley.cpp Options.cpp Template.cpp TemplateHTMLBuilder.cpp Utils.cpp SmileyWindow.cpp
RES  = ieview.res
OBJ = $(SRC:.cpp=.o) $(RES)
BIN  = d:/mim/plugins/ieview$(_DATE_).dll
INCS =  -I"../../include"
CFLAGS = $(INCS) -DBUILDING_DLL=1 -fexpensive-optimizations -O3 -Os -fno-exceptions -Wno-error -Wall
# -fexpensive-optimizations -O3 -Os
CXXFLAGS = $(INCS) -DBUILDING_DLL=1 -fexpensive-optimizations -O3 -Os -fno-exceptions -Wno-error -Wall
# -fexpensive-optimizations -O3 -Os
# -fexpensive-optimizations -O3 -Os
# -save-temps
LIBS =  -lversion -lwsock32 -lole32 -lcomctl32 -luuid -loleaut32
LFLAGS = -s -mdll -mwindows --add-stdcall-alias
#LFLAGS = -mdll -mwindows -Wl,-Map,ieview.map --no-export-all-symbols --add-stdcall-alias -g3
RCFLAGS = -I rc -O coff
#  --add-stdcall-alias -s  --no-export-all-symbols

.PHONY: all all-before all-after clean clean-custom

all: all-before $(BIN) all-after
ifdef PACK
	$(UPX) $(BIN)
endif

clean: clean-custom
	$(RM) -f $(OBJ) $(BIN)

depend :
	$(CPP) -MM $(CFLAGS) $(SRC)>Makefile.dep

$(RES): $(RES:.res=.rc) resource.h
	$(WINDRES) $(RCFLAGS) -i $(RES:.res=.rc) -o $(RES)

$(BIN): $(OBJ)
	$(CPP) $(CFLAGS) $(OBJ) $(LIBS) $(LFLAGS) -o $(BIN) -Wl

-include Makefile.dep
