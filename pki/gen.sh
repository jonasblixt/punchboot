#!/bin/sh

openssl genrsa -out dev_rsa_private.pem 4096
openssl rsa -in dev_rsa_private.pem -out dev_rsa_private.der -outform der
openssl rsa -in dev_rsa_private.pem -out dev_rsa_public.der -outform der -pubout

openssl genrsa -out prod_rsa_private.pem 4096
openssl rsa -in prod_rsa_private.pem -out prod_rsa_private.der -outform der
openssl rsa -in prod_rsa_private.pem -out prod_rsa_public.der -outform der -pubout

openssl genrsa -out field1_rsa_private.pem 4096
openssl rsa -in field1_rsa_private.pem -out field1_rsa_private.der -outform der
openssl rsa -in field1_rsa_private.pem -out field1_rsa_public.der -outform der -pubout

openssl genrsa -out field2_rsa_private.pem 4096
openssl rsa -in field2_rsa_private.pem -out field2_rsa_private.der -outform der
openssl rsa -in field2_rsa_private.pem -out field2_rsa_public.der -outform der -pubout


