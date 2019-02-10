@echo off

::-----------------------------------------------------------------------------
::
:: File: ahab_pki_tree.bat
::
:: Description: This script generates a basic AHAB PKI tree for the NXP
::              AHAB code signing feature.  What the PKI tree looks like depends
::              on whether the SRK is chosen to be a CA key or not. If the
::              SRKs are chosen to be CA keys then this script will generate the
::              following PKI tree:
::
::                                      CA Key
::                                      | | |
::                             -------- + | +---------------
::                            /           |                 ^
::                         SRK1          SRK2       ...      SRKN
::                          |             |                   |
::                          |             |                   |
::                         SGK1          SGK2                SGKN
::
::              where: N can be 1 to 4.
::
::              Additional keys can be added to the tree separately.  In this
::              configuration SRKs may only be used to sign/verify other
::              keys/certificates
::
::              If the SRKs are chosen to be non-CA keys then this script will
::              generate the following PKI tree:
::
::                                      CA Key
::                                      | | |
::                             -------- + | +---------------
::                            /           |                 ^
::                         SRK1          SRK2       ...      SRKN
::
::              In this configuration SRKs may only be used to sign code/data
::              and not other keys.  Note that not all NXP processors
::              including AHAB support this option.
::
::        Copyright 2018 NXP
::
:: Presence of a copyright notice is not an acknowledgement of publication.
:: This software file listing contains information of NXP that is of a
:: confidential and proprietary nature and any viewing or use of this file is
:: prohibited without specific written permission from NXP
::
::-----------------------------------------------------------------------------

echo.
echo    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo    This script is a part of the Code signing tools for NXP's
echo    Advanced High Assurance Boot.  It generates a basic PKI tree. The
echo    PKI tree consists of one or more Super Root Keys (SRK), with each
echo    SRK having one subordinate keys: 
echo        + a Signing key (SGK) 
echo    Additional keys can be added to the PKI tree but a separate 
echo    script is available for this.  This this script assumes openssl
echo    is installed on your system and is included in your search 
echo    path.  Finally, the private keys generated are password 
echo    protectedwith the password provided by the file key_pass.txt.
echo    The format of the file is the password repeated twice:
echo        my_password
echo        my_password
echo    All private keys in the PKI tree are in PKCS #8 format will be
echo    protected by the same password.\n
echo    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo.

:CA_KEY
set /P existing_ca="Do you want to use an existing CA key (y/n)?: "
if %existing_ca%==n goto KEY_TYPE
set /P ca_key="Enter CA key name: "
set /P ca_cert="Enter CA certificate name: "

:KEY_TYPE
set /P use_ecc="Do you want to use Elliptic Curve Cryptography (y/n)?: "
if %use_ecc%==n goto KEY_LENGTH_RSA

:KEY_LENGTH_ECC
set /P kl="Enter length for elliptic curve to be used for PKI tree (possible values p256, p384, p521): "
:: Confirm that a valid key length has been entered
if %kl%==p256 set "cn=prime256v1" & goto DIGEST_ALGO
if %kl%==p384 set "cn=secp384r1"  & goto DIGEST_ALGO
if %kl%==p521 set "cn=secp521r1"  & goto DIGEST_ALGO
echo Invalid key length. Supported key lengths: 256, 384, 521
exit /B

:KEY_LENGTH_RSA
set /P kl="Enter key length in bits for PKI tree: "
:: Confirm that a valid key length has been entered
if %kl%==2048 goto DIGEST_ALGO
if %kl%==3072 goto DIGEST_ALGO
if %kl%==4096 goto DIGEST_ALGO
echo Invalid key length. Supported key lengths: 2048, 3072, 4096
exit /B

:DIGEST_ALGO
set /P da="Enter the digest algorithm to use: "
:: Confirm that a valid digest algorithm has been entered
if %da%==sha256 goto KEY_DURATION
if %da%==sha384 goto KEY_DURATION
if %da%==sha512 goto KEY_DURATION
echo Invalid digest algorithm. Supported digest algorithms: sha256, sha384, sha512
exit /B

:KEY_DURATION
set /P duration="Enter PKI tree duration (years): "
:: Compute validity period
set /A val_period=%duration%*365

