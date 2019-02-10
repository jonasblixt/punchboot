#!/bin/sh

#-----------------------------------------------------------------------------
#
# File: ahab_pki_tree.sh
#
# Description: This script generates a basic AHAB PKI tree for the NXP
#              AHAB code signing feature.  What the PKI tree looks like depends
#              on whether the SRK is chosen to be a CA key or not.  If the
#              SRKs are chosen to be CA keys then this script will generate the
#              following PKI tree:
#
#                                      CA Key
#                                      | | |
#                             -------- + | +---------------
#                            /           |                 \
#                         SRK1          SRK2       ...      SRKN
#                          |             |                   |
#                          |             |                   |
#                         SGK1          SGK2                SGKN
#
#              where: N can be 1 to 4.
#
#              Additional keys can be added to the tree separately.  In this
#              configuration SRKs may only be used to sign/verify other
#              keys/certificates
#
#              If the SRKs are chosen to be non-CA keys then this script will
#              generate the following PKI tree:
#
#                                      CA Key
#                                      | | |
#                             -------- + | +---------------
#                            /           |                 \
#                         SRK1          SRK2       ...      SRKN
#
#              In this configuration SRKs may only be used to sign code/data
#              and not other keys.  Note that not all NXP processors
#              including AHAB support this option.
#
#        Copyright 2018 NXP
#
# Presence of a copyright notice is not an acknowledgement of publication.
# This software file listing contains information of NXP that is of a
# confidential and proprietary nature and any viewing or use of this file is
# prohibited without specific written permission from NXP
#
#-----------------------------------------------------------------------------

printf "\n"
printf "    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
printf "    This script is a part of the Code signing tools for NXP's\n"
printf "    Advanced High Assurance Boot.  It generates a basic PKI tree. The\n"
printf "    PKI tree consists of one or more Super Root Keys (SRK), with each\n"
printf "    SRK having one subordinate keys: \n"
printf "        + a Signing key (SGK) \n"
printf "    Additional keys can be added to the PKI tree but a separate \n"
printf "    script is available for this.  This this script assumes openssl\n"
printf "    is installed on your system and is included in your search \n"
printf "    path.  Finally, the private keys generated are password \n"
printf "    protectedwith the password provided by the file key_pass.txt.\n"
printf "    The format of the file is the password repeated twice:\n"
printf "        my_password\n"
printf "        my_password\n"
printf "    All private keys in the PKI tree are in PKCS #8 format will be\n"
printf "    protected by the same password.\n\n"
printf "    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"

stty erase 

printf "Do you want to use an existing CA key (y/n)?:  \b"
read existing_ca
if [ $existing_ca = "y" ]
then
    printf "Enter CA key name:  \b"
    read ca_key
    printf "Enter CA certificate name:  \b"
    read ca_cert
fi

printf "Do you want to use Elliptic Curve Cryptography (y/n)?:  \b"
read use_ecc
if [ $use_ecc = "y" ]
then
    printf "Enter length for elliptic curve to be used for PKI tree:\n"
    printf "Possible values p256, p384, p521:   \b"
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
else
    printf "Enter key length in bits for PKI tree:  \b"
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
fi

printf "Enter the digest algorithm to use:  \b"
read da

# Confirm that a valid digest algorithm has been entered
case $da in
    sha256) ;;
    sha384) ;;
    sha512) ;;
    *)
        echo Invalid digest algorithm. Supported digest algorithms: sha256, sha384, sha512
    exit 1 ;;
esac

printf "Enter PKI tree duration (years):  \b"
read duration

# Compute validity period
val_period=$((duration*365))

# printf "How many Super Root Keys should be generated?  \b"
# read num_srk
num_srk=4

# Check that 0 < num_srk <= 4 (Max. number of SRKs)
if [ $num_srk -lt 1 ] || [ $num_srk -gt 4 ]
then
    echo The number of SRKs generated must be between 1 and 4
    exit 1
fi

