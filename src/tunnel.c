/*
 * tunnel.c - Setup a local port forwarding through remote shadowsocks server
 *
 * Copyright (C) 2013 - 2015, Max Lv <max.c.lv@gmail.com>
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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef __MINGW32__
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_SYS_IOCTL_H) && defined(HAVE_NET_IF_H) && defined(__linux__)
#include <net/if.h>
#include <sys/ioctl.h>
#define SET_INTERFACE
#endif

#ifdef __MINGW32__
#include "win32.h"
#endif

#include <libcork/core.h>
#include <udns.h>

#include "netutils.h"
#include "utils.h"
#include "tunnel.h"

#ifndef EAGAIN
#define EAGAIN EWOULDBLOCK
#endif

#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

#ifndef BUF_SIZE
#define BUF_SIZE 2048
#endif

static void accept_cb(EV_P_ ev_io *w, int revents);
static void server_recv_cb(EV_P_ ev_io *w, int revents);
static void server_send_cb(EV_P_ ev_io *w, int revents);
static void remote_recv_cb(EV_P_ ev_io *w, int revents);
static void remote_send_cb(EV_P_ ev_io *w, int revents);

static struct remote * new_remote(int fd, int timeout);
static struct server * new_server(int fd, int method);

static void free_remote(struct remote *remote);
static void close_and_free_remote(EV_P_ struct remote *remote);
static void free_server(struct server *server);
static void close_and_free_server(EV_P_ struct server *server);

#ifdef ANDROID
int vpn = 0;
#endif
int verbose = 0;

static int mode = TCP_ONLY;
static int auth = 0;

#ifndef __MINGW32__
static int setnonblocking(int fd)
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        flags = 0;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
#endif

#ifdef SET_INTERFACE
int setinterface(int socket_fd, const char * interface_name)
{
    struct ifreq interface;
    memset(&interface, 0, sizeof(interface));
    strncpy(interface.ifr_name, interface_name, IFNAMSIZ);
    int res = setsockopt(socket_fd, SOL_SOCKET, SO_BINDTODEVICE, &interface,
                         sizeof(struct ifreq));
    return res;
}
#endif

int create_and_bind(const char *addr, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, listen_sock;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */

    s = getaddrinfo(addr, port, &hints, &result);
    if (s != 0) {
        LOGI("getaddrinfo: %s", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_sock == -1) {
            continue;
        }

        int opt = 1;
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_NOSIGPIPE
        setsockopt(listen_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

        s = bind(listen_sock, rp->ai_addr, rp->ai_addrlen);
        if (s == 0) {
            /* We managed to bind successfully! */
            break;
        } else {
            ERROR("bind");
        }

        close(listen_sock);
    }

    if (rp == NULL) {
        LOGE("Could not bind");
        return -1;
    }

    freeaddrinfo(result);

    return listen_sock;
}

static void server_recv_cb(EV_P_ ev_io *w, int revents)
{
    struct server_ctx *server_recv_ctx = (struct server_ctx *)w;
    struct server *server = server_recv_ctx->server;
    struct remote *remote = server->remote;

    if (remote == NULL) {
        close_and_free_server(EV_A_ server);
        return;
    }

    ssize_t r = recv(server->fd, remote->buf, BUF_SIZE, 0);

    if (r == 0) {
        // connection closed
        close_and_free_remote(EV_A_ remote);
        close_and_free_server(EV_A_ server);
        return;
    } else if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data
            // continue to wait for recv
            return;
        } else {
            ERROR("server recv");
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        }
    }

    if (auth) {
        remote->buf = ss_gen_hash(remote->buf, &r, &remote->counter, server->e_ctx, BUF_SIZE);
    }

    remote->buf = ss_encrypt(BUF_SIZE, remote->buf, &r, server->e_ctx);

    if (remote->buf == NULL) {
        LOGE("invalid password or cipher");
        close_and_free_remote(EV_A_ remote);
        close_and_free_server(EV_A_ server);
        return;
    }

    int s = send(remote->fd, remote->buf, r, 0);

    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for send
            remote->buf_len = r;
            remote->buf_idx = 0;
            ev_io_stop(EV_A_ & server_recv_ctx->io);
            ev_io_start(EV_A_ & remote->send_ctx->io);
            return;
        } else {
            ERROR("send");
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        }
    } else if (s < r) {
        remote->buf_len = r - s;
        remote->buf_idx = s;
        ev_io_stop(EV_A_ & server_recv_ctx->io);
        ev_io_start(EV_A_ & remote->send_ctx->io);
        return;
    }
}

