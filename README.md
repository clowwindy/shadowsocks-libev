shadowsocks-libev
===========

***This project is currently maintaineb by @madeye***

***Please visit https://github.com/madeye/shadowsocks-libev***

[![Build Status](https://travis-ci.org/clowwindy/shadowsocks-libev.png)](https://travis-ci.org/clowwindy/shadowsocks-libev)  
Current Version: 0.1.3

shadowsocks-libev is a lightweight tunnel proxy which can help you get through
 firewalls. It is a port of [shadowsocks](https://github.com/clowwindy/shadowsocks).

Currently not stable yet.
 Please use [shadowsocks-nodejs](https://github.com/clowwindy/shadowsocks-nodejs).

installation
-----------

Edit local.c, change server hostname.

Install the following package:

    sudo apt-get install build-essential autoconf libtool libev4 libev-dev libssl-dev
    autoreconf
    ./configure && make