# Check if SRKs should be generated as CA certs or user certs
printf "Do you want the SRK certificates to have the CA flag set? (y/n)?:  \b"
read srk_ca

# Check that the file "serial" is present, if not create it:
if [ ! -f serial ]
then
    echo "12345678" > serial
    echo "A default 'serial' file was created!"
fi

# Check that the file "key_pass.txt" is present, if not create it with default user/pwd:
if [ ! -f key_pass.txt ]
then
    echo "test" > key_pass.txt
    echo "test" >> key_pass.txt
    echo "A default file 'key_pass.txt' was created with password = test!"
fi

# The following is required otherwise OpenSSL complains
touch index.txt
echo "unique_subject = no" > index.txt.attr


if [ $existing_ca = "n" ]
then
    # Generate CA key and certificate
    # -------------------------------
    echo
    echo +++++++++++++++++++++++++++++++++++++
    echo + Generating CA key and certificate +
    echo +++++++++++++++++++++++++++++++++++++
    echo

    if [ $use_ecc = 'n' ]
    then
        ca_key=./CA1_${da}_${kl}_65537_v3_ca_key
        ca_cert=../crts/CA1_${da}_${kl}_65537_v3_ca_crt
        ca_subj_req=/CN=CA1_${da}_${kl}_65537_v3_ca/
        ca_key_type=rsa:${kl}
    else

        # Generate Elliptic Curve parameters:
        eck='ec-'$cn'.pem'
        openssl ecparam -out $eck -name $cn

        ca_key=./CA1_${da}_${cn}_v3_ca_key
        ca_cert=../crts/CA1_${da}_${cn}_v3_ca_crt
        ca_subj_req=/CN=CA1_${da}_${cn}_v3_ca/
        ca_key_type=ec:${eck}
    fi

    openssl req -newkey ${ca_key_type} -passout file:./key_pass.txt \
                   -${da} \
                   -subj ${ca_subj_req} \
                   -x509 -extensions v3_ca \
                   -keyout temp_ca.pem \
                   -out ${ca_cert}.pem \
                   -days ${val_period} -config ../ca/openssl.cnf

    # Generate CA key in PKCS #8 format - both PEM and DER
    openssl pkcs8 -passin file:./key_pass.txt -passout file:./key_pass.txt \
                  -topk8 -inform PEM -outform DER -v2 des3 \
                  -in temp_ca.pem \
                  -out ${ca_key}.der

    openssl pkcs8 -passin file:./key_pass.txt -passout file:./key_pass.txt \
                  -topk8 -inform PEM -outform PEM -v2 des3 \
                  -in temp_ca.pem \
                  -out ${ca_key}.pem

    # Convert CA Certificate to DER format
    openssl x509 -inform PEM -outform DER -in ${ca_cert}.pem -out ${ca_cert}.der

    # Cleanup
    \rm temp_ca.pem
fi


