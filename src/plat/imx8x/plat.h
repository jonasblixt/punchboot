#ifndef __IMX8M_PLAT_H__
#define __IMX8M_PLAT_H__

#define IMX8X_FUSE_ROW(__r, __d) \
        {.bank = __r , .word = 0, .description = __d, .status = FUSE_VALID}

#define IMX8X_FUSE_ROW_VAL(__r,__d,__v) \
        {.bank = __r , .word = 0 , .description = __d, \
         .default_value = __v, .status = FUSE_VALID}

#define IMX8X_FUSE_END { .status = FUSE_INVALID }

#endif
