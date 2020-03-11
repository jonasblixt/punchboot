#include <stdio.h>
#include "tool.h"

static int verbosity;

void pb_inc_verbosity(void)
{
    verbosity++;
}

int pb_get_verbosity(void)
{
    return verbosity;
}

void print_version(void)
{
    printf("Punchboot-tools v%s\n", PACKAGE_VERSION);
}

static void help_common_header(const char *action)
{
    print_version();
    printf("\n");
    printf("punchboot %s [options]\n\n", action);
}

static void help_common_footer(void)
{
    printf("\n");
    printf("Common options:\n");
    printf("    -v, --verbose               Verbose output\n");
    printf("    -h, --help                  Show help\n");
    printf("    -V, --version               Show version\n");
    printf("    -t, --transport             Communication transport (Default: usb)\n");
}

void help_main(void)
{
    help_common_header("<action>");
    printf("Actions:\n");
    printf("    dev                         Device commands\n");
    printf("    boot                        Boot commands\n");
    printf("    part                        Partition mananagement\n");
    help_common_footer();
}

void help_dev(void)
{
    help_common_header("dev");
    printf("dev options:\n");
    printf("    -S, --show                  Display device information\n");
    printf("    -C, --configure             Configure device fuses\n");
    printf("    -L, --lock                  Lock configuration\n");
    printf("    -a, --authenticate          Authenticate\n");
    printf("    -b, --board-cmd             Issue board specific command\n");
    printf("    -r, --reset                 Reset board\n");
    help_common_footer();
}
