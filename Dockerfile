FROM alpine:3 AS builder
RUN apk add --update --no-cache git build-base cmake libsecp256k1-dev rocksdb-dev libsodium-dev zlib-dev gmp-dev ccache
WORKDIR /app
COPY . .
RUN git submodule update --init --recursive
RUN --mount=type=cache,target=/root/.cache/ccache sh make_release.sh "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache"

FROM alpine:3
RUN apk add --update --no-cache bash libsecp256k1 rocksdb libsodium zlib gmp libgomp
WORKDIR /app
COPY --from=builder /app/build ./build
COPY ["activate.sh", "run_*.sh", "docker-entrypoint.sh", "./"]
COPY config ./config
COPY kernel ./kernel
COPY www ./www

ENV MMX_HOME="/data/"
VOLUME /data

# node p2p port
EXPOSE 12336/tcp
# http api port
EXPOSE 11380/tcp

ENTRYPOINT ["./docker-entrypoint.sh"]
CMD ["./run_node.sh"]
