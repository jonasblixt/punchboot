#ifndef __IMX8M_PLAT_H__
#define __IMX8M_PLAT_H__

#define IMX8M_FUSE_BANK_WORD(__b,__w, __d) \
        {.bank = __b , .word = __w , .description = __d, .status = FUSE_VALID}

#define IMX8M_FUSE_BANK_WORD_VAL(__b,__w,__d,__v) \
        {.bank = __b , .word = __w , .description = __d, \
         .default_value = __v, .status = FUSE_VALID}

#define IMX8M_FUSE_END { .status = FUSE_INVALID }

#endif