:NUMBER_OF_SRKS
:: set /P num_srk="How many Super Root Keys should be generated? "
set num_srk=4
set /A max=%num_srk%+1
:: Check that 0 < num_srk <= 4 (Max. number of SRKs)
if %num_srk%==1 goto CA_SRK
if %num_srk%==2 goto CA_SRK
if %num_srk%==3 goto CA_SRK
if %num_srk%==4 goto CA_SRK
echo The number of SRKs generated must be between 1 and 4
exit /B

:CA_SRK
:: Check if SRKs should be generated as CA certs or user certs
set /P srk_ca="Do you want the SRK certificates to have the CA flag set? (y/n)?: "

:SERIAL
:: Check that the file "serial" is present, if not create it:
if exist "serial" goto KEY_PASS
echo 12345678 > serial
echo A default 'serial' file was created!

:KEY_PASS
:: Check that the file "key_pass.txt" is present, if not create it with default user/pwd:
if exist "key_pass.txt" goto OPENSSL_INIT
echo test> key_pass.txt
echo test>> key_pass.txt
echo A default file 'key_pass.txt' was created with password = test!

:OPENSSL_INIT
:: Convert file in Unix format for OpenSSL 1.0.2
convlb key_pass.txt
:: The following is required otherwise OpenSSL complains
if exist index.txt      del /F index.txt
if exist index.txt.attr del /F index.txt.attr
type nul > index.txt
echo unique_subject = no > index.txt.attr
set OPENSSL_CONF=..\ca\openssl.cnf

:GEN_CA
if %existing_ca%==y goto GEN_SRK
:: Generate CA key and certificate
:: -------------------------------
echo.
echo +++++++++++++++++++++++++++++++++++++
echo + Generating CA key and certificate +
echo +++++++++++++++++++++++++++++++++++++
echo.

if %use_ecc%==n (
    set ca_key=.\CA1_%da%_%kl%_65537_v3_ca_key
    set ca_cert=..\crts\CA1_%da%_%kl%_65537_v3_ca_crt
    set ca_subj_req=/CN=CA1_%da%_%kl%_65537_v3_ca/
    set ca_key_type=rsa:%kl%
) else (
    set eck=ec-%cn%.pem
    openssl ecparam -out %eck% -name %cn%
    set ca_key=.\CA1_%da%_%cn%_v3_ca_key
    set ca_cert=..\crts\CA1_%da%_%cn%_v3_ca_crt
    set ca_subj_req=/CN=CA1_%da%_%cn%_v3_ca/
    set ca_key_type=ec:%eck%
)

openssl req -newkey %ca_key_type% -passout file:.\key_pass.txt ^
    -%da% ^
    -subj %ca_subj_req% ^
    -x509 -extensions v3_ca ^
    -keyout temp_ca.pem ^
    -out %ca_cert%.pem ^
    -days %val_period%

:: Generate CA key in PKCS #8 format - both PEM and DER
openssl pkcs8 -passin file:.\key_pass.txt -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform DER -v2 des3 ^
    -in temp_ca.pem ^
    -out %ca_key%.der

openssl pkcs8 -passin file:.\key_pass.txt -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform PEM -v2 des3 ^
    -in temp_ca.pem ^
    -out %ca_key%.pem

:: Convert CA Certificate to DER format
openssl x509 -inform PEM -outform DER -in %ca_cert%.pem -out %ca_cert%.der

:: Cleanup
del /F temp_ca.pem

:GEN_SRK
if %srk_ca%==y goto GEN_SRK_SGK
:: Generate SRK keys and certificates (non-CA)
:: SRKs suitable for signing code/data
:: -------------------------------------------

