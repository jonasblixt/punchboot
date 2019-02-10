@echo off

::-----------------------------------------------------------------------------
::
:: File: add_key.bat
::
:: Description: This script adds a key to an existing HAB PKI tree to be used
::              with the HAB code signing feature.  Both the private key and
::              corresponding certificate are generated.  This script can
::              generate new SRKs, CSF keys and Images keys for HAB3 or HAB4.
::              This script is not intended for generating CA root keys.
::
::            (c) Freescale Semiconductor, Inc. 2011. All rights reserved.
::            Copyright 2018 NXP
::
:: Presence of a copyright notice is not an acknowledgement of publication.
:: This software file listing contains information of NXP that is of a
:: confidential and proprietary nature and any viewing or use of this file is
:: prohibited without specific written permission from NXP
::
::-----------------------------------------------------------------------------

set /P ver="Which version of HAB do you want to generate the key for (3 = HAB3 / 4 = HAB4 / a = AHAB)?: "
if %ver%==3 goto VALID_ver
if %ver%==4 goto VALID_ver
if %ver%==a goto VALID_ver
echo Error - HAB version selected must be either 3, 4 or a
exit /B
:VALID_ver

set /P key_name="Enter new key name (e.g. SRK5): "

:: Key type
if %ver%==3 goto KEY_TYPE_DEFAULT
:: AHAB or HAB4
set /P kt="Enter new key type (ecc / rsa): "
:: Confirm that a valid key type has been entered
if %kt%==ecc goto KEY_TYPE_DONE
if %kt%==rsa goto KEY_TYPE_DONE
echo Invalid key type. Supported key types: rsa, ecc
exit /B
:: HAB3
:KEY_TYPE_DEFAULT
set kt=rsa
:KEY_TYPE_DONE

:: Key length
if not %kt%==rsa goto KEY_LENGTH_ECC
:: RSA
set /P kl="Enter new key length in bits: "
:: Confirm that a valid key length has been entered
if %kl%==2048 goto VALID_KEY_LENGTH
if %kl%==3072 goto VALID_KEY_LENGTH
if %kl%==4096 goto VALID_KEY_LENGTH
echo Invalid key length. Supported key lengths: 2048, 3072, 4096
exit /B
:KEY_LENGTH_ECC
:: ECC
set /P kl="Enter new key length (p256 / p384 / p521): "
:: Confirm that a valid key length has been entered
if %kl%==p256 set "cn=prime256v1" & goto VALID_KEY_LENGTH
if %kl%==p384 set "cn=secp384r1"  & goto VALID_KEY_LENGTH
if %kl%==p521 set "cn=secp521r1"  & goto VALID_KEY_LENGTH
echo Invalid key length. Supported key lengths: 256, 384, 521
exit /B
:VALID_KEY_LENGTH

:: Message digest
if not %ver%==a goto MD_DEFAULT
:: AHAB
set /P md="Enter new message digest (sha256, sha384, sha512): "
:: Confirm that a valid message digest has been entered
if %md%==sha256 goto MD_DONE
if %md%==sha384 goto MD_DONE
if %md%==sha512 goto MD_DONE
echo Invalid message digest. Supported message digests: sha256, sha384, sha512
exit /B
:: HAB3 or HAB4
:MD_DEFAULT
set md=sha256
:MD_DONE

set /P duration="Enter certificate duration (years): "

:: Compute validity period
set /A val_period=%duration%*365

:: ---------------- Add SRK key and certificate -------------------
set /P srk="Is this an SRK key? "
if %srk%==n goto GEN_CSF_IMG_SGK

:: Check if SRKs should be generated as CA certs or user certs
if %ver%==3 goto CA_DEFAULT
set /P srk_ca="Do you want the SRK to have the CA flag set (y/n)?: "
if %srk_ca%==y goto CA_DEFAULT
set ca=usr
goto CA_DONE
:CA_DEFAULT
set ca=ca
:CA_DONE

set /P signing_key="Enter CA signing key name: "
set /P signing_crt="Enter CA signing certificate name: "
goto GEN_OUTPUTS

:GEN_CSF_IMG_SGK
if %ver%==a goto GEN_SGK

