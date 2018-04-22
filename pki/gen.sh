#!/bin/sh

openssl genrsa -out test_rsa_private.pem 4096
openssl rsa -in test_rsa_private.pem -out test_rsa_private.der -outform der
openssl rsa -in test_rsa_private.pem -out test_rsa_public.der -outform der -pubout
