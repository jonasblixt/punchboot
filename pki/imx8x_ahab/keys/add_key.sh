#!/bin/sh

#-----------------------------------------------------------------------------
#
# File: add_key.sh
#
# Description: This script adds a key to an existing HAB PKI tree to be used
#              with the HAB code signing feature.  Both the private key and
#              corresponding certificate are generated.  This script can
#              generate new SRKs, CSF keys and Images keys for HAB3 or HAB4.
#              This script is not intended for generating CA root keys.
#
#            (c) Freescale Semiconductor, Inc. 2011. All rights reserved.
#            Copyright 2018 NXP
#
# Presence of a copyright notice is not an acknowledgement of publication.
# This software file listing contains information of NXP that is of a
# confidential and proprietary nature and any viewing or use of this file is
# prohibited without specific written permission from NXP
#
#-----------------------------------------------------------------------------

stty erase 

if [ $(uname -m) = "x86_64" ]
then
    bin_path="linux64/bin"
else
    bin_path="linux32/bin"
fi

printf "Which version of HAB/AHAB do you want to generate the key for (3 = HAB3 / 4 = HAB4 / a = AHAB)?:  \b"
read ver
case $ver in
    3) ;;
    4) ;;
    a) ;;
    *)
        echo Error - HAB/AHAB version selected must be either 3, 4 or a
   exit 1
esac

printf "Enter new key name (e.g. SRK5):  \b"
read key_name

if [ $ver = "4" ] || [ $ver = "a" ]
then
    printf "Enter new key type (ecc / rsa):  \b"
    read kt

    # Confirm that a valid key type has been entered
    case $kt in
        rsa) ;;
        ecc) ;;
        *)
            echo Invalid key type. Supported key types: rsa, ecc
        exit 1 ;;
    esac
# HAB3
else
    kt="rsa"
fi

if [ $kt = "rsa" ]
then
    printf "Enter new key length in bits:  \b"
    read kl

    # Confirm that a valid key length has been entered
    case $kl in
        2048) ;;
        3072) ;;
        4096) ;;
        *)
            echo Invalid key length. Supported key lengths: 2048, 3072, 4096
        exit 1 ;;
    esac
else
    printf "Enter new key length (p256 / p384 / p521):  \b"
    read kl

    # Confirm that a valid key length has been entered
    case $kl in
        p256)
            cn="prime256v1" ;;
        p384)
            cn="secp384r1" ;;
        p521)
            cn="secp521r1" ;;
        *)
            echo Invalid key length. Supported key lengths: 256, 384, 521
        exit 1 ;;
    esac
fi

if [ $ver = "a" ]
then
    printf "Enter new message digest (sha256, sha384, sha512):  \b"
    read md

    # Confirm that a valid message digest has been entered
    case $md in
        sha256) ;;
        sha384) ;;
        sha512) ;;
        *)
            echo Invalid message digest. Supported message digests: sha256, sha384, sha512
        exit 1 ;;
    esac
# HAB3 or HAB4
else
    md="sha256"
fi

printf "Enter certificate duration (years):  \b"
read duration

# Compute validity period
val_period=$((duration*365))

# ---------------- Add SRK Key and Certificate -------------------
printf "Is this an SRK key?:  \b"
read srk

if [ $srk = "y" ]
then
    # Check if SRKs should be generated as CA certs or user certs
    if [ $ver = "4" ] || [ $ver = "a" ]
    then
        printf "Do you want the SRK to have the CA flag set (y/n)?:  \b"
        read srk_ca
        if [ $srk_ca = "y" ]
        then
            ca="ca"
        else
            ca="usr"
        fi
    # HAB3
    else
        ca="ca"
    fi

    printf "Enter CA signing key name:  \b"
    read signing_key
    printf "Enter CA signing certificate name:  \b"
    read signing_crt
