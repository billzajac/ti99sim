# TI-99/sim src/util makefile

include ../../rules.mak

ifeq ($(OS),OS_LINUX)
CFLAGS	+= `sdl-config --cflags`
XLIBS	+= `sdl-config --libs`
endif

ifeq ($(OS),OS_MACOSX)
CFLAGS  += -I ~/Library/Frameworks/SDL.framework/Headers
XLIBS   += -F ~/Library/Frameworks -framework SDL
XLIBS   += -lobjc -framework Cocoa
SDLLIBS := SDLMain.o
endif

FILES	+= convert.cpp
FILES	+= decode.cpp
FILES	+= disk.cpp
FILES	+= dumpcpu.cpp
FILES	+= dumpgrom.cpp
FILES	+= dumpspch.cpp
FILES	+= list.cpp
FILES	+= mkspch.cpp
FILES	+= say.cpp

LIBS	+= ti-core.a

TARGET	+= convert-ctg
TARGET	+= decode
TARGET	+= disk
TARGET	+= dumpcpu
TARGET	+= dumpgrom
TARGET	+= dumpspch
TARGET	+= list
TARGET	+= mkspch
TARGET	+= say

vpath %.a ../core/$(CFG)
vpath %.o ../console/$(CFG):../sdl/$(CFG)

TARGETS	:= $(TARGET:%=$(CFG)/%)

all: $(TARGETS)

clean:
	@-rm -Rf *~ $(CFG) $(TARGETS)

$(CFG)/convert-ctg: $(CFG)/convert.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(CFG)/decode: $(CFG)/decode.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^

$(CFG)/disk: $(CFG)/disk.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(CFG)/dumpcpu: $(CFG)/dumpcpu.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(CFG)/dumpgrom: $(CFG)/dumpgrom.o gpl.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(CFG)/dumpspch: $(CFG)/dumpspch.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(CFG)/list: $(CFG)/list.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(CFG)/mkspch: $(CFG)/mkspch.o $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

$(CFG)/say: $(CFG)/say.o tms9919-sdl.o $(LIBS) $(SDLLIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

-include $(FILES:%.cpp=$(CFG)/%.dep)