:: SRK Loop index
set /A i=1
:LOOP_SRK
echo.
echo ++++++++++++++++++++++++++++++++++++++++
echo + Generating SRK key and certificate %i% +
echo ++++++++++++++++++++++++++++++++++++++++
echo.
if %use_ecc%==n (
    :: Generate SRK key
    openssl genrsa -des3 -passout file:.\key_pass.txt -f4 ^
        -out .\temp_srk.pem %kl%

    set srk_subj_req=/CN=SRK%i%_%da%_%kl%_65537_v3_usr/
    set srk_crt=..\crts\SRK%i%_%da%_%kl%_65537_v3_usr_crt
    set srk_key=.\SRK%i%_%da%_%kl%_65537_v3_usr_key
) else (
    :: Generate Elliptic Curve parameters:
    openssl ecparam -out .\temp_srk.pem -name %cn% -genkey
    :: Generate SRK key
    openssl ec -in .\temp_srk.pem -des3 -passout file:.\key_pass.txt ^
        -out .\temp_srk.pem

    set srk_subj_req=/CN=SRK%i%_%da%_%cn%_v3_usr/
    set srk_crt=..\crts\SRK%i%_%da%_%cn%_v3_usr_crt
    set srk_key=.\SRK%i%_%da%_%cn%_v3_usr_key
)

:: Generate SRK certificate signing request
openssl req -new -batch -passin file:.\key_pass.txt ^
    -subj %srk_subj_req% ^
    -key .\temp_srk.pem ^
    -out .\temp_srk_req.pem

:: Generate SRK certificate (this is a CA cert)
openssl ca -batch -passin file:.\key_pass.txt ^
    -md %da% -outdir .\ ^
    -in .\temp_srk_req.pem ^
    -cert %ca_cert%.pem ^
    -keyfile %ca_key%.pem ^
    -extfile ..\ca\v3_usr.cnf ^
    -out %srk_crt%.pem ^
    -days %val_period%

:: Convert SRK Certificate to DER format
openssl x509 -inform PEM -outform DER ^
    -in %srk_crt%.pem ^
    -out %srk_crt%.der

:: Generate SRK key in PKCS #8 format - both PEM and DER
openssl pkcs8 -passin file:.\key_pass.txt ^
    -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform DER -v2 des3 ^
    -in temp_srk.pem ^
    -out %srk_key%.der

openssl pkcs8 -passin file:.\key_pass.txt ^
    -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform PEM -v2 des3 ^
    -in temp_srk.pem ^
    -out %srk_key%.pem

:: Cleanup
del /F .\temp_srk.pem .\temp_srk_req.pem

set /A i+=1
if %i%==%max% exit /B
goto LOOP_SRK

:GEN_SRK_SGK
:: Generate SRK keys and certficiates (CA)
:: Generate SGK keys and certificates (non-CA)
:: SGKs suitable for signing code/data
:: -------------------------------------------

:: SRK Loop index
set /A i=1
:LOOP_SRK_SGK
echo.
echo ++++++++++++++++++++++++++++++++++++++++
echo + Generating SRK key and certificate %i% +
echo ++++++++++++++++++++++++++++++++++++++++
echo.

if %use_ecc%==n (
    :: Generate SRK key
    openssl genrsa -des3 -passout file:.\key_pass.txt -f4 ^
        -out ./temp_srk.pem %kl%

    set srk_subj_req=/CN=SRK%i%_%da%_%kl%_65537_v3_ca/
    set srk_crt=..\crts\SRK%i%_%da%_%kl%_65537_v3_ca_crt
    set srk_key=.\SRK%i%_%da%_%kl%_65537_v3_ca_key
) else (
    :: Generate Elliptic Curve parameters:
    openssl ecparam -out .\temp_srk.pem -name %cn% -genkey
    :: Generate SRK key
    openssl ec -in .\temp_srk.pem -des3 -passout file:.\key_pass.txt ^
        -out .\temp_srk.pem

    set srk_subj_req=/CN=SRK%i%_%da%_%cn%_v3_ca/
    set srk_crt=..\crts\SRK%i%_%da%_%cn%_v3_ca_crt
    set srk_key=.\SRK%i%_%da%_%cn%_v3_ca_key
)
:: Generate SRK certificate signing request
openssl req -new -batch -passin file:.\key_pass.txt ^
    -subj %srk_subj_req% ^
    -key .\temp_srk.pem ^
    -out .\temp_srk_req.pem

