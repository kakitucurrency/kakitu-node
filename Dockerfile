# Kakitu Node — Railway deployment Dockerfile
# Clones from GitHub with submodules so Boost/RocksDB etc. are available at build time

FROM nanocurrency/nano-env:gcc AS builder

ARG NETWORK=live
ARG CI_TAG=DEV_BUILD
ARG CI_BUILD=OFF
ARG GIT_BRANCH=main

# Clone with all submodules (Boost, RocksDB, cryptopp, etc. are git submodules)
RUN git clone --recursive --depth=1 --branch=${GIT_BRANCH} \
    https://github.com/kakitucurrency/kakitu-node.git /tmp/src

WORKDIR /tmp/build

RUN cmake /tmp/src \
    -DCI_BUILD=${CI_BUILD} \
    -DPORTABLE=1 \
    -DACTIVE_NETWORK=kshs_${NETWORK}_network \
    -DNANO_TEST=OFF \
    -DNANO_GUI=OFF && \
    make kakitu_node -j$(nproc) && \
    make kakitu_rpc -j$(nproc) && \
    echo ${NETWORK} > /etc/kakitu-network

FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends curl ca-certificates && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd --gid 1000 kakitu && \
    useradd --uid 1000 --gid kakitu --shell /bin/bash --create-home kakitu

COPY --from=builder /tmp/build/kakitu_node /usr/bin/kakitu_node
COPY --from=builder /tmp/build/kakitu_rpc /usr/bin/kakitu_rpc
COPY --from=builder /etc/kakitu-network /etc/kakitu-network
COPY --from=builder /tmp/src/docker/node/entry.sh /usr/bin/entry.sh
COPY --from=builder /tmp/src/docker/node/config /usr/share/kakitu/config
RUN chmod +x /usr/bin/entry.sh && ldconfig

WORKDIR /root

ENV PATH="${PATH}:/usr/bin"
# Data directory — mount a Railway Volume at this path for ledger persistence
ENV KAKITU_DATA=/home/kakitu

# Node P2P | RPC | IPC | WebSocket
EXPOSE 44075 44076 44077 44078

ENTRYPOINT ["/usr/bin/entry.sh"]
CMD ["kakitu_node", "daemon", "-l"]

ARG REPOSITORY=kakitucurrency/kakitu-node
LABEL org.opencontainers.image.source https://github.com/$REPOSITORY
