FROM ubuntu:xenial

RUN echo "deb mirror://mirrors.ubuntu.com/mirrors.txt xenial main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb mirror://mirrors.ubuntu.com/mirrors.txt xenial-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb mirror://mirrors.ubuntu.com/mirrors.txt xenial-security main restricted universe multiverse" >> /etc/apt/sources.list && \
    DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install -y \
    apt-transport-https \
    openssl \
    qemu-system-common \
    python3 \
    ca-certificates \
    curl \
    gdisk \
    gnupg2 \
    software-properties-common \
    wget \
    srecord \
    build-essential \
    u-boot-tools \
    python3-ecdsa \
    python3-crypto \
    qemu-system-arm \
    realpath \
    device-tree-compiler \
    git \
    bc \
    ruby \
    zlib1g-dev \
    pkg-config \
    libssl-dev \
    libpkcs11-helper1 \
    libpkcs11-helper1-dev \
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
&& gem i bit-struct \
&& rm -rf /var/lib/apt/lists/* \
&& git clone --depth 1 -b imx_4.14.98_2.0.0_ga git://source.codeaurora.org/external/imx/imx-mkimage.git imx-mkimage-imx8x \
&& git clone --depth 1 -b imx_4.9.51_imx8m_ga git://source.codeaurora.org/external/imx/imx-mkimage.git imx-mkimage-imx8m \
&& cd /imx-mkimage-imx8x && make SOC=iMX8QX REVISION=B0 bin \
&& cp /imx-mkimage-imx8x/mkimage_imx8 /usr/local/bin/mkimage_imx8 \
&& make clean \
&& cd /imx-mkimage-imx8m \
&& make SOC=iMX8M mkimage_imx8 \
&& cp /imx-mkimage-imx8m/iMX8M/mkimage_imx8 /usr/local/bin/mkimage_imx8_imx8m \
&& cd /