:: ---------------- Add CSF key and certificate -------------------
set /P csf="Is this a CSF key?: "
if %csf%==n goto GEN_IMG
set /P signing_key="Enter SRK signing key name: "
set /P signing_crt="Enter SRK signing certificate name: "
if %ver%==4 set ca=usr
if %ver%==3 set ca=ca
goto GEN_OUTPUTS

:GEN_IMG
:: ---------------- Add IMG key and certificate -------------------
set ca=usr
if %ver%==3 goto GEN_IMG_HAB3
set /P signing_key="Enter SRK signing key name: "
set /P signing_crt="Enter SRK signing certificate name: "
goto GEN_OUTPUTS
:GEN_IMG_HAB3
set /P signing_key="Enter CSF signing key name: "
set /P signing_crt="Enter CSF signing certificate name: "
goto GEN_OUTPUTS

:GEN_SGK
:: ---------------- Add SGK key and certificate -------------------
set ca="usr"
set /P signing_key="Enter SRK signing key name: "
set /P signing_crt="Enter SRK signing certificate name: "
goto GEN_OUTPUTS

:GEN_OUTPUTS
:: ---------------- Generate outputs ------------------------------

:: Generate key
if %kt%==rsa goto GEN_KEY_RSA
set key_fullname=%key_name%_%md%_%cn%_v3_%ca%
:: Generate Elliptic Curve parameters
openssl ecparam -out .\%key_fullname%_key.pem -name %cn% -genkey
:: Generate ECC key
openssl ec -in .\%key_fullname%_key.pem -des3 -passout file:.\key_pass.txt ^
    -out .\%key_fullname%_key.pem
goto GEN_KEY_DONE
:GEN_KEY_RSA
set key_fullname=%key_name%_%md%_%kl%_65537_v3_%ca%
set wtls_name=%key_name%_%md%_%kl%_65537_wtls.crt
:: Generate RSA key
openssl genrsa -des3 -passout file:.\key_pass.txt -f4 ^
    -out .\%key_fullname%_key.pem %kl%
:GEN_KEY_DONE

:: Generate certificate signing request
openssl req -new -batch -passin file:.\key_pass.txt ^
    -subj /CN=%key_fullname%/ ^
    -key .\%key_fullname%_key.pem ^
    -out .\%key_fullname%_req.pem

:: Generate certificate
openssl ca -batch -passin file:.\key_pass.txt ^
    -md %md% -outdir . ^
    -in .\%key_fullname%_req.pem ^
    -cert %signing_crt% ^
    -keyfile %signing_key% ^
    -extfile ..\ca\v3_%ca%.cnf ^
    -out ..\crts\%key_fullname%_crt.pem ^
    -days %val_period% ^
    -config ..\ca\openssl.cnf

:: Convert certificate to DER format
openssl x509 -inform PEM -outform DER ^
    -in ..\crts\%key_fullname%_crt.pem ^
    -out ..\crts\%key_fullname%_crt.der

:: Generate key in PKCS #8 format - both PEM and DER
openssl pkcs8 -passin file:.\key_pass.txt ^
    -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform DER -v2 des3 ^
    -in %key_fullname%_key.pem ^
    -out %key_fullname%_key.der

openssl pkcs8 -passin file:.\key_pass.txt ^
    -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform PEM -v2 des3 ^
    -in %key_fullname%_key.pem ^
    -out %key_fullname%_key_tmp.pem

del /F %key_fullname%_key.pem
ren %key_fullname%_key_tmp.pem %key_fullname%_key.pem

if not %ver%==3 goto CLEAN
:: Generate WTLS certificate, ...
..\mingw32\bin\x5092wtls ^
    -c ..\crts\%key_fullname%_crt.pem ^
    -k .\%signing_key% ^
    -w ..\crts\%wtls_name% ^
    -p .\key_pass.txt

if not %srk%==y goto CLEAN
:: C file and add corresponding fuse information to srktool_wtls.txt
..\mingw32\bin\srktool ^
    -h 3 ^
    -c ..\crts\%wtls_name% ^
    -o >> ..\crts\srk_wtls_cert_efuse_info.txt

:CLEAN
:: Clean up
del /F *_req.pem
exit /B
