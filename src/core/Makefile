# TI-99/sim src/core makefile

include ../../rules.mak

FILES	+= cBaseObject.cpp

FILES	+= arcfs.cpp
FILES	+= cartridge.cpp
FILES	+= compress.cpp
FILES	+= decodelzw.cpp
FILES	+= device.cpp
FILES	+= disassemble.cpp
FILES	+= diskfs.cpp
FILES	+= diskio.cpp
FILES	+= fileio.cpp
FILES	+= encodelzw.cpp
FILES	+= fs.cpp
FILES	+= opcodes.cpp
FILES	+= option.cpp
FILES	+= pseudofs.cpp
FILES	+= support.cpp
FILES	+= ti-disk.cpp
FILES	+= ti994a.cpp
FILES	+= tms5220.cpp
FILES	+= tms9900.cpp
FILES	+= tms9901.cpp
FILES	+= tms9918a.cpp
FILES	+= tms9919.cpp

OBJS	+= $(FILES:%.cpp=$(CFG)/%.o)

TARGET  := $(CFG)/ti-core.a

all: $(TARGET)

clean:
	@-rm -Rf *~ $(CFG) $(TARGETS)

$(CFG)/ti-core.a: $(OBJS)
	$(AR) rc $@ $?
	ranlib $@

-include $(FILES:%.cpp=$(CFG)/%.dep)