static void server_send_cb(EV_P_ ev_io *w, int revents)
{
    struct server_ctx *server_send_ctx = (struct server_ctx *)w;
    struct server *server = server_send_ctx->server;
    struct remote *remote = server->remote;
    if (server->buf_len == 0) {
        // close and free
        close_and_free_remote(EV_A_ remote);
        close_and_free_server(EV_A_ server);
        return;
    } else {
        // has data to send
        ssize_t s = send(server->fd, server->buf + server->buf_idx,
                         server->buf_len, 0);
        if (s < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ERROR("send");
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
            }
            return;
        } else if (s < server->buf_len) {
            // partly sent, move memory, wait for the next time to send
            server->buf_len -= s;
            server->buf_idx += s;
            return;
        } else {
            // all sent out, wait for reading
            server->buf_len = 0;
            server->buf_idx = 0;
            ev_io_stop(EV_A_ & server_send_ctx->io);
            if (remote != NULL) {
                ev_io_start(EV_A_ & remote->recv_ctx->io);
            } else {
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }
        }
    }

}

static void remote_timeout_cb(EV_P_ ev_timer *watcher, int revents)
{
    struct remote_ctx *remote_ctx = (struct remote_ctx *)(((void *)watcher)
                                                          - sizeof(ev_io));
    struct remote *remote = remote_ctx->remote;
    struct server *server = remote->server;

    if (verbose) {
        LOGI("TCP connection timeout");
    }

    ev_timer_stop(EV_A_ watcher);

    close_and_free_remote(EV_A_ remote);
    close_and_free_server(EV_A_ server);
}

static void remote_recv_cb(EV_P_ ev_io *w, int revents)
{
    struct remote_ctx *remote_recv_ctx = (struct remote_ctx *)w;
    struct remote *remote = remote_recv_ctx->remote;
    struct server *server = remote->server;

    ssize_t r = recv(remote->fd, server->buf, BUF_SIZE, 0);

    if (r == 0) {
        // connection closed
        close_and_free_remote(EV_A_ remote);
        close_and_free_server(EV_A_ server);
        return;
    } else if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data
            // continue to wait for recv
            return;
        } else {
            ERROR("remote recv");
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        }
    }

    server->buf = ss_decrypt(BUF_SIZE, server->buf, &r, server->d_ctx);

    if (server->buf == NULL) {
        LOGE("invalid password or cipher");
        close_and_free_remote(EV_A_ remote);
        close_and_free_server(EV_A_ server);
        return;
    }

    int s = send(server->fd, server->buf, r, 0);

    if (s == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // no data, wait for send
            server->buf_len = r;
            server->buf_idx = 0;
            ev_io_stop(EV_A_ & remote_recv_ctx->io);
            ev_io_start(EV_A_ & server->send_ctx->io);
            return;
        } else {
            ERROR("send");
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        }
    } else if (s < r) {
        server->buf_len = r - s;
        server->buf_idx = s;
        ev_io_stop(EV_A_ & remote_recv_ctx->io);
        ev_io_start(EV_A_ & server->send_ctx->io);
        return;
    }
}

