
BIN	:= zforth
SRC	:= src/main.c src/zforth.c
OBJS    := $(subst .c,.o, $(SRC))
DEPS    := $(subst .c,.d, $(SRC))

CC	:= $(CROSS)gcc

CFLAGS  += -Os -g
CFLAGS  += -Wall -Wextra -Werror -Wno-unused-parameter -ansi
CFLAGS	+= -Isrc
LDFLAGS	+= -g 

LDFLAGS += -lreadline -lm

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c Makefile zforth.h
	$(CC) $(CFLAGS) -MMD -c $<

clean:
	rm -f $(BIN) $(OBJS) $(DEPS)

-include $(DEPS)