# Not SRK
else
    if [ $ver != "a" ]
    then
        # ---------------- Add CSF Key and Certificate -------------------
        printf "Is this a CSF key?:  \b"
        read csf

        if [ $csf = "y" ]
        then
            printf "Enter SRK signing key name:  \b"
            read signing_key
            printf "Enter SRK signing certificate name:  \b"
            read signing_crt

            if [ $ver = "4" ]
            then
                ca="usr"
            # HAB3
            else
                ca="ca"
            fi
        # IMG
        else
            # ---------------- Add IMG Key and Certificate -------------------
            ca="usr"
            if [ $ver = "4" ]
            then
                printf "Enter SRK signing key name:  \b"
                read signing_key
                printf "Enter SRK signing certificate name:  \b"
                read signing_crt
            else
                printf "Enter CSF signing key name:  \b"
                read signing_key
                printf "Enter CSF signing certificate name:  \b"
                read signing_crt
            fi
        fi
    # AHAB
    else
        ca="usr"
        # ---------------- Add SGK key and certificate -------------------
        printf "Enter SRK signing key name:  \b"
        read signing_key
        printf "Enter SRK signing certificate name:  \b"
        read signing_crt
    fi
fi

# Generate outputs
if [ $kt = "ecc" ]
then
    key_fullname=${key_name}_${md}_${cn}_v3_${ca}

    # Generate Elliptic Curve parameters:
    openssl ecparam -out ./${key_fullname}_key.pem -name ${cn} -genkey
    # Generate ECC key
    openssl ec -in ./${key_fullname}_key.pem -des3 -passout file:./key_pass.txt \
        -out ./${key_fullname}_key.pem
else
    key_fullname=${key_name}_${md}_${kl}_65537_v3_${ca}
    wtls_name=${key_name}_${md}_${kl}_65537_wtls.crt

    # Generate RSA key
    openssl genrsa -des3 -passout file:./key_pass.txt -f4 \
        -out ./${key_fullname}_key.pem ${kl}
fi

# Generate certificate signing request
openssl req -new -batch -passin file:./key_pass.txt \
    -subj /CN=${key_fullname}/ \
    -key ./${key_fullname}_key.pem \
    -out ./${key_fullname}_req.pem

# Generate certificate
openssl ca -batch -passin file:./key_pass.txt \
    -md ${md} -outdir ./ \
    -in ./${key_fullname}_req.pem \
    -cert ${signing_crt} \
    -keyfile ${signing_key} \
    -extfile ../ca/v3_${ca}.cnf \
    -out ../crts/${key_fullname}_crt.pem \
    -days ${val_period} \
    -config ../ca/openssl.cnf

# Convert certificate to DER format
openssl x509 -inform PEM -outform DER \
    -in ../crts/${key_fullname}_crt.pem \
    -out ../crts/${key_fullname}_crt.der

# Generate key in PKCS #8 format - both PEM and DER
openssl pkcs8 -passin file:./key_pass.txt \
    -passout file:./key_pass.txt \
    -topk8 -inform PEM -outform DER -v2 des3 \
    -in ${key_fullname}_key.pem \
    -out ${key_fullname}_key.der

openssl pkcs8 -passin file:./key_pass.txt \
    -passout file:./key_pass.txt \
    -topk8 -inform PEM -outform PEM -v2 des3 \
    -in ${key_fullname}_key.pem \
    -out ${key_fullname}_key_tmp.pem

mv ${key_fullname}_key_tmp.pem ${key_fullname}_key.pem

if [ $ver = "3" ]
then
    # Generate WTLS certificate, ...
    ../${bin_path}/x5092wtls \
        -c ../crts/${key_fullname}_crt.pem \
        -k ./${signing_key} \
        -w ../crts/${wtls_name} \
        -p ./key_pass.txt

    if [ $srk == "y" ]
    then
        # C file and add corresponding fuse information to srktool_wtls.txt
        ../${bin_path}/srktool \
            -h 3 \
            -c ../crts/${wtls_name} \
            -o >> ../crts/srk_wtls_cert_efuse_info.txt
    fi
fi

# Clean up
\rm -f *_req.pem
exit 0