static void remote_send_cb(EV_P_ ev_io *w, int revents)
{
    struct remote_ctx *remote_send_ctx = (struct remote_ctx *)w;
    struct remote *remote = remote_send_ctx->remote;
    struct server *server = remote->server;

    if (!remote_send_ctx->connected) {
        struct sockaddr_storage addr;
        socklen_t len = sizeof addr;

        int r = getpeername(remote->fd, (struct sockaddr *)&addr, &len);
        if (r == 0) {
            remote_send_ctx->connected = 1;
            ev_io_stop(EV_A_ & remote_send_ctx->io);
            ev_timer_stop(EV_A_ & remote_send_ctx->watcher);
            char *ss_addr_to_send = malloc(BUF_SIZE);
            ssize_t addr_len = 0;

            ss_addr_t *sa = &server->destaddr;
            struct cork_ip ip;
            if (cork_ip_init(&ip, sa->host) != -1) {
                if (ip.version == 4) {
                    // send as IPv4
                    struct in_addr host;
                    int host_len = sizeof(struct in_addr);

                    if (dns_pton(AF_INET, sa->host, &host) == -1) {
                        FATAL("IP parser error");
                    }
                    ss_addr_to_send[addr_len++] = 1;
                    memcpy(ss_addr_to_send + addr_len, &host, host_len);
                    addr_len += host_len;
                } else if (ip.version == 6) {
                    // send as IPv6
                    struct in6_addr host;
                    int host_len = sizeof(struct in6_addr);

                    if (dns_pton(AF_INET6, sa->host, &host) == -1) {
                        FATAL("IP parser error");
                    }
                    ss_addr_to_send[addr_len++] = 4;
                    memcpy(ss_addr_to_send + addr_len, &host, host_len);
                    addr_len += host_len;
                } else {
                    FATAL("IP parser error");
                }
            } else {
                // send as domain
                int host_len = strlen(sa->host);

                ss_addr_to_send[addr_len++] = 3;
                ss_addr_to_send[addr_len++] = host_len;
                memcpy(ss_addr_to_send + addr_len, sa->host, host_len);
                addr_len += host_len;
            }

            uint16_t port = htons(atoi(sa->port));
            memcpy(ss_addr_to_send + addr_len, &port, 2);
            addr_len += 2;

            if (auth) {
                ss_addr_to_send[0] |= ONETIMEAUTH_FLAG;
                ss_onetimeauth(ss_addr_to_send + addr_len, ss_addr_to_send, addr_len, server->e_ctx->evp.iv);
                addr_len += ONETIMEAUTH_BYTES;
            }

            ss_addr_to_send = ss_encrypt(BUF_SIZE, ss_addr_to_send, &addr_len,
                                         server->e_ctx);
            if (ss_addr_to_send == NULL) {
                LOGE("invalid password or cipher");
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }

            int s = send(remote->fd, ss_addr_to_send, addr_len, 0);
            free(ss_addr_to_send);

            if (s < addr_len) {
                LOGE("failed to send addr");
                close_and_free_remote(EV_A_ remote);
                close_and_free_server(EV_A_ server);
                return;
            }

            ev_io_start(EV_A_ & remote->recv_ctx->io);
            ev_io_start(EV_A_ & server->recv_ctx->io);

            return;
        } else {
            ERROR("getpeername");
            // not connected
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        }
    } else {
        if (remote->buf_len == 0) {
            // close and free
            close_and_free_remote(EV_A_ remote);
            close_and_free_server(EV_A_ server);
            return;
        } else {
            // has data to send
            ssize_t s = send(remote->fd, remote->buf + remote->buf_idx,
                             remote->buf_len, 0);
            if (s < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    ERROR("send");
                    // close and free
                    close_and_free_remote(EV_A_ remote);
                    close_and_free_server(EV_A_ server);
                }
                return;
            } else if (s < remote->buf_len) {
                // partly sent, move memory, wait for the next time to send
                remote->buf_len -= s;
                remote->buf_idx += s;
                return;
            } else {
                // all sent out, wait for reading
                remote->buf_len = 0;
                remote->buf_idx = 0;
                ev_io_stop(EV_A_ & remote_send_ctx->io);
                ev_io_start(EV_A_ & server->recv_ctx->io);
            }
        }

    }
}

