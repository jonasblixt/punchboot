#ifndef INCLUDE_PLAT_IMXRT_H
#define INCLUDE_PLAT_IMXRT_H

#include <pb/utils_def.h>

/* The OCOTP has 8 words per bank. The fuses are in bank 0 unless noted */
#define IMXRT_FUSE_WORDS_PER_BANK    (8)

#define IMXRT_FUSE_UNIQUE_1          (1)
#define IMXRT_FUSE_UNIQUE_2          (2)

/* 0x440[17:16] SPEED_GRADING (RM Table 22-9) */
#define IMXRT_FUSE_SPEED_GRADE_WORD  (4)
#define IMXRT_FUSE_SPEED_GRADE_SHIFT (16)
#define IMXRT_FUSE_SPEED_GRADE_MASK  (0x3)

/*
 * HAB's fuses are at the 6UL's places: SEC_CONFIG[1] (0x460[1]), SRK_HASH
 * (bank 3) and SRK_REVOKE[2:0] (bank 5 word 7). The RM lists them as
 * reserved, the SRM has them.
 */
#define IMXRT_FUSE_SEC_CONFIG_BANK   (0)
#define IMXRT_FUSE_SEC_CONFIG_WORD   (6)
#define IMXRT_FUSE_SEC_CONFIG_CLOSED BIT(1)
#define IMXRT_FUSE_SRK_BANK          (3)
#define IMXRT_FUSE_SRK_WORD          (0)
#define IMXRT_FUSE_REVOKE_BANK       (5)
#define IMXRT_FUSE_REVOKE_WORD       (7)
#define IMXRT_FUSE_REVOCABLE_SRKS    (3)

#endif
