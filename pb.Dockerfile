FROM ubuntu:trusty

RUN echo "deb mirror://mirrors.ubuntu.com/mirrors.txt trusty main restricted universe multiverse" > /etc/apt/sources.list && \
    echo "deb mirror://mirrors.ubuntu.com/mirrors.txt trusty-updates main restricted universe multiverse" >> /etc/apt/sources.list && \
    echo "deb mirror://mirrors.ubuntu.com/mirrors.txt trusty-security main restricted universe multiverse" >> /etc/apt/sources.list && \
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
&& rm -rf /var/lib/apt/lists/* \
&& curl -sL https://releases.linaro.org/components/toolchain/binaries/7.3-2018.05/aarch64-linux-gnu/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz | tar xJv -C /opt \
&& curl -sL https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/7-2018q2/gcc-arm-none-eabi-7-2018-q2-update-linux.tar.bz2 | tar xjv -C /opt \
&& git clone -b imx_4.14.78_1.0.0_ga git://source.codeaurora.org/external/imx/imx-mkimage.git

RUN cd /imx-mkimage && make SOC=iMX8QX REVISION=B0 \
&& cp /imx-mkimage/mkimage_imx8 /usr/local/bin/mkimage_imx8x \
&& make clean \
&& cd /imx-mkimage && make SOC=iMX8M \
&& cp /imx-mkimage/mkimage_imx8 /usr/local/bin/mkimage_imx8m \
&& cd /

RUN echo "export PATH=\"/opt/gcc-arm-none-eabi-7-2018-q2-update/bin:/opt/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin:/imx-mkimage:\$PATH\"" >> ~/.bashrc
