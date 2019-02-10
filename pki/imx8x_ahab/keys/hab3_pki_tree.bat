@echo off
::-----------------------------------------------------------------------------
::
:: File: hab3_pki_tree.bat
::
:: Description: This script generates a basic HAB3 PKI tree for the Freescale
::              HAB code signing feature. This script generates the
::              following PKI tree:
::
::                                      CA Key
::                                      | | |
::                             ---------+ | +---------
::                            /           |           \
::                         SRK1          SRK2   ...  SRKN
::                         /              |             \
::                        /               |              \
::                    CSF1_1            CSF2_1  ...     CSFN_1
::                      /                 |                \
::                     /                  |                 \
::                  IMG1_1              IMG2_1  ...        IMGN_1
::
::
::
::              where: N can be 1 to 4.
::
::              Additional keys can be added to the tree separately.
::
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

echo.
echo     +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo     This script is a part of the Code signing tools for Freescale's
echo     High Assurance Boot.  It generates a basic HAB3 PKI tree.
echo     The PKI tree consists of one or more Super Root Keys (SRK),
echo     with each SRK having a one subordinate Command Sequence File
echo     (CSF) key.  Each CSF key Image key then has one subordinate.
echo     Image key.  Additional keys can be added to the PKI tree but a
echo     separate script is available for this.  This this script\n"
echo     assumes openssl is installed on your system and is included in
echo     your search path.  Note that this script automatically generates
echo     the WTLS certificates required for HAB3.
echo     Finally, the private keys generated are password
echo     protected with the password provided by the file key_pass.txt.
echo     The format of the file is:
echo         my_password
echo     All private keys in the PKI tree will be protected by the same
echo     password.
echo     +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo.

set /P existing_ca=Do you want to use an existing CA key (y/n)?:

if not %existing_ca%=="y" goto KEY_LENGTH
set /P ca_key=Enter CA key name:
set /P ca_cert=Enter CA certificate name:

:KEY_LENGTH
set /P kl=Enter key length in bits for PKI tree:

:: Confirm that a valid key length has been entered
if %kl%==1024 goto VALID_KEY_LENGTH
if %kl%==2048 goto VALID_KEY_LENGTH
echo Invalid key length. Supported key lengths for HAB3: 1024, 2048
exit /B
:VALID_KEY_LENGTH

set /P duration=Enter PKI tree duration (years):

:: Compute validity period
set /A val_period=%duration%*365


set /P num_srk=How many Super Root Keys should be generated?

:: Check that 0 < num_srk < 4 (Max. number of SRKs)
if %num_srk%==1 goto VALID_NUM_KEYS
if %num_srk%==2 goto VALID_NUM_KEYS
if %num_srk%==3 goto VALID_NUM_KEYS
if %num_srk%==4 goto VALID_NUM_KEYS
echo The number of SRKs generated must be between 1 and 4
exit /B
:VALID_NUM_KEYS

:: Check that the file "serial" is present, if not create it:
if exist "serial" goto KEY_PASS
echo 12345678 > serial
echo A default 'serial' file was created!

:KEY_PASS
:: Check that the file "key_pass.txt" is present, if not create it with default user/pwd:
if exist "key_pass.txt" goto OPENSSL_INIT
echo test > key_pass.txt
echo test >> key_pass.txt
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

:: Generate CA key and certificate
:: -------------------------------
::if not %existing_ca%=="y" goto GEN_HAB_KEYS
set ca_key=.\CA1_sha256_%kl%_65537_v3_ca_key.pem
set ca_cert=..\crts\CA1_sha256_%kl%_65537_v3_ca_crt.pem

echo.
echo +++++++++++++++++++++++++++++++++++++
echo + Generating CA key and certificate +
echo +++++++++++++++++++++++++++++++++++++
echo.
:: Note: '^' is to continue the command on the next line
openssl req -newkey rsa:%kl% -passout file:.\key_pass.txt ^
-subj /CN=CA1_sha256_%kl%_65537_v3_ca/ ^
-x509 -extensions v3_ca ^
-keyout %ca_key% ^
-out %ca_cert% ^
-days %val_period%

:: Generate HAB Keys and certificates
:: ----------------------------------
:GEN_HAB_KEYS
set /a i=1
set /a max=num_srk+1

:GEN_LOOP
echo.
echo ++++++++++++++++++++++++++++++++++++++++
echo + Generating SRK key and certificate %i% +
echo ++++++++++++++++++++++++++++++++++++++++
echo.
:: Generate SRK key
openssl genrsa -des3 -passout file:.\key_pass.txt -f4 ^
-out ./SRK%i%_sha256_%kl%_65537_v3_ca_key.pem %kl%

:: Generate SRK certificate signing request
openssl req -new -batch -passin file:.\key_pass.txt ^
-subj /CN=SRK%i%_sha256_%kl%_65537_v3_ca/ ^
-key .\SRK%i%_sha256_%kl%_65537_v3_ca_key.pem ^
-out .\SRK%i%_sha256_%kl%_65537_v3_ca_req.pem

