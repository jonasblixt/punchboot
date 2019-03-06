#!/bin/sh

rm -rf blobs/* \
&& mkdir -p blobs/ \
&& wget -q http://www.freescale.com/lgfiles/NMG/MAD/YOCTO/firmware-imx-8.0.bin \
&& mv firmware-imx-8.0.bin blobs/ \
&& cd blobs \
&& chmod +x firmware-imx-8.0.bin \
&& ./firmware-imx-8.0.bin \
&& rm -f firmware-imx-8.0.bin \
&& cd .. \
&& docker build -f pb.Dockerfile -t pb_docker_env .
