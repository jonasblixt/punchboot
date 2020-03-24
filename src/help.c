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
    printf("    auth                        Authentication commands\n");
    printf("    board                       Board specific commands\n");
    printf("    slc                         Manage security life cycle\n");
    help_common_footer();
}

void help_part(void)
{
    help_common_header("part");
    printf("part options:\n");
    printf("    -l, --list                          List partitions\n");
    printf("    -w, --write <filename> [part uuid]  Write data to partition\n");
    printf("    -i, --install                       Install default partition table\n");
    printf("    -s, --show [uuid]                   Show BPAK information\n");
    help_common_footer();
}

void help_dev(void)
{
    help_common_header("dev");
    printf("dev options:\n");
    printf("    -S, --show                  Display device information\n");
    printf("    -r, --reset                 Reset board\n");
    help_common_footer();
}

void help_board(void)
{
    help_common_header("board");
    printf("board options:\n");
    printf("    -b, --board-cmd             Issue board specific command\n");
    printf("    -s, --board-status          Show board status\n");
}

void help_auth(void)
{
    help_common_header("auth");
    printf("auth options:\n");
    printf("    -a, --authenticate          Authenticate\n");
    printf("        --set-otp-password      Set OTP password\n");
}

void help_slc(void)
{
    help_common_header("slc");
    printf("slc options:\n");
    printf("    -C, --set-configuration       Configure device fuses\n");
    printf("    -L, --set-configuration-lock  Lock configuration\n");
    printf("    -E, --set-end-of-life         End of life\n");
    help_common_footer();
}
