/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Device UUID (src/device_uuid.c), a UUID3 of the platform unique id.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/crypto/mbedtls.h>
#include <pb/device_uuid.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <string.h>
#include <uuid.h>

/* Same as the host platform, whose device UUID the auth token is made for */
static const uint8_t unique_id[8] = "\xbe\x4e\xfc\xb4\x32\x58\xcd\x63";
const char *platform_ns_uuid = "\x3f\xaf\xc6\xd3\xc3\x42\x4e\xdf\xa5\xa6\x0e\xb1\x39\xa7\x83\xb5";

static int unique_id_rc;

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    if (unique_id_rc != PB_OK)
        return unique_id_rc;

    memcpy(output, unique_id, sizeof(unique_id));
    *length = sizeof(unique_id);
    return PB_OK;
}

static void test_unique_id_error(void **state)
{
    uuid_t uu;
    (void)state;

    unique_id_rc = -PB_ERR_IO;
    assert_int_equal(device_uuid(uu), -PB_ERR_IO);
    unique_id_rc = PB_OK;
}

static void test_hash_error(void **state)
{
    uuid_t uu;
    uuid_t null_uu = { 0 };
    (void)state;

    /* No MD5 provider registered yet */
    assert_int_equal(device_uuid(uu), -PB_ERR_NOT_SUPPORTED);
    /* Nothing is cached on failure */
    assert_int_equal(mbedtls_pb_init(), PB_OK);
    assert_int_equal(device_uuid(uu), PB_OK);
    assert_memory_not_equal(uu, null_uu, sizeof(uu));
}

static void test_uuid_is_stable(void **state)
{
    uuid_t uu;
    char uu_str[37];
    (void)state;

    assert_int_equal(device_uuid(uu), PB_OK);
    uuid_unparse(uu, uu_str);
    assert_string_equal(uu_str, "02e49231-756e-35ee-a982-378e5ba866a9");

    /* Cached: the platform isn't asked again */
    unique_id_rc = -PB_ERR_IO;
    assert_int_equal(device_uuid(uu), PB_OK);
    unique_id_rc = PB_OK;
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_unique_id_error),
        cmocka_unit_test(test_hash_error),
        cmocka_unit_test(test_uuid_is_stable),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
