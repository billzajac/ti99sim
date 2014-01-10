# TI-99/sim src/sdl makefile

include ../../rules.mak

ifeq ($(OS),OS_LINUX)
CFLAGS	+= `sdl-config --cflags`
XLIBS	+= `sdl-config --libs`
endif

ifeq ($(OS),OS_MACOSX)
CFLAGS  += -I ~/Library/Frameworks/SDL.framework/Headers
XLIBS   += -F ~/Library/Frameworks -framework SDL
XLIBS   += -lobjc -framework Cocoa
OBJS    += $(CFG)/SDLMain.o
endif

FILES	+= main.cpp
FILES	+= bitmap.cpp
FILES	+= tms9919-sdl.cpp
FILES	+= tms9918a-sdl.cpp
FILES	+= ti994a-sdl.cpp

LIBS	+= ti-core.a

OBJS	+= $(FILES:%.cpp=$(CFG)/%.o)

TARGET	:= $(CFG)/ti99sim-sdl

vpath %.a ../core/$(CFG)

all: $(TARGET)

clean:
	@-rm -Rf *~ $(CFG) $(TARGETS)

$(TARGET): $(OBJS) $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

-include $(FILES:%.cpp=$(CFG)/%.dep)
