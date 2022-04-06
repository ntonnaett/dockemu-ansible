FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive \
    TMPDIR=/tmp \
    TERM=linux \
    JACK_NO_AUDIO_RESERVATION=1 \
    JACK_SAMPLE_RATE=48000 \
    JACK_PERIOD=512\
#    JACKTRIP_VERSION=1.5.2
    BRANCH=io_stats_csv
#ENV JACKTRIP_TAR=https://github.com/jacktrip/jacktrip/archive/refs/tags/v${JACKTRIP_VERSION}.tar.gz \

# Install dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential ca-certificates locales vim git wget net-tools iputils-ping \
                                                  jackd2 alsa-utils libjack-jackd2-dev qtbase5-dev libqt5network5 mpg123 ecasound \
                                                  python3-pip python3-setuptools \
    && locale-gen en_US.UTF-8 \
    && pip3 install meson ninja pyyaml jinja2

# Download tar.gz
# RUN wget --progress=bar:force:noscroll -O ${TMPDIR}/jacktrip.tgz ${JACKTRIP_TAR} \
#     && tar xzvf jacktrip.tgz --strip-components=1 \
#     && rm jacktrip.tgz \

# Clone git repo
RUN git clone https://github.com/jacktrip/jacktrip.git ${TMPDIR} \
    && cd ${TMPDIR} \
    && git switch ${BRANCH}

# Install jacktrip
RUN export ARCH=`dpkg --print-architecture` \
    && cd ${TMPDIR} \
    && meson --buildtype=release -Dnogui=true builddir \
    && cd builddir \
    && ninja \
    && ninja install

COPY entrypoint.sh /sbin/entrypoint.sh
RUN chmod +x /sbin/entrypoint.sh

EXPOSE 4464
EXPOSE 61002-61100/udp

CMD entrypoint.sh > /var/log/dockemu/jacktrip-docker.log