:: Generate SRK certificate (this is a CA cert)
openssl ca -batch -passin file:.\key_pass.txt ^
    -md %da% -outdir .\ ^
    -in .\temp_srk_req.pem ^
    -cert %ca_cert%.pem ^
    -keyfile %ca_key%.pem ^
    -extfile ..\ca\v3_ca.cnf ^
    -out %srk_crt%.pem ^
    -days %val_period%

:: Convert SRK Certificate to DER format
openssl x509 -inform PEM -outform DER ^
    -in %srk_crt%.pem ^
    -out %srk_crt%.der

:: Generate SRK key in PKCS #8 format - both PEM and DER
openssl pkcs8 -passin file:.\key_pass.txt ^
    -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform DER -v2 des3 ^
    -in temp_srk.pem ^
    -out %srk_key%.der

openssl pkcs8 -passin file:.\key_pass.txt ^
    -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform PEM -v2 des3 ^
    -in temp_srk.pem ^
    -out %srk_key%.pem

:: Cleanup
del /F .\temp_srk.pem .\temp_srk_req.pem

echo.
echo ++++++++++++++++++++++++++++++++++++++++
echo + Generating SGK key and certificate %i% +
echo ++++++++++++++++++++++++++++++++++++++++
echo.

if %use_ecc%==n (
    set srk_crt_i=..\crts\SRK%i%_%da%_%kl%_65537_v3_ca_crt.pem
    set srk_key_i=.\SRK%i%_%da%_%kl%_65537_v3_ca_key.pem
    :: Generate key
    openssl genrsa -des3 -passout file:.\key_pass.txt -f4 ^
        -out ./temp_sgk.pem %kl%

    set sgk_subj_req=/CN=SGK%i%_1_%da%_%kl%_65537_v3_usr/
    set sgk_crt=..\crts\SGK%i%_1_%da%_%kl%_65537_v3_usr_crt
    set sgk_key=.\SGK%i%_1_%da%_%kl%_65537_v3_usr_key
) else (
    set srk_crt_i=..\crts\SRK%i%_%da%_%cn%_v3_ca_crt.pem
    set srk_key_i=.\SRK%i%_%da%_%cn%_v3_ca_key.pem
    :: Generate Elliptic Curve parameters:
    openssl ecparam -out .\temp_sgk.pem -name %cn% -genkey
    :: Generate key
    openssl ec -in .\temp_sgk.pem -des3 -passout file:.\key_pass.txt ^
        -out .\temp_sgk.pem

    set sgk_subj_req=/CN=SGK%i%_1_%da%_%cn%_v3_usr/
    set sgk_crt=..\crts\SGK%i%_1_%da%_%cn%_v3_usr_crt
    set sgk_key=.\SGK%i%_1_%da%_%cn%_v3_usr_key
)

:: Generate SGK certificate signing request
openssl req -new -batch -passin file:.\key_pass.txt ^
    -subj %sgk_subj_req% ^
    -key .\temp_sgk.pem ^
    -out .\temp_sgk_req.pem

:: Generate SGK certificate (this is a user cert)
openssl ca -batch -md %da% -outdir .\ ^
    -passin file:.\key_pass.txt ^
    -in .\temp_sgk_req.pem ^
    -cert %srk_crt_i% ^
    -keyfile %srk_key_i% ^
    -extfile ..\ca\v3_usr.cnf ^
    -out %sgk_crt%.pem ^
    -days %val_period%

:: Convert SGK Certificate to DER format
openssl x509 -inform PEM -outform DER ^
    -in %sgk_crt%.pem ^
    -out %sgk_crt%.der

:: Generate SGK key in PKCS #8 format - both PEM and DER
openssl pkcs8 -passin file:.\key_pass.txt -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform DER -v2 des3 ^
    -in temp_sgk.pem ^
    -out %sgk_key%.der

openssl pkcs8 -passin file:.\key_pass.txt -passout file:.\key_pass.txt ^
    -topk8 -inform PEM -outform PEM -v2 des3 ^
    -in temp_sgk.pem ^
    -out %sgk_key%.pem

:: Cleanup
del /F .\temp_sgk.pem .\temp_sgk_req.pem

set /A i+=1
if %i%==%max% exit /B
goto LOOP_SRK_SGK
