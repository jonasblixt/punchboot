/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The block device conformance tests (conformance/bio_conformance.h, bio.md
 * 8) against the devices that need no model: the tests' RAM disk, with 512
 * byte and with 4 KiB blocks and through a partition, and the host
 * platform's disk file and its memory mapped flash.
 *
 * None of them has an erase. A write that fails half way: the RAM disk's
 * fault injection, and for the host's files a file size limit
 * (RLIMIT_FSIZE) at the block where the medium is to fail, which cuts a
 * pwrite() short or refuses it.
 *
 * The MMC and SD drivers run the same tests on their card models, in
 * test_mmc_emmc.c, test_mmc_sd.c and test_imx_usdhc.c, and the FlexSPI's
 * driver on its NOR model, in test_imx_flexspi.c.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <dirent.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <plat/host/host.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>

#include "conformance/bio_conformance.h"
#include "fakes/ramdisk.h"

#define DISK_BYTES (4 * BIO_CONFORMANCE_MIN_BYTES)
#define PART_FIRST 100

/* Where pb-host maps its flash (src/board/host), and its size */
#define NOR_BASE   0x70000000
#define NOR_BYTES  (512 * 1024)

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
#define UU_PART \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"

static struct ramdisk *disk;
static char disk_path[64];
static char nor_path[64];

static void ramdisk_fail_writes(lba_t lba, size_t blocks)
{
    (void)lba;
    disk->fail_writes_after_blocks = (int)blocks;
}

static void ramdisk_fail_reads(void)
{
    disk->fail_reads_after = 0;
}

static void ramdisk_fault_end(void)
{
    disk->fail_writes_after_blocks = -1;
    disk->fail_reads_after = -1;
}

static const struct bio_conformance_fault ramdisk_fault = {
    .fail_writes = ramdisk_fail_writes,
    .fail_reads = ramdisk_fail_reads,
    .end = ramdisk_fault_end,
};

static struct rlimit fsize_before;
static void (*sigxfsz_before)(int);

/* The process may write its files up to the block where the medium is to
 * fail, and not past it: a pwrite() that crosses it is cut short, one
 * that starts there is refused (EFBIG, with SIGXFSZ ignored) */
static void file_fail_writes(lba_t lba, size_t blocks)
{
    struct rlimit limit;
    int64_t first = bio_get_first_block(bio_conformance_dev);
    ssize_t bs = bio_block_size(bio_conformance_dev);

    assert_true(first >= 0 && bs > 0);
    assert_int_equal(getrlimit(RLIMIT_FSIZE, &fsize_before), 0);

    /* What is buffered for the test's output goes out before the limit */
    fflush(stdout);
    fflush(stderr);
    sigxfsz_before = signal(SIGXFSZ, SIG_IGN);

    limit = fsize_before;
    limit.rlim_cur = (rlim_t)((uint64_t)first + lba + blocks) * (rlim_t)bs;
    assert_true(limit.rlim_max == RLIM_INFINITY || limit.rlim_cur <= limit.rlim_max);
    assert_int_equal(setrlimit(RLIMIT_FSIZE, &limit), 0);
}

static void file_fault_end(void)
{
    assert_int_equal(setrlimit(RLIMIT_FSIZE, &fsize_before), 0);
    signal(SIGXFSZ, sigxfsz_before);
}

static const struct bio_conformance_fault file_fault = {
    .fail_writes = file_fail_writes,
    .end = file_fault_end,
};

/* What the devices here have: no erase, no write_done */
static void conformance_device(bio_dev_t dev, const struct bio_conformance_fault *fault)
{
    assert_true(dev >= 0);
    bio_conformance_dev = dev;
    bio_conformance_erases = false;
    bio_conformance_fault = fault;
}

static int setup_ramdisk(size_t block_size)
{
    disk = ramdisk_new_block_size(DISK_BYTES / block_size, block_size);
    assert_non_null(disk);
    bio_reset();
    conformance_device(ramdisk_attach(disk, UU_DISK), &ramdisk_fault);
    return 0;
}

static int setup_ramdisk_512(void **state)
{
    (void)state;
    return setup_ramdisk(512);
}

static int setup_ramdisk_4k(void **state)
{
    (void)state;
    return setup_ramdisk(4096);
}

/* A partition that doesn't start at its device's first block */
static int setup_ramdisk_partition(void **state)
{
    lba_t part_last = PART_FIRST + BIO_CONFORMANCE_MIN_BYTES / RAMDISK_BLOCK_SIZE - 1;
    bio_dev_t dev;
    (void)state;

    setup_ramdisk(RAMDISK_BLOCK_SIZE);
    dev = bio_conformance_dev;
    conformance_device(
        bio_allocate_parent(dev, PART_FIRST, part_last, RAMDISK_BLOCK_SIZE, UU_PART, "Part"),
        &ramdisk_fault);
    return 0;
}