static struct remote * new_remote(int fd, int timeout)
{
    struct remote *remote;
    remote = malloc(sizeof(struct remote));

    memset(remote, 0, sizeof(struct remote));

    remote->buf = malloc(BUF_SIZE);
    remote->recv_ctx = malloc(sizeof(struct remote_ctx));
    remote->send_ctx = malloc(sizeof(struct remote_ctx));
    remote->fd = fd;
    ev_io_init(&remote->recv_ctx->io, remote_recv_cb, fd, EV_READ);
    ev_io_init(&remote->send_ctx->io, remote_send_cb, fd, EV_WRITE);
    ev_timer_init(&remote->send_ctx->watcher, remote_timeout_cb,
                  min(MAX_CONNECT_TIMEOUT, timeout), 0);
    remote->recv_ctx->remote = remote;
    remote->recv_ctx->connected = 0;
    remote->send_ctx->remote = remote;
    remote->send_ctx->connected = 0;
    remote->buf_len = 0;
    remote->buf_idx = 0;
    return remote;
}

static void free_remote(struct remote *remote)
{
    if (remote != NULL) {
        if (remote->server != NULL) {
            remote->server->remote = NULL;
        }
        if (remote->buf) {
            free(remote->buf);
        }
        free(remote->recv_ctx);
        free(remote->send_ctx);
        free(remote);
    }
}

static void close_and_free_remote(EV_P_ struct remote *remote)
{
    if (remote != NULL) {
        ev_timer_stop(EV_A_ & remote->send_ctx->watcher);
        ev_io_stop(EV_A_ & remote->send_ctx->io);
        ev_io_stop(EV_A_ & remote->recv_ctx->io);
        close(remote->fd);
        free_remote(remote);
    }
}

static struct server * new_server(int fd, int method)
{
    struct server *server;
    server = malloc(sizeof(struct server));
    server->buf = malloc(BUF_SIZE);
    server->recv_ctx = malloc(sizeof(struct server_ctx));
    server->send_ctx = malloc(sizeof(struct server_ctx));
    server->fd = fd;
    ev_io_init(&server->recv_ctx->io, server_recv_cb, fd, EV_READ);
    ev_io_init(&server->send_ctx->io, server_send_cb, fd, EV_WRITE);
    server->recv_ctx->server = server;
    server->recv_ctx->connected = 0;
    server->send_ctx->server = server;
    server->send_ctx->connected = 0;
    if (method) {
        server->e_ctx = malloc(sizeof(struct enc_ctx));
        server->d_ctx = malloc(sizeof(struct enc_ctx));
        enc_ctx_init(method, server->e_ctx, 1);
        enc_ctx_init(method, server->d_ctx, 0);
    } else {
        server->e_ctx = NULL;
        server->d_ctx = NULL;
    }
    server->buf_len = 0;
    server->buf_idx = 0;
    return server;
}

static void free_server(struct server *server)
{
    if (server != NULL) {
        if (server->remote != NULL) {
            server->remote->server = NULL;
        }
        if (server->e_ctx != NULL) {
            cipher_context_release(&server->e_ctx->evp);
            free(server->e_ctx);
        }
        if (server->d_ctx != NULL) {
            cipher_context_release(&server->d_ctx->evp);
            free(server->d_ctx);
        }
        if (server->buf) {
            free(server->buf);
        }
        free(server->recv_ctx);
        free(server->send_ctx);
        free(server);
    }
}

static void close_and_free_server(EV_P_ struct server *server)
{
    if (server != NULL) {
        ev_io_stop(EV_A_ & server->send_ctx->io);
        ev_io_stop(EV_A_ & server->recv_ctx->io);
        close(server->fd);
        free_server(server);
    }
}

