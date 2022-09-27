#include <stdio.h>
#include <pb-tools/pb-tools.h>
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
    printf("Punchboot-tools v%s\n", PB_TOOLS_VERSION_STRING);
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
    printf("    -v, --verbose               Verbose output (Can be issued multiple times to increase output)\n");
    printf("    -h, --help                  Show help\n");
    printf("    -V, --version               Show version\n");
    printf("    -t, --transport             Communication transport (Default: usb)\n");
    printf("        --device <uuid>         Select device\n");
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
    printf("    -l, --list                              List partitions\n");
    printf("    -w, --write <filename> --part <uuid>    Write data to partition\n");
    printf("    -c, --verify <filename> --part <uuid>   Verify data\n");
    printf("    -i, --install                           Install default partition table\n");
    printf("    -s, --show [--part <uuid>]              Show BPAK information\n");
    printf("    -D, --dump <filename> --part <uuid>     Dump partition to file\n");
    printf("    -R, --resize <blocks> --part <uuid>     Resize a partition\n");
    printf("    -F, --force                             Force operation without confirmation\n");
    help_common_footer();
}

void help_boot(void)
{
    help_common_header("boot");
    printf("boot options:\n");
    printf("    -l, --load <filename> [--part <uuid>]  Load image into ram and boot\n");
    printf("    -b, --boot <uuid>                      Boot partition with <uuid>\n");
    printf("    -a, --activate <uuid>                  Activate boot partition\n");
    printf("    -s, --status                           Display boot status\n");
    printf("\nCommon boot options:\n");
    printf("    --verbose-boot                         Verbose output\n");
    help_common_footer();
}

void help_dev(void)
{
    help_common_header("dev");
    printf("dev options:\n");
    printf("    -S, --show                  Display device information\n");
    printf("    -r, --reset                 Reset board\n");
    printf("    -w, --wait <seconds>        Wait for device to enumerate\n");
    help_common_footer();
}

void help_board(void)
{
    help_common_header("board");
    printf("board options:\n");
    printf("    -b, --command <command> [--args <args>] Issue board specific command\n");
    printf("    -s, --show                              Show board status\n");
    help_common_footer();
}

void help_auth(void)
{
    help_common_header("auth");
    printf("auth options:\n");
    printf("    -T, --token <filename> --key-id <id>     Authenticate\n");
    printf("    -P, --password <password>\n");
    printf("        --set-otp-password <password>        Set OTP password\n");
    help_common_footer();
}

void help_slc(void)
{
    help_common_header("slc");
    printf("slc options:\n");
    printf("    -s, --show                    Show SLC status\n");
    printf("\nDangerous slc options:\n");
    printf("    -C, --set-configuration       Configure device fuses\n");
    printf("    -L, --set-configuration-lock  Lock configuration\n");
    printf("    -E, --set-end-of-life         End of life\n");
    printf("    -R, --revoke-key <id>         Revoke key\n");
    printf("\nSLC common options:\n");
    printf("        --force                   Force\n");
    help_common_footer();
}
