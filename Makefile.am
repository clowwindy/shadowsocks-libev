bin_PROGRAMS = local server testencrypt
local_SOURCES = encrypt.c encrypt.h local.c local.h
server_SOURCES = encrypt.c encrypt.h server.c server.h socks5.h
testencrypt_SOURCES = encrypt.c encrypt.h testencrypt.c

.PHONY: test

test: local server testencrypt
	./testencrypt
	python pytest/test.py