if [ $srk_ca != "y" ]
then
    # Generate SRK keys and certificates (non-CA)
    #     SRKs suitable for signing code/data
    # -------------------------------------------
    i=1;  # SRK Loop index
    while [ $i -le $num_srk ]
    do
        echo
        echo ++++++++++++++++++++++++++++++++++++++++
        echo + Generating SRK key and certificate $i +
        echo ++++++++++++++++++++++++++++++++++++++++
        echo
        if [ $use_ecc = 'n' ]
        then
            # Generate SRK key
            openssl genrsa -des3 -passout file:./key_pass.txt -f4 \
                           -out ./temp_srk.pem ${kl}

            srk_subj_req=/CN=SRK${i}_${da}_${kl}_65537_v3_usr/
            srk_crt=../crts/SRK${i}_${da}_${kl}_65537_v3_usr_crt
            srk_key=./SRK${i}_${da}_${kl}_65537_v3_usr_key
        else
            # Generate Elliptic Curve parameters:
            openssl ecparam -out ./temp_srk.pem -name ${cn} -genkey
            # Generate SRK key
            openssl ec -in ./temp_srk.pem -des3 -passout file:./key_pass.txt \
                       -out ./temp_srk.pem

            srk_subj_req=/CN=SRK${i}_${da}_${cn}_v3_usr/
            srk_crt=../crts/SRK${i}_${da}_${cn}_v3_usr_crt
            srk_key=./SRK${i}_${da}_${cn}_v3_usr_key
        fi

        # Generate SRK certificate signing request
        openssl req -new -batch -passin file:./key_pass.txt \
                    -subj ${srk_subj_req} \
                    -key ./temp_srk.pem \
                    -out ./temp_srk_req.pem

        # Generate SRK certificate (this is a CA cert)
           openssl ca -batch -passin file:./key_pass.txt \
                      -md ${da} -outdir ./ \
                      -in ./temp_srk_req.pem \
                      -cert ${ca_cert}.pem \
                   -keyfile ${ca_key}.pem \
                      -extfile ../ca/v3_usr.cnf \
                      -out ${srk_crt}.pem \
                      -days ${val_period} \
                      -config ../ca/openssl.cnf

        # Convert SRK Certificate to DER format
        openssl x509 -inform PEM -outform DER \
                     -in ${srk_crt}.pem \
                     -out ${srk_crt}.der

        # Generate SRK key in PKCS #8 format - both PEM and DER
        openssl pkcs8 -passin file:./key_pass.txt \
                      -passout file:./key_pass.txt \
                      -topk8 -inform PEM -outform DER -v2 des3 \
                      -in temp_srk.pem \
                      -out ${srk_key}.der

        openssl pkcs8 -passin file:./key_pass.txt \
                      -passout file:./key_pass.txt \
                      -topk8 -inform PEM -outform PEM -v2 des3 \
                      -in temp_srk.pem \
                      -out ${srk_key}.pem

        # Cleanup
        \rm ./temp_srk.pem ./temp_srk_req.pem
        i=$((i+1))
    done
else

