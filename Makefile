# TI-99/sim Makefile

include rules.mak

all: ti99sim

ifeq ($(OS),OS_WIN32)
include Makefile.win32
endif

ifeq ($(OS),OS_LINUX)
include Makefile.linux
endif

ifeq ($(OS),OS_MACOSX)
include Makefile.macosx
endif
