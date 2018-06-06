#include <pb.h>

struct ocotp_dev {
    __iomem base;
};

#define OCOTP_CTRL           0x0000
#define OCOTP_TIMING         0x0010
#define OCOTP_DATA           0x0020
#define OCOTP_READ_CTRL      0x0030
#define OCOTP_READ_FUSE_DATA 0x0040

/* Control register */
#define OCOTP_CTRL_BUSY  (1 << 8)
#define OCOTP_CTRL_ERROR (1 << 9)


#define OCOTP_CTRL_WR_KEY    0x3E77

void ocotp_init(struct ocotp_dev *dev);
uint32_t ocotp_read (uint32_t bank, uint32_t row, uint32_t * value);
uint32_t ocotp_write(uint32_t bank, uint32_t row, uint32_t value);
    
   
