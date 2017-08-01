/*
 * socks5.h - Define SOCKS5's header
 *
 * Copyright (C) 2013, clowwindy <clowwindy42@gmail.com>
 *
 * This file is part of the shadowsocks-libev.
 *
 * shadowsocks-libev is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * shadowsocks-libev is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with shadowsocks-libev; see the file COPYING. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef _SOCKS5_H
#define _SOCKS5_H

#define SVERSION 0x05
#define CONNECT 0x01
#define IPV4 0x01
#define DOMAIN 0x03
#define IPV6 0x04
#define CMD_NOT_SUPPORTED 0x07

struct method_select_request {
    unsigned char ver;
    unsigned char nmethods;
    unsigned char methods[0];
} __attribute__((packed, aligned(1)));

struct method_select_response {
    unsigned char ver;
    unsigned char method;
} __attribute__((packed, aligned(1)));

struct socks5_request {
    unsigned char ver;
    unsigned char cmd;
    unsigned char rsv;
    unsigned char atyp;
} __attribute__((packed, aligned(1)));

struct socks5_response {
    unsigned char ver;
    unsigned char rep;
    unsigned char rsv;
    unsigned char atyp;
} __attribute__((packed, aligned(1)));

#endif // _SOCKS5_H
