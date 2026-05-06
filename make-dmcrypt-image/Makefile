all: make-dmcrypt-image-kcapi make-dmcrypt-image-openssl

CC        ?= gcc
CFLAGS    += -std=gnu99 -Og -ggdb
CFLAGS    += -Wall -Wextra -Werror -Wshadow -Wformat -Wpointer-arith -Wundef

make-dmcrypt-image-kcapi: make-dmcrypt-image-kcapi.c Makefile
	$(CC) -o $@ $(CFLAGS) $< -lkcapi

make-dmcrypt-image-openssl: make-dmcrypt-image-openssl.c Makefile
	$(CC) -o $@ $(CFLAGS) $< -lcrypto

.PHONY: clean
clean:
	rm -f make-dmcrypt-image-kcapi make-dmcrypt-image-openssl
