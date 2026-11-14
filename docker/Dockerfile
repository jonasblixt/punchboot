# Punchboot build and test environment.
#
# Contains the cross toolchains, Mbed TLS sources, the bpak CLI, the i.MX image
# tools (mkimage, NXP's mkimage_imx8m and cst) and the test tools (CMocka, pytest, gcovr) for the
# firmware and host builds. The punchboot
# sources are not part of the image; they are bind mounted at runtime.
#
# Use scripts/pbdev rather than building/running this directly.

FROM debian:trixie-slim@sha256:62bd423aeee912639ca8ee681c2b762cb3e25137e0a92b20be455898a9f034ad

ARG MBEDTLS_VERSION=3.4.0
ARG MBEDTLS_SHA256=1b899f355022e8d02c4d313196a0a16af86c5a692456fa99d302915b8cf0320a
ARG BPAK_VERSION=1.0.1
# NXP's imx-mkimage, the version and hash the test bench's Buildroot has
ARG IMX_MKIMAGE_VERSION=lf-6.6.23-2.0.0
ARG IMX_MKIMAGE_SHA256=0a5654229c063e4e87b5d69f47a4a913ecee51f8a817ae2346a2a407e765ed3b

RUN apt-get update && apt-get install -y --no-install-recommends \
        bc \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        file \
        gcc-aarch64-linux-gnu \
        gcc-arm-none-eabi \
        gdisk \
        git \
        imx-code-signing-tool \
        libc6-dev-arm64-cross \
        libcmocka-dev \
        liblzma-dev \
        libmbedtls-dev \
        pkg-config \
        python3 \
        python3-dev \
        python3-venv \
        u-boot-tools \
        uuid-dev \
        xxd \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# NXP's mkimage_imx8m, the i.MX8MQ's image tool (src/plat/imx8m/makefile.mk):
# imx-mkimage's iMX8M/mkimage_imx8.c alone. U-Boot's mkimage makes the 8M
# Plus's image
RUN curl -fsSL -o /tmp/imx-mkimage.tar.gz \
        https://github.com/nxp-imx/imx-mkimage/archive/${IMX_MKIMAGE_VERSION}/imx-mkimage-${IMX_MKIMAGE_VERSION}.tar.gz \
    && echo "${IMX_MKIMAGE_SHA256}  /tmp/imx-mkimage.tar.gz" | sha256sum -c - \
    && tar xzf /tmp/imx-mkimage.tar.gz -C /tmp \
    && cd /tmp/imx-mkimage-${IMX_MKIMAGE_VERSION} \
    && gcc -O2 -I iMX8M -I src -o /usr/local/bin/mkimage_imx8m iMX8M/mkimage_imx8.c -lz \
    && cd / && rm -rf /tmp/imx-mkimage.tar.gz /tmp/imx-mkimage-${IMX_MKIMAGE_VERSION}

# Mbed TLS sources, compiled into the firmware via MBEDTLS_DIR.
RUN curl -fsSL -o /tmp/mbedtls.tar.gz \
        https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v${MBEDTLS_VERSION}.tar.gz \
    && echo "${MBEDTLS_SHA256}  /tmp/mbedtls.tar.gz" | sha256sum -c - \
    && tar xzf /tmp/mbedtls.tar.gz -C /opt \
    && rm /tmp/mbedtls.tar.gz
ENV MBEDTLS_DIR=/opt/mbedtls-${MBEDTLS_VERSION}

# Python environment for the punchboot package, coverage reports, the bpak
# CLI (the bpak python package since 1.0), which builds test images, and
# cryptography, which scripts/gen-keystore.py reads the board's keys with.
# The container runs as the invoking host user, so the locations that get
# installed into at runtime (venv, /opt/pb prefix for pbstate) are left
# world writable. Containers are ephemeral, so nothing
# persists between runs.
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir \
        "click>=8.2,<9" \
        "cryptography>=42" \
        "semver>=3,<4" \
        "pyusb>=1.2,<2" \
        "libusb-package>=1.0.26" \
        "setuptools" \
        "wheel" \
        "gcovr==8.3" \
        "pytest" \
        "bpak==${BPAK_VERSION}" \
    && mkdir -p /opt/pb \
    && chmod -R a+rwX /opt/venv /opt/pb

ENV PATH=/opt/pb/bin:/opt/venv/bin:$PATH \
    LD_LIBRARY_PATH=/opt/pb/lib \
    HOME=/tmp/home
