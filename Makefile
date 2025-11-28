CC = gcc
CFLAGS=-Wall -Wextra -c -g -fPIC
# CFLAGS += -fsanitize=undefined,address
# LDFLAGS = -fsanitize=undefined,address

ifeq ($(RELEASE),TRUE)
#	LDFLAGS += -s
	CFLAGS += -O2
else
	CFLAGS += -O0
endif

SRCDIR = src

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(SRCDIR)/%.o,$(SRCS))

all: alloc.so alloc

alloc: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

alloc.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -o $@ $^

run:
	./alloc

clean:
	rm -f ./src/*.o