static void accept_cb(EV_P_ ev_io *w, int revents)
{
    struct listen_ctx *listener = (struct listen_ctx *)w;
    int serverfd = accept(listener->fd, NULL, NULL);
    if (serverfd == -1) {
        ERROR("accept");
        return;
    }
    setnonblocking(serverfd);
    int opt = 1;
    setsockopt(serverfd, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt));
#ifdef SO_NOSIGPIPE
    setsockopt(serverfd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

    int index = rand() % listener->remote_num;
    struct sockaddr *remote_addr = listener->remote_addr[index];

    int remotefd = socket(remote_addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (remotefd < 0) {
        ERROR("socket");
        return;
    }

#ifdef ANDROID
    if (vpn) {
        if (protect_socket(remotefd) == -1) {
            ERROR("protect_socket");
            close(remotefd);
            return;
        }
    }
#endif

    setsockopt(remotefd, SOL_TCP, TCP_NODELAY, &opt, sizeof(opt));
#ifdef SO_NOSIGPIPE
    setsockopt(remotefd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
#endif

    // Setup
    setnonblocking(remotefd);
#ifdef SET_INTERFACE
    if (listener->iface) {
        setinterface(remotefd, listener->iface);
    }
#endif

    struct server *server = new_server(serverfd, listener->method);
    struct remote *remote = new_remote(remotefd, listener->timeout);
    server->destaddr = listener->tunnel_addr;
    server->remote = remote;
    remote->server = server;

    connect(remotefd, remote_addr, get_sockaddr_len(remote_addr));
    // listen to remote connected event
    ev_io_start(EV_A_ & remote->send_ctx->io);
    ev_timer_start(EV_A_ & remote->send_ctx->watcher);
}

int main(int argc, char **argv)
{

    int i, c;
    int pid_flags = 0;
    char *user = NULL;
    char *local_port = NULL;
    char *local_addr = NULL;
    char *password = NULL;
    char *timeout = NULL;
    char *method = NULL;
    char *pid_path = NULL;
    char *conf_path = NULL;
    char *iface = NULL;

    int remote_num = 0;
    ss_addr_t remote_addr[MAX_REMOTE_NUM];
    char *remote_port = NULL;

    ss_addr_t tunnel_addr = { .host = NULL, .port = NULL };
    char *tunnel_addr_str = NULL;

    opterr = 0;

    USE_TTY();

#ifdef ANDROID
    while ((c = getopt(argc, argv, "f:s:p:l:k:t:m:i:c:b:L:a:uUvVA")) != -1) {
#else
    while ((c = getopt(argc, argv, "f:s:p:l:k:t:m:i:c:b:L:a:uUvA")) != -1) {
#endif
        switch (c) {
        case 's':
            if (remote_num < MAX_REMOTE_NUM) {
                remote_addr[remote_num].host = optarg;
                remote_addr[remote_num++].port = NULL;
            }
            break;
        case 'p':
            remote_port = optarg;
            break;
        case 'l':
            local_port = optarg;
            break;
        case 'k':
            password = optarg;
            break;
        case 'f':
            pid_flags = 1;
            pid_path = optarg;
            break;
        case 't':
            timeout = optarg;
            break;
        case 'm':
            method = optarg;
            break;
        case 'c':
            conf_path = optarg;
            break;
        case 'i':
            iface = optarg;
            break;
        case 'b':
            local_addr = optarg;
            break;
        case 'u':
            mode = TCP_AND_UDP;
            break;
        case 'U':
            mode = UDP_ONLY;
            break;
        case 'L':
            tunnel_addr_str = optarg;
            break;
        case 'a':
            user = optarg;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'A':
            auth = 1;
            break;
#ifdef ANDROID
        case 'V':
            vpn = 1;
            break;
#endif
        }
    }

    if (opterr) {
        usage();
        exit(EXIT_FAILURE);
    }

    if (argc == 1) {
        if (conf_path == NULL) {
            conf_path = DEFAULT_CONF_PATH;
        }
    }

    if (conf_path != NULL) {
        jconf_t *conf = read_jconf(conf_path);
        if (remote_num == 0) {
            remote_num = conf->remote_num;
            for (i = 0; i < remote_num; i++) {
                remote_addr[i] = conf->remote_addr[i];
            }
        }
        if (remote_port == NULL) {
            remote_port = conf->remote_port;
        }
        if (local_addr == NULL) {
            local_addr = conf->local_addr;
        }
        if (local_port == NULL) {
            local_port = conf->local_port;
        }
        if (password == NULL) {
            password = conf->password;
        }
        if (method == NULL) {
            method = conf->method;
        }
        if (timeout == NULL) {
            timeout = conf->timeout;
        }
        if (auth == 0) {
            auth = conf->auth;
        }
    }

    if (remote_num == 0 || remote_port == NULL || tunnel_addr_str == NULL ||
        local_port == NULL || password == NULL) {
        usage();
        exit(EXIT_FAILURE);
    }

    if (timeout == NULL) {
        timeout = "60";
    }

    if (local_addr == NULL) {
        local_addr = "127.0.0.1";
    }

    if (pid_flags) {
        USE_SYSLOG(argv[0]);
        daemonize(pid_path);
    }

    if (auth) {
        LOGI("onetime authentication enabled");
    }

    // parse tunnel addr
    parse_addr(tunnel_addr_str, &tunnel_addr);

    if (tunnel_addr.port == NULL) {
        FATAL("tunnel port is not defined");
    }

#ifdef __MINGW32__
    winsock_init();
#else
    // ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    signal(SIGABRT, SIG_IGN);
#endif

    // Setup keys
    LOGI("initialize ciphers... %s", method);
    int m = enc_init(password, method);

    // Setup proxy context
    struct listen_ctx listen_ctx;
    listen_ctx.tunnel_addr = tunnel_addr;
    listen_ctx.remote_num = remote_num;
    listen_ctx.remote_addr = malloc(sizeof(struct sockaddr *) * remote_num);
    for (i = 0; i < remote_num; i++) {
        char *host = remote_addr[i].host;
        char *port = remote_addr[i].port == NULL ? remote_port :
                     remote_addr[i].port;
        struct sockaddr_storage *storage = malloc(sizeof(struct sockaddr_storage));
        memset(storage, 0, sizeof(struct sockaddr_storage));
        if (get_sockaddr(host, port, storage, 1) == -1) {
            FATAL("failed to resolve the provided hostname");
        }
        listen_ctx.remote_addr[i] = (struct sockaddr *)storage;
    }
    listen_ctx.timeout = atoi(timeout);
    listen_ctx.iface = iface;
    listen_ctx.method = m;

    struct ev_loop *loop = EV_DEFAULT;

    if (mode != UDP_ONLY) {
        // Setup socket
        int listenfd;
        listenfd = create_and_bind(local_addr, local_port);
        if (listenfd < 0) {
            FATAL("bind() error:");
        }
        if (listen(listenfd, SOMAXCONN) == -1) {
            FATAL("listen() error:");
        }
        setnonblocking(listenfd);

        listen_ctx.fd = listenfd;

        ev_io_init(&listen_ctx.io, accept_cb, listenfd, EV_READ);
        ev_io_start(loop, &listen_ctx.io);
    }

    // Setup UDP
    if (mode != TCP_ONLY) {
        LOGI("UDP relay enabled");
        init_udprelay(local_addr, local_port, listen_ctx.remote_addr[0],
                      get_sockaddr_len(listen_ctx.remote_addr[0]),
                      tunnel_addr, m, auth, listen_ctx.timeout, iface);
    }

    if (mode == UDP_ONLY) {
        LOGI("TCP relay disabled");
    }

    LOGI("listening at %s:%s", local_addr, local_port);

    // setuid
    if (user != NULL) {
        run_as(user);
    }

    ev_run(loop, 0);

#ifdef __MINGW32__
    winsock_cleanup();
#endif

    return 0;
}