:: Generate SRK certificate (this is a CA cert)
openssl ca -batch -passin file:.\key_pass.txt ^
-md sha256 -outdir . ^
-in ./SRK%i%_sha256_%kl%_65537_v3_ca_req.pem ^
-cert %ca_cert% ^
-keyfile %ca_key% ^
-extfile ..\ca\v3_ca.cnf ^
-out ..\crts\SRK%i%_sha256_%kl%_65537_v3_ca_crt.pem ^
-days %val_period%

echo.
echo ++++++++++++++++++++++++++++++++++++++++
echo + Generating CSF key and certificate %i% +
echo ++++++++++++++++++++++++++++++++++++++++
echo.
:: Generate key
openssl genrsa -des3 -passout file:./key_pass.txt ^
-f4 -out .\CSF%i%_1_sha256_%kl%_65537_v3_ca_key.pem %kl%


:: Generate CSF certificate signing request
openssl req -new -batch -passin file:.\key_pass.txt ^
-subj /CN=CSF%i%_1_sha256_%kl%_65537_v3_ca/ ^
-key .\CSF%i%_1_sha256_%kl%_65537_v3_ca_key.pem ^
-out .\CSF%i%_1_sha256_%kl%_65537_v3_ca_req.pem

:: Generate CSF certificate (this is a CA cert)
openssl ca -batch -md sha256 -outdir . ^
-passin file:.\key_pass.txt ^
-in .\CSF%i%_1_sha256_%kl%_65537_v3_ca_req.pem ^
-cert ..\crts\SRK%i%_sha256_%kl%_65537_v3_ca_crt.pem ^
-keyfile .\SRK%i%_sha256_%kl%_65537_v3_ca_key.pem ^
-extfile ..\ca\v3_ca.cnf ^
-out ..\crts\CSF%i%_1_sha256_%kl%_65537_v3_ca_crt.pem ^
-days %val_period%

echo.
echo ++++++++++++++++++++++++++++++++++++++++
echo + Generating IMG key and certificate %i% +
echo ++++++++++++++++++++++++++++++++++++++++
echo.

:: Generate key
openssl genrsa -des3 -passout file:.\key_pass.txt ^
-f4 -out .\IMG%i%_1_sha256_%kl%_65537_v3_usr_key.pem %kl%

:: Generate IMG certificate signing request
openssl req -new -batch -passin file:./key_pass.txt ^
-subj /CN=IMG%i%_1_sha256_%kl%_65537_v3_usr/ ^
-key .\IMG%i%_1_sha256_%kl%_65537_v3_usr_key.pem ^
-out .\IMG%i%_1_sha256_%kl%_65537_v3_usr_req.pem

openssl ca -batch -md sha256 -outdir . ^
-passin file:.\key_pass.txt ^
-in .\IMG%i%_1_sha256_%kl%_65537_v3_usr_req.pem ^
-cert ..\crts\CSF%i%_1_sha256_%kl%_65537_v3_ca_crt.pem ^
-keyfile .\CSF%i%_1_sha256_%kl%_65537_v3_ca_key.pem ^
-extfile ..\ca\v3_usr.cnf ^
-out ..\crts\IMG%i%_1_sha256_%kl%_65537_v3_usr_crt.pem ^
-days %val_period%

set /a i += 1
if %i%==%max% goto DONE
goto GEN_LOOP
:DONE


:: Cleanup - remove certificate signing request files
del *_req.pem


:: Generate WTLS certificates
echo.
echo +++++++++++++++++++++++++++++++++++++
echo + Generating HAB3 WTLS certificates +
echo +++++++++++++++++++++++++++++++++++++
echo.

set /a i=1
set /a max=num_srk+1
set srk_list=

:GEN_WTLS
:: Convert SRK certs
..\mingw32\bin\x5092wtls.exe --cert=..\crts\SRK%i%_sha256_%kl%_65537_v3_ca_crt.pem ^
--key=.\%ca_key% ^
-w ..\crts\SRK%i%_sha256_%kl%_65537_wtls.crt ^
-p .\key_pass.txt

:: Convert CSF crts
..\mingw32\bin\x5092wtls.exe --cert=..\crts\CSF%i%_1_sha256_%kl%_65537_v3_ca_crt.pem ^
--key=.\SRK%i%_sha256_%kl%_65537_v3_ca_key.pem ^
-w ..\crts\CSF%i%_1_sha256_%kl%_65537_wtls.crt ^
-p .\key_pass.txt

:: Convert IMG certs
..\mingw32\bin\x5092wtls.exe --cert=..\crts\IMG%i%_1_sha256_%kl%_65537_v3_usr_crt.pem ^
--key=.\CSF%i%_1_sha256_%kl%_65537_v3_ca_key.pem ^
-w ..\crts\IMG%i%_1_sha256_%kl%_65537_wtls.crt ^
-p .\key_pass.txt

set srk_list=%srk_list%..\crts\SRK%i%_sha256_%kl%_65537_wtls.crt,
set /a i += 1
if %i%==%max% goto FINISHED
goto GEN_WTLS
:FINISHED

:: Finally generate SRK fuse and C file information
echo.
echo +++++++++++++++++++++++++++++++++++++
echo + Generating WTLS certificates info +
echo +++++++++++++++++++++++++++++++++++++
echo.
..\mingw32\bin\srktool.exe --hab_ver=3 --certs=%srk_list% --o > ..\crts\srk_wtls_cert_efuse_info.txt

echo done.
