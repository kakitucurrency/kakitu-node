# Railway deployment — builds and runs the Kakitu live network node
# See docker/node/ for detailed configuration

ARG ENV_REPOSITORY=nanocurrency/nano-env
ARG COMPILER=gcc
FROM ${ENV_REPOSITORY}:${COMPILER} AS builder

ARG NETWORK=live
ARG CI_TAG=DEV_BUILD
ARG CI_BUILD=OFF
ADD ./ /tmp/src

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
COPY docker/node/entry.sh /usr/bin/entry.sh
COPY docker/node/config /usr/share/kakitu/config
RUN chmod +x /usr/bin/entry.sh && ldconfig

WORKDIR /home/kakitu
USER kakitu

ENV PATH="${PATH}:/usr/bin"

# Node P2P port | RPC port | IPC port | WebSocket port
EXPOSE 44075 44076 44077 44078

ENTRYPOINT ["/usr/bin/entry.sh"]
CMD ["kakitu_node", "daemon", "-l"]

ARG REPOSITORY=kakitucurrency/kakitu-node
LABEL org.opencontainers.image.source https://github.com/$REPOSITORY
