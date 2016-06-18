
BIN	:= zforth-atmega8
SRC	:= main.c zforth.c

IHX	:= $(BIN).ihx
OBJS    := $(subst .c,.o, $(SRC))
DEPS    := $(subst .c,.d, $(SRC))

CC	:= avr-gcc

VPATH   := ../zforth
CFLAGS	+= -I. -I../zforth

CFLAGS	+= -DF_CPU=11059200
CFLAGS	+= -mmcu=atmega8
CFLAGS	+= -Os -g -ansi -MMD
CFLAGS	+= -Wall -Wextra -Werror -Wno-unused-parameter -Wno-clobbered -Wno-unused-result

LDFLAGS	+= -g
LDFLAGS	+= -mmcu=atmega8

CFLAGS  += -flto
LDFLAGS += -flto

$(IHX): $(BIN)
	avr-objcopy -j .text -j .data -O ihex $(BIN) $(IHX)
	avr-size $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: $(IHX)
	avrdude -p m8 -c avrispv2 -P usb -y -e -U flash:w:$(IHX):i

clean:
	rm -f $(IHX) $(BIN) $(OBJS) $(DEPS)

-include $(DEPS)

