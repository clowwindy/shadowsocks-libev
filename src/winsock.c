/*
 * winsock.c - Windows socket compatibility layer
 *
 * Copyright (C) 2013 - 2018, Max Lv <max.c.lv@gmail.com>
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

#ifdef __MINGW32__

#include "winsock.h"
#include "utils.h"

void
winsock_init(void)
{
    int ret;
    WSADATA wsa_data;
    ret = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (ret != 0) {
        FATAL("Failed to initialize winsock");
    }
}

void
winsock_cleanup(void)
{
    WSACleanup();
}

int
setnonblocking(SOCKET socket)
{
    u_long arg = 1;
    return ioctlsocket(socket, FIONBIO, &arg);
}

void
ss_error(const char *s)
{
    char *msg = NULL;
    DWORD err = WSAGetLastError();
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&msg, 0, NULL);
    if (msg != NULL) {
        // Remove trailing newline character
        ssize_t len = strlen(msg) - 1;
        if (len >= 0 && msg[len] == '\n') {
            msg[len] = '\0';
        }
        LOGE("%s: [%ld] %s", s, err, msg);
        LocalFree(msg);
    }
}

#endif // __MINGW32__
