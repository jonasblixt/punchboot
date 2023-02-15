FROM ubuntu:bionic

RUN echo "deb mirror://mirrors.ubuntu.com/mirrors.txt bionic main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb mirror://mirrors.ubuntu.com/mirrors.txt bionic-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb mirror://mirrors.ubuntu.com/mirrors.txt bionic-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install -y \
    apt-transport-https \
    openssl \
    qemu-system-common \
    ca-certificates \
    curl \
    gdisk \
    gnupg2 \
    software-properties-common \
    wget \
    srecord \
    build-essential \
    u-boot-tools \
    qemu-system-arm \
    device-tree-compiler \
    git \
    bc \
    zlib1g-dev \
    pkg-config \
    libssl-dev \
    libblkid1 \
    libblkid-dev \
    bison \
    libfl-dev \
    flex \
    byacc \
    libusb-1.0-0-dev \
    gcc-aarch64-linux-gnu \
    gcc-arm-none-eabi \
    binutils-aarch64-linux-gnu \
    binutils-arm-none-eabi \
    vim-common \
    autotools-dev \
    libmbedtls-dev \
    automake \
    autoconf-archive \
    autoconf \
    libtool \
    iproute2 \
    libtool-bin \
    uuid-runtime \
    sudo \
    libpkcs11-helper1 \
    libpkcs11-helper1-dev \
&& rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 -b imx_4.14.98_2.0.0_ga git://source.codeaurora.org/external/imx/imx-mkimage.git imx-mkimage-imx8x \
&& git clone --depth 1 -b imx_4.9.51_imx8m_ga git://source.codeaurora.org/external/imx/imx-mkimage.git imx-mkimage-imx8m \
&& cd /imx-mkimage-imx8x && make SOC=iMX8QX REVISION=B0 bin \
&& cp /imx-mkimage-imx8x/mkimage_imx8 /usr/local/bin/mkimage_imx8 \
&& make clean \
&& cd /imx-mkimage-imx8m \
&& make SOC=iMX8M mkimage_imx8 \
&& cp /imx-mkimage-imx8m/iMX8M/mkimage_imx8 /usr/local/bin/mkimage_imx8_imx8m \
&& cd / \
&& git clone https://github.com/jonasblixt/bpak --branch v0.3.4 --depth 1 \
&& cd /bpak && autoreconf -fi && ./configure && make && make install \
&& ldconfig \
&& cd / && git clone https://github.com/jonasblixt/punchboot-tools --branch v0.1.17 --depth 1 \
&& cd /punchboot-tools && autoreconf -fi && ./configure && make && make install \
&& ldconfig \
&& cd / && git clone https://github.com/jonasblixt/punchboot --depth 1 --branch v0.7.9 \
&& cd /punchboot/tools/pbstate && autoreconf -fi && ./configure && make && make install \
&& ldconfig \
&& cd / && git clone https://github.com/jonasblixt/nxpcst --depth 1 --branch v0.1.4 \
&& cd /nxpcst && autoreconf -fi && ./configure && make && make install \
&& rm -rf /punchboot /punchboot-tools /imx-mkimage-imx8m /imx-mkimage-imx8x /nxpcst /bpak
