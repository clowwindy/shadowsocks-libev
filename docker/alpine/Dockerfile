#
# Dockerfile for shadowsocks-libev
#

FROM alpine
LABEL maintainer="kev <noreply@datageek.info>, Sah <contact@leesah.name>"

ENV SERVER_ADDR 0.0.0.0
ENV PASSWORD=
ENV METHOD      aes-256-cfb
ENV TIMEOUT     300
ENV DNS_ADDR    8.8.8.8
ENV DNS_ADDR_2  8.8.4.4
ENV ARGS=

COPY . /tmp

RUN set -ex && \
    apk add --no-cache --virtual .build-deps \
                                git \
                                autoconf \
                                automake \
                                libtool \
                                build-base \
                                libev-dev \
                                linux-headers \
                                libsodium-dev \
                                mbedtls-dev \
                                pcre-dev \
                                c-ares-dev && \
    cd /tmp && \
    ./autogen.sh && \
    ./configure --prefix=/usr --disable-documentation && \
    make install && \

    runDeps="$( \
        scanelf --needed --nobanner /usr/bin/ss-* \
            | awk '{ gsub(/,/, "\nso:", $2); print "so:" $2 }' \
            | xargs -r apk info --installed \
            | sort -u \
    )" && \
    apk add --no-cache --virtual .run-deps \
                                rng-tools \
                                $runDeps && \
    apk del .build-deps && \
    rm -rf /tmp/*

USER nobody

EXPOSE 8388/tcp 8388/udp

CMD ss-server -s $SERVER_ADDR \
              -p 8388 \
              -k ${PASSWORD:-$(hostname)} \
              -m $METHOD \
              -t $TIMEOUT \
              --fast-open \
              -d $DNS_ADDR \
              -d $DNS_ADDR_2 \
              -u \
              $ARGS
