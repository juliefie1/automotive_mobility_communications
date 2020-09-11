##########################################################################################################################
# File devoloped by [bsakli@kineis.com] version: [1.0.0] date: [Wed Mar 04 2020]
##########################################################################################################################

######################################
# target
######################################
TARGET = kineis_demo

######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og

#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

# doc/doxygen path
DOC_DIR = doc
DOXY_WARN_LOGFILE = doxy_warn_log_file.txt

######################################
# source
######################################
# C sources
C_SOURCES = \
gpslib.c \
kineis_demo.c \
mangOH_Kim1.c \
previpass.c \
previpass_util.c


#######################################
# binaries
#######################################
PREFIX = arm-poky-linux-gnueabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.

CC = $(PREFIX)gcc
AR = $(PREFIX)ar -cvq

ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################

# C includes
C_INCLUDES = -I./


# compile gcc flags
CFLAGS = $(C_INCLUDES) $(OPT) -Wall -Werror -std=gnu11

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# libraries
LDFLAGS = -lm


#######################################
# build the library
#######################################

# default action: build all
all: $(BUILD_DIR)/$(TARGET).bin

# generate doc (doxygen framework), configure some dynamic parameters from Makefile
doc: Doxyfile
	(cat $< ; \
echo $(CFLAGS) | xargs -n1 echo | grep "^-D" | sed 's/-D//' | xargs echo PREDEFINED= ; \
echo OUTPUT_DIRECTORY=$(DOC_DIR) ; \
echo EXCLUDE=$(DOC_DIR) ; \
echo INCLUDE_PATH=$(subst -I,,$(C_INCLUDES)) ; \
echo WARN_LOGFILE=$(DOXY_WARN_LOGFILE)) \
| doxygen -
	(git log -n 1 > $(DOC_DIR)/version.txt)

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir $@

#######################################
# clean up
#######################################
clean:
	rm -rf $(BUILD_DIR)

doc_clean:
	-rm -fR $(DOC_DIR)
	-rm -f  $(DOXY_WARN_LOGFILE)

.PHONY: doc doc_clean
