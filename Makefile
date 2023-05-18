# TMote Sky (emulated in Cooja) is the default target
ifndef TARGET
	TARGET = sky
else
	# To compile for Zolertia Firefly (testbed) use the following target and board
	# Don't forget to make clean when you are changing the board
	ifeq ($(TARGET), zoul)
		BOARD	?= firefly
		LDFLAGS += -specs=nosys.specs # For Zolertia Firefly only
	endif
endif

DEFINES=PROJECT_CONF_H=\"src/include/project-conf.h\"
CONTIKI_PROJECT = app

PROJECTDIRS += src/tools
PROJECTDIRS += src/include
PROJECTDIRS += src/res
# Tool to estimate node duty cycle 
PROJECT_SOURCEFILES += simple-energest.c

PROJECT_SOURCEFILES += protocol.c
PROJECT_SOURCEFILES += routing-table.c
PROJECT_SOURCEFILES += packet.c
PROJECT_SOURCEFILES += buffer.c

all: $(CONTIKI_PROJECT)

purge:
	rm -f ./*.csv
	rm -f ./*.log
	rm -f ./*.testlog

CONTIKI_WITH_RIME = 1
CONTIKI ?= ../../contiki
include $(CONTIKI)/Makefile.include