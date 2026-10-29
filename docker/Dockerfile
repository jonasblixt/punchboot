# Punchboot build and test environment.
#
# Contains the cross toolchains, Mbed TLS sources, the bpak CLI, the i.MX image
# tools (mkimage, NXP cst) and the test tools (CMocka, pytest, gcovr) for the
# firmware and host builds. The punchboot
# sources are not part of the image; they are bind mounted at runtime.
#
# Use scripts/pbdev rather than building/running this directly.

FROM debian:trixie-slim@sha256:62bd423aeee912639ca8ee681c2b762cb3e25137e0a92b20be455898a9f034ad

ARG MBEDTLS_VERSION=3.4.0
ARG MBEDTLS_SHA256=1b899f355022e8d02c4d313196a0a16af86c5a692456fa99d302915b8cf0320a
ARG BPAK_VERSION=0.9.3
ARG BPAK_SHA256=043f50c0cf83b644870f6db2463937f79139e588588b74c61c7effa80ab533e5

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
    && rm -rf /var/lib/apt/lists/*

# Mbed TLS sources, compiled into the firmware via MBEDTLS_DIR.
RUN curl -fsSL -o /tmp/mbedtls.tar.gz \
        https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v${MBEDTLS_VERSION}.tar.gz \
    && echo "${MBEDTLS_SHA256}  /tmp/mbedtls.tar.gz" | sha256sum -c - \
    && tar xzf /tmp/mbedtls.tar.gz -C /opt \
    && rm /tmp/mbedtls.tar.gz
ENV MBEDTLS_DIR=/opt/mbedtls-${MBEDTLS_VERSION}

# bpak CLI, used to generate the keystore and to build test images.
RUN curl -fsSL -o /tmp/bpak.tar.gz \
        https://github.com/jonasblixt/bpak/archive/refs/tags/v${BPAK_VERSION}.tar.gz \
    && echo "${BPAK_SHA256}  /tmp/bpak.tar.gz" | sha256sum -c - \
    && tar xzf /tmp/bpak.tar.gz -C /tmp \
    && cmake -S /tmp/bpak-${BPAK_VERSION} -B /tmp/bpak-build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tmp/bpak-build -j"$(nproc)" \
    && cmake --install /tmp/bpak-build \
    && ldconfig \
    && rm -rf /tmp/bpak.tar.gz /tmp/bpak-${BPAK_VERSION} /tmp/bpak-build

# Python environment for the punchboot package and coverage reports. The
# container runs as the invoking host user, so the locations that get
# installed into at runtime (venv, /opt/pb prefix for pbstate) are left
# world writable. Containers are ephemeral, so nothing persists between runs.
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir \
        "click>=8.2,<9" \
        "semver>=3,<4" \
        "pyusb>=1.2,<2" \
        "libusb-package>=1.0.26" \
        "setuptools" \
        "wheel" \
        "gcovr==8.3" \
        "pytest" \
    && mkdir -p /opt/pb \
    && chmod -R a+rwX /opt/venv /opt/pb

ENV PATH=/opt/pb/bin:/opt/venv/bin:$PATH \
    LD_LIBRARY_PATH=/opt/pb/lib \
    HOME=/tmp/home
