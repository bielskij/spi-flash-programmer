CROSS_COMPILE=avr-

CC      := $(CROSS_COMPILE)gcc
AS      := $(CROSS_COMPILE)gcc -x assembler-with-cpp
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size

ifneq ($(V),1)
CC      := @$(CC)
AS      := @$(AS)
OBJCOPY := @$(OBJCOPY)
OBJDUMP := @$(OBJDUMP)
SIZE    := @$(SIZE)
endif

MKDIR := @mkdir
RM    := @rm

DIR_OUT := out
DIR_INC := include
DIR_SRC := src

F_CPU ?= 16000000
BAUD  ?=  500000
UART  ?= /dev/ttyUSB0
MCU   := atmega328p

CFLAGS := -std=c99 
CFLAGS += -Wall -Wundef -Warray-bounds -Wformat -Wmissing-braces -Wreturn-type
CFLAGS += -fshort-enums -fpack-struct  -fno-split-wide-types -ffunction-sections -fdata-sections
CFLAGS += -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DBAUD=$(BAUD) -mrelax -O2

CFLAGS += -I$(DIR_SRC) -I$(DIR_INC) -I../../common/include -I../common/include

LDFLAGS += -Wl,--gc-sections

APP_NAME := firmware

# common src
SRC := \
	$(DIR_SRC)/main.c \
	$(DIR_SRC)/../../../common/src/common/crc8.c \
	$(DIR_SRC)/../../../common/src/common/protocol/common.c \
	$(DIR_SRC)/../../../common/src/common/protocol/packet.c \
	$(DIR_SRC)/../../../common/src/common/protocol/request.c \
	$(DIR_SRC)/../../../common/src/common/protocol/response.c \
	$(DIR_SRC)/../../common/src/firmware/programmer.c
	
TMP := $(foreach file, $(SRC), $(shell echo $(file) | sed -e 's|$(DIR_SRC)\/|$(DIR_OUT)\/|'))
OBJ := $(foreach file, $(TMP), $(shell echo $(file) | sed -e 's|\.c$$|.c.o|' | sed -e 's|\.S$$|.S.o|')) 

HEX_FILE ?= $(DIR_OUT)/$(APP_NAME).hex

init:
	$(MKDIR) -p $(DIR_OUT)

all: init \
	$(DIR_OUT)/$(APP_NAME).elf \
	$(DIR_OUT)/$(APP_NAME).lss \
	$(DIR_OUT)/$(APP_NAME).hex \
	$(DIR_OUT)/$(APP_NAME).eep \
	$(DIR_OUT)/$(APP_NAME).bin

	$(SIZE) --format=avr $(DIR_OUT)/$(APP_NAME).elf

burn: burn-arduino


burn-arduino:
	avrdude -p m328p -P $(UART) -c arduino -b57600 -U flash:w:$(HEX_FILE):i

burn-usbasp:
	# fuses used for atmega328p
	# -u -U efuse:w:0xFF:m -U hfuse:w:0xD9:m -U lfuse:w:0xF7:m 
	avrdude -p m328p -c usbasp -U flash:w:$(HEX_FILE):i

clean:
	$(RM) -rf $(DIR_OUT)

$(DIR_OUT)/%.c.o: $(DIR_SRC)/%.c
	@echo "Building `basename $@`"
	$(MKDIR) -p `dirname $@`
	$(CC) -c $(CFLAGS) -o $@ $<

$(DIR_OUT)/%.S.o: $(DIR_SRC)/%.S
	@echo "Building `basename $@`"
	$(MKDIR) -p `dirname $@`
	$(AS) -c $(CFLAGS) -o $@  $< 

$(DIR_OUT)/%.elf: $(OBJ)
	@echo "Building binary... $(APP_NAME).elf"
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
	
$(DIR_OUT)/%.lss: $(DIR_OUT)/%.elf
	@echo "Creating Extended Listing... $@"
	$(OBJDUMP) -h -S $< > $@
	
$(DIR_OUT)/%.hex: $(DIR_OUT)/%.elf
	@echo "Create Flash image (ihex format)... $@"
	$(OBJCOPY) -R .eeprom -O ihex $< $@

$(DIR_OUT)/%.bin: $(DIR_OUT)/%.elf
	@echo "Create Flash image (binary format)... $@"
	$(OBJCOPY) $(OBJCOPY_BIN_FLAGS) -R .eeprom -O binary $< $@
	
$(DIR_OUT)/%.eep: $(DIR_OUT)/%.elf
	@echo "Create eeprom image (ihex format)... $@"
	$(OBJCOPY) -j .eeprom -O ihex $< $@
