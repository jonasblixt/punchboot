#!/bin/bash

if [ ! $# -eq 4 ]; then
    echo "Usage: createtoken.sh <UUID> <method> <chksum> <param>"
    echo "  method can be either 'file' or 'pkcs11'"
    echo "  chksum should be '-sha256', '-sha384' or '-sha512'"
    echo "  param should be a private key if method=file"
    echo "    or openssl sign command for pkcs11"
    echo
    echo " file example)"
    echo "  createtoken.sh 72fe2ffb-af0b-41aa-8fc3-d23db346d34a file -sha384 key.pem"
    echo
    echo " pkcs11 example)"
    echo "  createtoken.sh 72fe2ffb-af0b-41aa-8fc3-d23db346d34a pkcs11 -sha384 \"pkcs11:id=%02;type=private\""

    exit -1
fi

UUID_STR=$1
METHOD=$2
CHKSUM=$3
PARAM=$4

OUTPUT_FN=$UUID_STR.token
TMP_FN=/tmp/$UUID_STR.tmp

awk -v UUID=$UUID_STR 'BEGIN{ printf UUID;  while (i++ < 92) printf " " }' > $TMP_FN

if [ ! $? -eq 0 ]; then
    echo "Could not create temporary file"
    exit -1
fi

if [ $METHOD == "pkcs11" ]; then
    openssl dgst -engine pkcs11 -keyform engine -sign $PARAM $CHKSUM -out $OUTPUT_FN $TMP_FN
elif [ $METHOD == "file" ]; then
    openssl dgst -sign $PARAM $CHKSUM -out $OUTPUT_FN $TMP_FN
else
    echo "Unknown method"
    exit -1
fi
