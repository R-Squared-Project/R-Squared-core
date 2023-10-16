FROM phusion/baseimage:18.04-1.0.0 AS build

ENV LANG=en_US.UTF-8
RUN \
    apt-get update -y && \
    apt-get install -y \
      g++ \
      autoconf \
      cmake \
      git \
      libbz2-dev \
      libcurl4-openssl-dev \
      libssl-dev \
      libncurses-dev \
      libboost-thread-dev \
      libboost-iostreams-dev \
      libboost-date-time-dev \
      libboost-system-dev \
      libboost-filesystem-dev \
      libboost-program-options-dev \
      libboost-chrono-dev \
      libboost-test-dev \
      libboost-context-dev \
      libboost-regex-dev \
      libboost-coroutine-dev \
      libtool \
      doxygen \
      ca-certificates \
    && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ADD . /rsquared-core
WORKDIR /rsquared-core

# Compile
RUN \
    ( git submodule sync --recursive || \
      find `pwd`  -type f -name .git | \
      while read f; do \
        rel="$(echo "${f#$PWD/}" | sed 's=[^/]*/=../=g')"; \
        sed -i "s=: .*/.git/=: $rel/=" "$f"; \
      done && \
      git submodule sync --recursive ) && \
    git submodule update --init --recursive && \
    cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DGRAPHENE_DISABLE_UNITY_BUILD=ON \
        . && \
    make witness_node cli_wallet get_dev_key && \
    install -s programs/witness_node/witness_node programs/genesis_util/get_dev_key programs/cli_wallet/cli_wallet /usr/local/bin && \
    #
    # Obtain version
    mkdir /etc/rsquared && \
    git rev-parse --short HEAD > /etc/rsquared/version && \
    cd / && \
    rm -rf /rsquared-core

FROM phusion/baseimage:18.04-1.0.0 AS run

COPY --from=build /usr/local/bin /usr/local/bin
COPY --from=build /etc/rsquared/version /etc/rsquared/version

# Home directory $HOME
WORKDIR /
RUN useradd -s /bin/bash -m -d /var/lib/rsquared rsquared
ENV HOME /var/lib/rsquared
RUN chown rsquared:rsquared -R /var/lib/rsquared

# Volume
VOLUME ["/var/lib/rsquared", "/etc/rsquared"]

# rpc service:
EXPOSE 8090
# p2p service:
EXPOSE 2771

# default exec/config files
ADD docker/default_config.ini /etc/rsquared/config.ini
ADD docker/default_logging.ini /etc/rsquared/logging.ini
ADD docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod a+x /usr/local/bin/entrypoint.sh

# Make Docker send SIGINT instead of SIGTERM to the daemon
STOPSIGNAL SIGINT

# default execute entry
CMD ["/usr/local/bin/entrypoint.sh"]
