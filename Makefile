
BIN	:= zforth
SRC	:= src/main.c src/zforth.c
OBJS    := $(subst .c,.o, $(SRC))
DEPS    := $(subst .c,.d, $(SRC))

CC	:= $(CROSS)gcc

CFLAGS  += -Os -g
CFLAGS  += -Wall -Wextra -Werror -Wno-unused-parameter -Wno-clobbered -ansi
CFLAGS	+= -Isrc
LDFLAGS	+= -g 

LDFLAGS += -lreadline -lm

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c Makefile zforth.h
	$(CC) $(CFLAGS) -MMD -c $<

clean:
	rm -f $(BIN) $(OBJS) $(DEPS)

lint:
	lint -i /opt/flint/supp/lnt -i src -w2 co-gcc.lnt \
		-e537 -e451 -e524 -e534 -e641 -e661 -e64 \
		$(SRC)

-include $(DEPS)

