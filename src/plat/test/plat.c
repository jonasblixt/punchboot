#include <board.h>
#include <io.h>
#include <tinyprintf.h>
#include <plat.h>
#include <string.h>
#include <plat/test/virtio.h>
#include <plat/test/virtio_block.h>


uint32_t plat_early_init(void)
{
    board_init();
    return PB_OK;

}

uint32_t plat_get_us_tick(void)
{
    return 0;
}

