#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/ramlog.h>

static const struct console_ops *ops;
static uintptr_t base;
static bool uart_muted;

#ifdef CONFIG_RAMLOG
/* Messages muted before console_init(), as RAM log positions [start, end),
 * so that the replay leaves them out. Muted messages past the last slot are
 * replayed. */
#define EARLY_MUTED_MAX 32

static struct {
    uint32_t start;
    uint32_t end;
} early_muted[EARLY_MUTED_MAX];

static unsigned int no_of_early_muted;
#endif

static bool console_has_output(void)
{
    return ops != NULL && ops->putc != NULL;
}

#ifdef CONFIG_RAMLOG
/* Write out what was only logged to RAM while there was no UART, except the
 * muted messages */
static void console_flush_ramlog(void)
{
    char buf[64];
    uint32_t head = ramlog_head();
    uint32_t oldest = head - ramlog_length();
    uint32_t pos = oldest;
    unsigned int r = 0;

    while (pos < head) {
        uint32_t end = head;
        size_t n;

        while (r < no_of_early_muted && early_muted[r].end <= pos)
            r++;

        if (r < no_of_early_muted) {
            if (early_muted[r].start <= pos) {
                pos = early_muted[r].end;
                continue;
            }
            end = early_muted[r].start;
        }

        n = ramlog_read(pos - oldest, buf, MIN(sizeof(buf), (size_t)(end - pos)));

        if (n == 0)
            break;

        for (size_t i = 0; i < n; i++)
            ops->putc(base, buf[i]);
        pos += n;
    }

    no_of_early_muted = 0;
}
#endif

void console_init(uintptr_t base_, const struct console_ops *ops_)
{
    bool had_output = console_has_output();

    ops = ops_;
    base = base_;

#ifdef CONFIG_RAMLOG
    if (!had_output && console_has_output())
        console_flush_ramlog();
#else
    (void)had_output;
#endif

#ifdef CONFIG_PRINT_BOOT_BANNER
    printf("\n\rPB " PB_VERSION ", %s (%i)\n\r", plat_boot_reason_str(), plat_boot_reason());
#endif
}

void console_uart_mute(bool mute)
{
    if (mute == uart_muted)
        return;

#ifdef CONFIG_RAMLOG
    if (!console_has_output() && no_of_early_muted < EARLY_MUTED_MAX) {
        if (mute) {
            early_muted[no_of_early_muted].start = ramlog_head();
        } else {
            early_muted[no_of_early_muted].end = ramlog_head();
            no_of_early_muted++;
        }
    }
#endif

    uart_muted = mute;
}

void console_putc(char c)
{
#ifdef CONFIG_RAMLOG
    ramlog_putc(c);
#endif
    /* Before console_init(), on boards without a UART (NULL ops) and while
     * muted, the output only goes to the RAM log */
    if (!uart_muted && console_has_output()) {
        ops->putc(base, c);
    }
}
