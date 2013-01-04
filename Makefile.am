bin_PROGRAMS = local server
local_SOURCES = encrypt.c encrypt.h local.c local.h
server_SOURCES = encrypt.c encrypt.h server.c server.h socks5.h

.PHONY: test

test: local server
	python test/test.py


