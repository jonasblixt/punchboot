#!/bin/bash

docker run  -it --rm \
            -u $(id -u $USER) \
            -v $(readlink -f .):$(readlink -f .) \
            --privileged \
            -v /dev/bus/usb:/dev/bus/usb \
            -v /home/${USER}:/home/${USER} \
            -v /etc/passwd:/etc/passwd \
            -v /etc/group:/etc/group \
            -u ${UID} \
            -w ${PWD} pb_docker_env $@

