#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import sys
from Crypto.PublicKey import RSA


def import_rsa_key(fn):
    f = open(fn,"rb")
    raw_key_data = f.read()
    f.close()
    k = RSA.importKey(raw_key_data)

    key_mod_bytes = k.publickey().n.to_bytes(512, sys.byteorder)
    key_mod_bytearray = bytearray(key_mod_bytes)

    key_exp_bytes = k.publickey().e.to_bytes(3, sys.byteorder)
    key_exp_bytearray = bytearray(key_exp_bytes)

    return (key_mod_bytearray, key_exp_bytearray)

if __name__ == "__main__":
    key_kind = sys.argv[1]
    input_files = sys.argv[2:]
    
    print("/*")
    print(" AUTOMATICALLY GENERATED - DO NOT EDIT")
    print ("Importing %s keys"%(key_kind))
    print ("Input keys: " + str.join(",",input_files))
    print ("*/")
    print ()
    print("#include <crypto.h>")
    key_count = 0

    if key_kind == "RSA":
        for fn in input_files:
            n, e = import_rsa_key(fn)
            print ("const struct pb_rsa4096_key key%u= {"%(key_count))
            print (".mod = {")
            count = 0
            for b in n[::-1]:
                print ("0x%x," % b,end='')
                if count % 10 == 0:
                    print ()
                count = count + 1
            print("},")

            print (".exp = {")
            for b in e[::-1]:
                print ("0x%x," % b,end='')
            print("},")
            print ("};")

            key_count = key_count + 1
        #for fn

        print ("const struct pb_key keys[] = {")

        for n in range(key_count):
            print (" {.kind = PB_KEY_RSA4096, .id = %u, .data = &key%u},"%(n,n))
        print("};")
        print("const uint8_t no_of_keys = %u;"%key_count)
    #if key

    if key_kind == "EC384":
        for fn in input_files:
            n, e = import_rsa_key(fn)
            print ("const struct pb_rsa4096_key key%u= {"%(key_count))
            print (".mod = {")
            count = 0
            for b in n[::-1]:
                print ("0x%x," % b,end='')
                if count % 10 == 0:
                    print ()
                count = count + 1
            print("},")

            print (".exp = {")
            for b in e[::-1]:
                print ("0x%x," % b,end='')
            print("},")
            print ("};")

            key_count = key_count + 1
        #for fn

        print ("const struct pb_key keys[] = {")

        for n in range(key_count):
            print (" {.kind = PB_KEY_RSA4096, .id = %u, .data = &key%u},"%(n,n))
        print("};")
        print("const uint8_t no_of_keys = %u;"%key_count)
    #if key
