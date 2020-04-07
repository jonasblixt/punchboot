#!/bin/bash

if [ ! $# -eq 4 ]; then
    echo "Usage: createtoken.sh <UUID> <method> <hash> <param>"
    echo "  method can be either 'file' or 'pkcs11'"
    echo "  param should be a private key if method=file"
    echo "    or openssl sign command for pkcs11"
    echo
    echo " file example)"
    echo "  createtoken.sh 72fe2ffb-af0b-41aa-8fc3-d23db346d34a file sha256 key.pem"
    echo
    echo " pkcs11 example)"
    echo "  createtoken.sh 72fe2ffb-af0b-41aa-8fc3-d23db346d34a pkcs11 sha256 \"pkcs11:id=%02;type=private\""

    exit -1
fi

UUID_STR=$1
METHOD=$2
HASH=$3
PARAM=$4

OUTPUT_FN=$UUID_STR.token

if [ ! $? -eq 0 ]; then
    echo "Could not create temporary file"
    exit -1
fi

if [ $METHOD == "pkcs11" ]; then
    echo -n $UUID_STR | openssl dgst -engine pkcs11 -keyform engine -sign $PARAM -$HASH -out $OUTPUT_FN
elif [ $METHOD == "file" ]; then
    echo -n $UUID_STR | openssl dgst -sign $PARAM -$HASH -out $OUTPUT_FN
else
    echo "Unknown method"
    exit -1
fi
