BIN	:= zforth_fuzzer
SRC	:= main.c zforth.c

OBJS    := $(subst .c,.o, $(SRC))
DEPS    := $(subst .c,.d, $(SRC))

ifeq ($(CC),cc)
	CC := clang
endif

CC ?= clang

VPATH   := ../zforth
# For local build only, CI build supplies its own CFLAGS
CFLAGS	?= -Os -g -fsanitize=fuzzer,address -dict=zforth_fuzzer.dict -Wno-unused-command-line-argument

# Required flags
CFLAGS	+= -I. -I../zforth
CFLAGS  += -pedantic -MMD
CFLAGS  += -Wall -Wextra -Wno-unused-parameter -Wno-unused-result

LDFLAGS	+= $(CFLAGS) $(LIB_FUZZING_ENGINE)

LIBS	+= -lm

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(BIN) $(OBJS) $(DEPS)

-include $(DEPS)