# Generate AHAB  keys and certificates
# -----------------------------------
i=1;  # SRK Loop index
while [ $i -le $num_srk ]
do

    echo
    echo ++++++++++++++++++++++++++++++++++++++++
    echo + Generating SRK key and certificate $i +
    echo ++++++++++++++++++++++++++++++++++++++++
    echo

    if [ $use_ecc = 'n' ]
        then
            # Generate SRK key
            openssl genrsa -des3 -passout file:./key_pass.txt -f4 \
                           -out ./temp_srk.pem ${kl}

            srk_subj_req=/CN=SRK${i}_${da}_${kl}_65537_v3_ca/
            srk_crt=../crts/SRK${i}_${da}_${kl}_65537_v3_ca_crt
            srk_key=./SRK${i}_${da}_${kl}_65537_v3_ca_key
        else
            # Generate Elliptic Curve parameters:
            openssl ecparam -out ./temp_srk.pem -name ${cn} -genkey
            # Generate SRK key
            openssl ec -in ./temp_srk.pem -des3 -passout file:./key_pass.txt \
                       -out ./temp_srk.pem

            srk_subj_req=/CN=SRK${i}_${da}_${cn}_v3_ca/
            srk_crt=../crts/SRK${i}_${da}_${cn}_v3_ca_crt
            srk_key=./SRK${i}_${da}_${cn}_v3_ca_key
    fi
    # Generate SRK certificate signing request
       openssl req -new -batch -passin file:./key_pass.txt \
                   -subj ${srk_subj_req} \
                   -key ./temp_srk.pem \
                   -out ./temp_srk_req.pem

    # Generate SRK certificate (this is a CA cert)
       openssl ca -batch -passin file:./key_pass.txt \
                  -md ${da} -outdir ./ \
                  -in ./temp_srk_req.pem \
                  -cert ${ca_cert}.pem \
                  -keyfile ${ca_key}.pem \
                  -extfile ../ca/v3_ca.cnf \
                  -out ${srk_crt}.pem \
                  -days ${val_period} \
                  -config ../ca/openssl.cnf

    # Convert SRK Certificate to DER format
    openssl x509 -inform PEM -outform DER \
                 -in ${srk_crt}.pem \
                 -out ${srk_crt}.der

    # Generate SRK key in PKCS #8 format - both PEM and DER
    openssl pkcs8 -passin file:./key_pass.txt \
                  -passout file:./key_pass.txt \
                  -topk8 -inform PEM -outform DER -v2 des3 \
                  -in temp_srk.pem \
                  -out ${srk_key}.der

    openssl pkcs8 -passin file:./key_pass.txt \
                  -passout file:./key_pass.txt \
                  -topk8 -inform PEM -outform PEM -v2 des3 \
                  -in temp_srk.pem \
                  -out ${srk_key}.pem

    # Cleanup
    \rm ./temp_srk.pem ./temp_srk_req.pem

    echo
    echo ++++++++++++++++++++++++++++++++++++++++
    echo + Generating SGK key and certificate $i +
    echo ++++++++++++++++++++++++++++++++++++++++
    echo

    if [ $use_ecc = 'n' ]
        then
            srk_crt_i=../crts/SRK${i}_${da}_${kl}_65537_v3_ca_crt.pem
            srk_key_i=./SRK${i}_${da}_${kl}_65537_v3_ca_key.pem
            # Generate key
            openssl genrsa -des3 -passout file:./key_pass.txt -f4 \
                           -out ./temp_sgk.pem ${kl}

            sgk_subj_req=/CN=SGK${i}_1_${da}_${kl}_65537_v3_usr/
            sgk_crt=../crts/SGK${i}_1_${da}_${kl}_65537_v3_usr_crt
            sgk_key=./SGK${i}_1_${da}_${kl}_65537_v3_usr_key
        else
            srk_crt_i=../crts/SRK${i}_${da}_${cn}_v3_ca_crt.pem
            srk_key_i=./SRK${i}_${da}_${cn}_v3_ca_key.pem
            # Generate Elliptic Curve parameters:
            openssl ecparam -out ./temp_sgk.pem -name ${cn} -genkey
            # Generate key
            openssl ec -in ./temp_sgk.pem -des3 -passout file:./key_pass.txt \
                       -out ./temp_sgk.pem

            sgk_subj_req=/CN=SGK${i}_1_${da}_${cn}_v3_usr/
            sgk_crt=../crts/SGK${i}_1_${da}_${cn}_v3_usr_crt
            sgk_key=./SGK${i}_1_${da}_${cn}_v3_usr_key
    fi

    # Generate SGK certificate signing request
    openssl req -new -batch -passin file:./key_pass.txt \
                -subj ${sgk_subj_req} \
                -key ./temp_sgk.pem \
                -out ./temp_sgk_req.pem

    # Generate SGK certificate (this is a user cert)
    openssl ca -batch -md ${da} -outdir ./ \
               -passin file:./key_pass.txt \
               -in ./temp_sgk_req.pem \
               -cert ${srk_crt_i} \
               -keyfile ${srk_key_i} \
               -extfile ../ca/v3_usr.cnf \
               -out ${sgk_crt}.pem \
               -days ${val_period} \
               -config ../ca/openssl.cnf

    # Convert SGK Certificate to DER format
    openssl x509 -inform PEM -outform DER \
                 -in ${sgk_crt}.pem \
                 -out ${sgk_crt}.der

    # Generate SGK key in PKCS #8 format - both PEM and DER
    openssl pkcs8 -passin file:./key_pass.txt -passout file:./key_pass.txt \
                  -topk8 -inform PEM -outform DER -v2 des3 \
                  -in temp_sgk.pem \
                  -out ${sgk_key}.der

    openssl pkcs8 -passin file:./key_pass.txt -passout file:./key_pass.txt \
                  -topk8 -inform PEM -outform PEM -v2 des3 \
                  -in temp_sgk.pem \
                  -out ${sgk_key}.pem

    # Cleanup
    \rm ./temp_sgk.pem ./temp_sgk_req.pem

    i=$((i+1))
done
fi
exit 0
