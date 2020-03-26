#!/bin/bash

docker run -it --rm \
            --privileged \
            -v /dev/bus/usb:/dev/bus/usb \
            -v $(readlink -f ..):/work \
            -v /home/${USER}:/home/${USER} \
            -v /etc/passwd:/etc/passwd \
            -v /etc/group:/etc/group \
            -u ${UID} \
            -w ${PWD} \
            --name pb-dev \
            pb


