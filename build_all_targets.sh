#!/bin/bash

set -e

DOCKER_ARGS="-it --rm -u $(id -u $USER) -v $(readlink -f .):$(readlink -f .) -u ${UID} -w ${PWD} pb_docker_env"

rm -rf build-*

cp configs/bebop_defconfig .config
docker run $DOCKER_ARGS make CROSS_COMPILE=arm-none-eabi- BOARD=board/bebop -j $(nproc)

cp configs/jiffy_defconfig .config
docker run $DOCKER_ARGS make CROSS_COMPILE=arm-none-eabi- BOARD=board/jiffy -j $(nproc)

cp configs/qemuarmv7a_defconfig .config
docker run $DOCKER_ARGS make CROSS_COMPILE=arm-none-eabi- BOARD=board/qemuarmv7 -j $(nproc)

cp configs/test_defconfig .config
docker run $DOCKER_ARGS make CROSS_COMPILE=arm-none-eabi- BOARD=board/test -j $(nproc)

cp configs/imx8qxmek_defconfig .config
docker run $DOCKER_ARGS make CROSS_COMPILE=aarch64-linux-gnu- BOARD=board/imx8qxmek MKIMAGE=mkimage_imx8 -j $(nproc)

cp configs/imx8mevk_defconfig .config
docker run $DOCKER_ARGS make CROSS_COMPILE=aarch64-linux-gnu- BOARD=board/imx8mevk MKIMAGE=mkimage_imx8_imx8m -j $(nproc)

cp configs/pico8ml_defconfig .config
docker run $DOCKER_ARGS make CROSS_COMPILE=aarch64-linux-gnu- BOARD=board/pico8ml MKIMAGE=mkimage_imx8_imx8m -j $(nproc)
