# TI-99/sim src/console makefile

include ../../rules.mak

FILES	+= gpl.cpp
FILES	+= screenio.cpp
FILES	+= ti-main.cpp
FILES	+= ti994a-console.cpp
FILES	+= tms9918a-console.cpp

LIBS	+= ti-core.a

TARGET  := $(CFG)/ti99sim-console

ifdef DEBUG
XLIBS     += -Wl,-rpath,/usr/lib -lpthread -lrt
endif

OBJS	+= $(FILES:%.cpp=$(CFG)/%.o)

vpath %.a ../core/$(CFG)

all: $(TARGET)

clean:
	@-rm -Rf *~ $(CFG) $(TARGETS)

$(CFG)/ti99sim-console: $(OBJS) $(LIBS)
	$(CXX) -o $@ $(LFLAGS) $^ $(XLIBS)

-include $(FILES:%.cpp=$(CFG)/%.dep)
