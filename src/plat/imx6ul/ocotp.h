#include <pb_types.h>

struct ocotp_dev {
    __iomem base;
};

void ocotp_init(struct ocotp_dev *dev);


