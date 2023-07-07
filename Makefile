.POSIX:
.PHONY: all clean

CFLAGS=\
  -O0 \
  -march=native

CFLAGS_ALL=\
  -std=c99 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -Wdeclaration-after-statement \
  -Wformat=2 \
  -Wmissing-prototypes \
  -Wold-style-definition \
  -Wshadow \
  -Wstrict-prototypes \
  -D_XOPEN_SOURCE=700 \
  $(CFLAGS)

LIBS=-lX11 -lXss

LDFLAGS=
LDFLAGS_ALL=$(LDFLAGS) $(LIBS)

BIN=idlemon

OBJS=main.o util.o

all: $(BIN)

$(BIN): $(OBJS)
	@echo LD $@
	@$(CC) $(LDFLAGS_ALL) -o $@ $(OBJS)

clean:
	@echo CLEAN
	@rm -f $(BIN) $(OBJ) &> /dev/null

.c.o:
	@echo CC $@
	@$(CC) $(CFLAGS_ALL) -o $@  -c $<

