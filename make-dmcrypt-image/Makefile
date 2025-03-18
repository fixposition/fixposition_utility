all: make-dmcrypt-image

CC        ?= gcc
CFLAGS    += -std=gnu99 -Og -ggdb
CFLAGS    += -Wall -Wextra -Werror -Wshadow -Wformat -Wpointer-arith -Wundef
LDFLAGS   += -lkcapi

make-dmcrypt-image: make-dmcrypt-image.c Makefile
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS)

.PHONY: clean
clean:
	rm -f make-dmcrypt-image