static int teardown_ramdisk(void **state)
{
    (void)state;
    ramdisk_free(disk);
    disk = NULL;
    return 0;
}

static int temp_file(char *path, size_t size)
{
    int fd;

    snprintf(path, size, "/tmp/pb-bio-conformance-XXXXXX");
    fd = mkstemp(path);

    if (fd < 0)
        return -1;

    close(fd);
    return 0;
}

static int setup_host_disk(void **state)
{
    (void)state;

    assert_int_equal(temp_file(disk_path, sizeof(disk_path)), 0);
    bio_reset();
    conformance_device(host_disk_init(disk_path, DISK_BYTES, UU_DISK, "Host disk"), &file_fault);
    return 0;
}

static int teardown_host_disk(void **state)
{
    (void)state;
    unlink(disk_path);
    return 0;
}

/* The process's open descriptors */
static int open_fds(void)
{
    DIR *dir = opendir("/proc/self/fd");
    int n = 0;

    assert_non_null(dir);

    while (readdir(dir) != NULL)
        n++;

    closedir(dir);
    return n;
}

/* A second init of the host's disk replaces the first: one file stays open,
 * and a failed init keeps none (F-406) */
static void test_host_disk_init_twice_keeps_one_file(void **state)
{
    (void)state;
    int once;

    assert_int_equal(temp_file(disk_path, sizeof(disk_path)), 0);

    /* The tests before this one had a disk too */
    bio_reset();
    assert_true(host_disk_init(disk_path, DISK_BYTES, UU_DISK, "Host disk") >= 0);
    once = open_fds();

    bio_reset();
    assert_true(host_disk_init(disk_path, DISK_BYTES, UU_DISK, "Host disk") >= 0);
    assert_int_equal(open_fds(), once);

    /* A file that can't be opened: the first one's is closed all the same */
    bio_reset();
    assert_true(host_disk_init("/nonexistent/pb-disk", DISK_BYTES, UU_DISK, "Host disk") < 0);
    assert_int_equal(open_fds(), once - 1);

    unlink(disk_path);
}

/* The flash is mapped once, as pb-host does it in plat_early_init() */
static int group_setup_host_nor(void **state)
{
    (void)state;

    assert_int_equal(temp_file(nor_path, sizeof(nor_path)), 0);
    assert_int_equal(host_nor_map(nor_path, NOR_BASE, NOR_BYTES), PB_OK);
    return 0;
}

static int group_teardown_host_nor(void **state)
{
    (void)state;
    unlink(nor_path);
    return 0;
}

static int setup_host_nor(void **state)
{
    (void)state;

    bio_reset();
    conformance_device(host_nor_init(UU_DISK, "Host NOR"), &file_fault);
    return 0;
}

int main(void)
{
    const struct CMUnitTest ramdisk_512[] = {
        BIO_CONFORMANCE_TESTS(setup_ramdisk_512, teardown_ramdisk),
    };
    const struct CMUnitTest ramdisk_4k[] = {
        BIO_CONFORMANCE_TESTS(setup_ramdisk_4k, teardown_ramdisk),
    };
    const struct CMUnitTest ramdisk_partition[] = {
        BIO_CONFORMANCE_TESTS(setup_ramdisk_partition, teardown_ramdisk),
    };
    const struct CMUnitTest host_disk[] = {
        BIO_CONFORMANCE_TESTS(setup_host_disk, teardown_host_disk),
    };
    const struct CMUnitTest host_nor[] = {
        BIO_CONFORMANCE_TESTS(setup_host_nor, NULL),
    };
    const struct CMUnitTest host_disk_twice[] = {
        cmocka_unit_test(test_host_disk_init_twice_keeps_one_file),
    };
    int failed = 0;

    failed += cmocka_run_group_tests_name("RAM disk, 512 byte blocks", ramdisk_512, NULL, NULL);
    failed += cmocka_run_group_tests_name("RAM disk, 4 KiB blocks", ramdisk_4k, NULL, NULL);
    failed += cmocka_run_group_tests_name("RAM disk, a partition", ramdisk_partition, NULL, NULL);
    failed += cmocka_run_group_tests_name("Host disk", host_disk, NULL, NULL);
    failed += cmocka_run_group_tests_name("Host disk init", host_disk_twice, NULL, NULL);
    failed += cmocka_run_group_tests_name(
        "Host NOR", host_nor, group_setup_host_nor, group_teardown_host_nor);

    return failed;
}
