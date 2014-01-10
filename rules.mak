# TI-99/sim common build rules

ifneq (,$(findstring /,$(shell whereis ccache)))
CXX      := ccache g++
else
CXX      := g++
endif

CFLAGS   := -g -O3 -fno-strict-aliasing -fexceptions -fPIC

WARNINGS := -Wall -Wextra -Wno-unused-parameter

INCLUDES := -I../../include

ifndef CFG
CFG      := Release
endif

ifeq ($(CFG),Debug)
DEBUG    := 1
endif

ifdef ARCH
CFLAGS   += -march=$(ARCH)
endif

ifdef DEBUG
CFLAGS   += -DDEBUG
CFLAGS   += -ggdb3
CXX      += -rdynamic
endif

ifdef LOGGER
CFLAGS   += -D_REENTRANT -DLOGGER
LIBS	 += -lpthread -lrt
ifdef DEBUG
CFLAGS   += -DDEBUG_LOGGER
endif
endif

OS       := OS_UNKNOWN

# OSTYPE may not be exported from the shell - make our own
ifndef OSTYPE
OSTYPE   := $(shell uname -s)
endif

ifeq ($(OSTYPE),Linux)
OS       := OS_LINUX
endif

ifeq ($(OSTYPE),FreeBSD)
OS       := OS_LINUX
endif

ifeq ($(OSTYPE),Darwin)
CXX      := c++
OS       := OS_MACOSX
endif

ifeq ($(OSTYPE),CYGWIN_NT-5.1)
OS       := OS_WIN32
endif

ifdef WIN32
OS       := OS_WIN32
endif

ifdef AMIGA
OS	     := OS_AMIGAOS
endif

CFLAGS	 += -D$(OS)

DF	= $(CFG)/$(*F)

$(CFG)/%.o : %.cpp
	@echo $<
	@$(CXX) -c $(CFLAGS) $(WARNINGS) $(INCLUDES) -MD -o $@ $<
	@cp $(DF).d $(DF).dep; \
		sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
			-e '/^$$/ d' -e 's/$$/ :/' < $(DF).d >> $(DF).dep; \
		rm -f $(DF).d

$(CFG)/%.o : %.m
	@echo $<
	@$(CC) -c $(CFLAGS) $(WARNINGS) $(INCLUDES) -o $@ $<

%.h.gch: %.h
	@echo Generating pre-compiled header for $<
	@$(CXX) $(CFLAGS) $(WARNINGS) $(INCLUDES) $<

.SUFFIXES: .cpp .c .o

all: $(CFG)/.

target: $(CFG)/.

$(CFG)/.:
	@mkdir $(CFG)
