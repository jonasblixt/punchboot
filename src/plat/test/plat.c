#include <board.h>
#include <io.h>
#include <tinyprintf.h>
#include <plat.h>


uint32_t plat_early_init(void)
{
    return PB_OK;

}

uint32_t plat_get_us_tick(void)
{
    return 0;
}
