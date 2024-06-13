"""Punchboot CLI."""

from __future__ import annotations

import contextlib
import getopt
import logging
import os
import pathlib
import sys
import uuid
from functools import update_wrapper
from importlib.metadata import version
from typing import Any, Callable, Iterable, List, Optional, TypeVar, Union

import click
from click.shell_completion import CompletionItem

import punchboot

from . import Partition, Session, list_usb_devices

logger = logging.getLogger("pb")


def _completion_helper_init_session() -> Session:
    """Shell completion helper for initializing sessions.

    When click's shell completion is invoked the normal chain of functions
    are not called, specifically the 'cli' function is not called and thus
    the Session is never initiated.

    The completion shell script's for all supported shells however expose
    the variable 'COMP_WORDS' which contains the current command line,
    using getopt we can extract the relevant parameters to correctly
    initialize the Punchboot Session object.
    """
    cmd_line = os.environ["COMP_WORDS"].split()[1:]
    opts, _ = getopt.getopt(cmd_line, "t:u:s:", ["transport=", "device-uuid=", "socket="])
    skt_path: Optional[str] = None
    dev_uuid: Optional[uuid.UUID] = None
    for o, a in opts:
        if o in ("-s", "--socket"):
            skt_path = a
        if o in ("-t", "--transport") and a == "socket" and skt_path is None:
            skt_path = "/tmp/pb.sock"  # noqa: S108
        if o in ("-u", "--device-uuid"):
            dev_uuid = uuid.UUID(a)

    return Session(socket_path=skt_path, device_uuid=dev_uuid)


def _print_version(_ctx: click.Context, _param: click.Option, value: bool) -> None:
    if value:
        click.echo(f"Punchboot tool {version('punchboot')}")
        sys.exit(0)


def _get_board_name(uu: uuid.UUID) -> str:
    return Session(device_uuid=uu).device_get_boardname()


def _dev_completion_helper(
    _ctx: click.Context, _param: click.Option, _incomplete: str
) -> List[CompletionItem]:
    """Completion handler for USB attached devices."""
    return [CompletionItem(str(uu), help=_get_board_name(uu)) for uu in list_usb_devices()]


def _get_part_completion_helper(
    *, filt_write: bool = False, filt_read: bool = False, filt_boot: bool = False
) -> Callable[[click.Context, click.Parameter, str], List[CompletionItem]]:
    """Construct a completion handler for partitions with given filtering."""

    def _part_completion_helper(
        _ctx: click.Context, _param: click.Parameter, _incomplete: str
    ) -> List[CompletionItem]:
        """Completion handler for partitions."""
        with contextlib.suppress(Exception):
            s: Session = _completion_helper_init_session()
            parts: Iterable[Partition] = s.part_get_partitions()

            if filt_write:
                parts = [p for p in parts if p.writable]
            if filt_read:
                parts = [p for p in parts if p.readable]
            if filt_boot:
                parts = [p for p in parts if p.bootable]

            return [CompletionItem(str(p.uuid), help=p.description) for p in parts]

        return []  # Intentionally suppress all exceptions

    return _part_completion_helper


RT = TypeVar("RT")
TFunc = Callable[..., RT]


def pb_session(f: TFunc) -> TFunc:
    """Punchboot Session setup decorator."""

    @click.pass_context
    def new_func(ctx: click.Context, *args: Any, **kwargs: Any) -> Any:  # noqa: ANN401
        device_uuid: Optional[uuid.UUID] = ctx.obj["device-uuid"]
        socket: str = ctx.obj["socket"]
        transport: str = ctx.obj["transport"]
        try:
            if transport == "usb":
                logger.debug("Using USB transport")
                if device_uuid is None:
                    attached = list_usb_devices()
                    if len(attached) > 1:
                        click.echo(
                            f"Warning: More than one device is attached, using: {attached[0]}"
                        )
                return ctx.invoke(f, Session(device_uuid=device_uuid), *args, **kwargs)
            if transport == "socket":
                if device_uuid is not None:
                    msg = "It's not possible to address different devices over sockets"
                    raise click.BadParameter(msg)
                logger.debug("Using socket transport")
                return ctx.invoke(f, Session(socket_path=socket), *args, **kwargs)
        except punchboot.Error as e:
            error_descriptions = {
                punchboot.GenericError: "Generic error",
                punchboot.AuthenticationError: "Authentication failed",
                punchboot.NotAuthenticatedError: "Session requires authentication",
                punchboot.CommandError: "Command failed",
                punchboot.PartVerifyError: "Partition verification failed",
                punchboot.PartNotBootableError: "Partition is not bootable",
                punchboot.NoMemoryError: "Out of memory",
                punchboot.TransferError: "Transfer",
                punchboot.TimeoutError: "Timeout",
                punchboot.SignatureError: "Signature verifcation failed",
                punchboot.MemError: "Memory",
                punchboot.ArgumentError: "Bad argument",
                punchboot.NotFoundError: "Could not connect to device",
                punchboot.NotSupportedError: "Command not supported",
                punchboot.KeyRevokedError: "Key is revoked",
                punchboot.IOError: "I/O error",
            }

            raise click.ClickException(error_descriptions.get(type(e), "Generic error")) from e

    return update_wrapper(new_func, f)


@click.group(help="")
@click.option(
    "-V",
    "--version",
    is_flag=True,
    is_eager=True,
    expose_value=True,
    callback=_print_version,
    help="Display version and exit",
)
@click.option(
    "-v",
    "--verbose",
    is_flag=True,
    expose_value=True,
    help="Verbose debug output",
)
@click.option(
    "-t",
    "--transport",
    type=click.Choice(["usb", "socket"]),
    default="usb",
    expose_value=True,
    help="Communications transport",
)
@click.option(
    "-u",
    "--device-uuid",
    type=uuid.UUID,
    default=lambda: os.environ.get("PB_DEVICE_UUID"),
    expose_value=True,
    shell_complete=_dev_completion_helper,
    help="Select device by UUID (Can be used together with USB transport)",
)
@click.option(
    "-s",
    "--socket",
    type=click.Path(path_type=pathlib.Path),
    default=pathlib.Path("/tmp/pb.sock"),  # noqa: S108
    expose_value=True,
    help="Socket path",
)
@click.pass_context
def cli(
    ctx: click.Context,
    version: bool,  # noqa: ARG001
    verbose: bool,
    transport: str,
    device_uuid: uuid.UUID,
    socket: str,
) -> None:
    """Punchboot."""
    logging.basicConfig(level=logging.DEBUG if verbose else logging.INFO)
    ctx.obj = {"transport": transport, "device-uuid": device_uuid, "socket": socket}


@cli.group()
@click.pass_context
def dev(_ctx: click.Context) -> None:
    """Device management."""


@dev.command("reset")
@pb_session
@click.pass_context
def dev_reset(_ctx: click.Context, s: Session) -> None:
    """Reset the device."""
    s.device_reset()


@dev.command("show")
@pb_session
@click.pass_context
def dev_show(_ctx: click.Context, s: Session) -> None:
    """Show device info."""
    with contextlib.suppress(punchboot.NotAuthenticatedError):
        click.echo(f"{'Bootloader version:':<20}{s.device_get_punchboot_version()}")
    click.echo(f"{'Device UUID:':<20}{s.device_get_uuid()}")
    click.echo(f"{'Board name:':<20}{s.device_get_boardname()}")


@cli.group()
@click.pass_context
def auth(_ctx: click.Context) -> None:
    """Session authentication."""


@auth.command("password")
@click.option("set_flag", "--set", is_flag=True, default=False, help="Set password.")
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@click.argument(
    "password",
    expose_value=True,
    required=True,
)
@pb_session
@click.pass_context
def auth_password(
    _ctx: click.Context, s: Session, set_flag: bool, force: bool, password: str
) -> None:
    """Password authentication."""
    if set_flag and (force or click.confirm("Are you sure?")):
        s.auth_set_password(password)
    elif not set_flag:
        s.authenticate(password)


@auth.command("token")
@click.argument(
    "token",
    type=click.Path(path_type=pathlib.Path),
    expose_value=True,
)
@click.argument(
    "key-id",
    expose_value=True,
)
@pb_session
@click.pass_context
def auth_token(_ctx: click.Context, s: Session, token: pathlib.Path, key_id: str) -> None:
    """DSA token  authentication."""
    key_id_p: Union[int, str] = 0

    try:
        key_id_p = int(key_id, 0)
    except ValueError:
        key_id_p = key_id

    s.authenticate_dsa_token(token, key_id_p)


@cli.group()
@click.pass_context
def part(_ctx: click.Context) -> None:
    """Partition management."""


B_TO_KB = 1024
B_TO_MB = 1024 * 1024


@part.command("list")
@pb_session
@click.pass_context
def part_list(_ctx: click.Context, s: Session) -> None:
    """List partitions."""

    def _flag_helper(part: Partition) -> str:
        flags: Iterable[str] = ("B", "o", "W", "E", "R", "?", "?", "?")
        part_param: Iterable[bool] = (
            part.bootable,
            part.otp,
            part.writable,
            part.erase_before_write,
            part.readable,
            False,
            False,
            False,
        )
        return "".join(flag if is_set else "-" for flag, is_set in zip(flags, part_param))

    def _size_helper(part: Partition) -> str:
        part_bytes = (part.last_block - part.first_block + 1) * part.block_size
        if part_bytes > B_TO_MB:
            return f"{(part_bytes // B_TO_MB):<5} MB"
        if part_bytes > B_TO_KB:
            return f"{(part_bytes // B_TO_KB):<5} kB"

        return f"{part_bytes:<4} B"

    click.echo(f"{'Partition UUID':<37}   {'Flags':<8}   {'Size':<8}   {'Name':<16}")
    click.echo(f"{'--------------':<37}   {'-----':<8}   {'----':<8}   {'----':<16}")
    for part in s.part_get_partitions():
        click.echo(
            f"{part.uuid!s:<37}   {_flag_helper(part):<8}   {_size_helper(part):<7}   {part.description:<16}"  # noqa: E501
        )


@part.command("install")
@click.argument(
    "partition",
    type=click.UUID,
    shell_complete=_get_part_completion_helper(),
    required=True,
)
@click.option(
    "variant",
    "--variant",
    help="Partition table variant to install",
    type=int,
    default=0,
)
@pb_session
@click.pass_context
def part_install(_ctx: click.Context, s: Session, partition: uuid.UUID, variant: int) -> None:
    """Install partition table."""
    s.part_table_install(partition, variant)


@part.command("erase")
@click.argument(
    "part_uuid",
    type=click.UUID,
    shell_complete=_get_part_completion_helper(),
    required=True,
)
@pb_session
@click.pass_context
def part_erase(_ctx: click.Context, s: Session, part_uuid: uuid.UUID) -> None:
    """Erase partition."""
    logger.debug("Erasing partition %s...", part_uuid)

    def _update_pgbar(total: int, remaining: int) -> None:
        click.echo(f"\rErasing {total-remaining}/{total}", nl=False)

    s.part_erase(part_uuid, _update_pgbar)
    click.echo("")


@part.command("write")
@click.argument("file", type=click.Path(path_type=pathlib.Path), required=True)
@click.argument(
    "part_uuid",
    type=click.UUID,
    shell_complete=_get_part_completion_helper(filt_write=True),
    required=True,
)
@pb_session
@click.pass_context
def part_write(_ctx: click.Context, s: Session, part_uuid: uuid.UUID, file: pathlib.Path) -> None:
    """Write data to a partiton."""
    logger.debug("Writing %s to partition %s...", file, part_uuid)
    s.part_write(file, part_uuid)


@part.command("read")
@click.argument(
    "part_uuid",
    type=click.UUID,
    shell_complete=_get_part_completion_helper(filt_read=True),
    required=True,
)
@click.argument("file", type=click.Path(path_type=pathlib.Path), required=True)
@pb_session
@click.pass_context
def part_read(_ctx: click.Context, s: Session, file: pathlib.Path, part_uuid: uuid.UUID) -> None:
    """Read data from a partition."""
    s.part_read(file, part_uuid)


@part.command("verify")
@click.argument("file", type=click.Path(path_type=pathlib.Path), required=True)
@click.argument(
    "part_uuid",
    type=click.UUID,
    shell_complete=_get_part_completion_helper(),
    required=True,
)
@pb_session
@click.pass_context
def part_verify(_ctx: click.Context, s: Session, part_uuid: uuid.UUID, file: pathlib.Path) -> None:
    """Verify the contents of a partition."""
    logger.debug("Verifying %s against contents of partition %s...", file, part_uuid)
    s.part_verify(file, part_uuid)


@cli.group()
@click.pass_context
def board(_ctx: click.Context) -> None:
    """Board specific commands."""


@board.command("command")
@click.argument("command", type=str)
@click.argument("args", type=str, required=False)
@click.option(
    "out_fmt",
    "--output-fmt",
    type=click.Choice(["str", "binary"]),
    default="str",
    help="Print command result to stdout as binary or a string (Default: string)",
)
@click.option(
    "args_from_file",
    "--args-from-file",
    type=click.Path(path_type=pathlib.Path),
    required=False,
    help="Pass contents of a file as argument",
)
@pb_session
@click.pass_context
def board_command(
    _ctx: click.Context,
    s: Session,
    command: str,
    args: Optional[str],
    out_fmt: str,
    args_from_file: Optional[pathlib.Path],
) -> None:
    """Execute a board specific command."""
    board_command: Union[int, str] = 0
    board_args: bytes = b""

    try:
        board_command = int(command, 0)
    except ValueError:
        board_command = command

    if args and args_from_file:
        msg = "Use either --args or --args-from-file"
        raise click.BadParameter(msg)

    if args:
        board_args = args.encode() if args is not None else b""
    elif args_from_file:
        board_args = args_from_file.read_bytes()

    result: bytes = s.board_run_command(board_command, board_args)

    if out_fmt == "binary":
        sys.stdout.buffer.write(result)
    else:
        click.echo(result.decode().strip())


@board.command("status")
@pb_session
@click.pass_context
def board_status(_ctx: click.Context, s: Session) -> None:
    """Read board status."""
    click.echo(s.board_read_status())


@cli.group()
@click.pass_context
def boot(_ctx: click.Context) -> None:
    """Boot commands."""


@boot.command("partition")
@click.argument(
    "partition",
    type=click.UUID,
    shell_complete=_get_part_completion_helper(filt_boot=True),
    required=True,
)
@click.option(
    "verbose", "--verbose-boot", type=bool, default=False, is_flag=True, help="Verbose boot output"
)
@pb_session
@click.pass_context
def boot_partition(_ctx: click.Context, s: Session, partition: uuid.UUID, verbose: bool) -> None:
    """Boot from a partition."""
    s.boot_partition(partition, verbose)


@boot.command("bpak")
@click.argument(
    "file",
    type=click.Path(path_type=pathlib.Path),
    required=True,
)
@click.option(
    "pretend_part",
    "--pretend-part",
    type=click.UUID,
    shell_complete=_get_part_completion_helper(filt_boot=True),
    help="Pretend like we're booting from this block device",
)
@click.option(
    "verbose", "--verbose-boot", type=bool, default=False, is_flag=True, help="Verbose boot output"
)
@pb_session
@click.pass_context
def boot_bpak(
    _ctx: click.Context,
    s: Session,
    file: pathlib.Path,
    pretend_part: Optional[uuid.UUID],
    verbose: bool,
) -> None:
    """Load a bpak file into ram and run it."""
    uu = pretend_part if pretend_part is not None else uuid.UUID(bytes=b"\x00" * 16)
    s.boot_bpak(file, uu, verbose)


@boot.command("status")
@pb_session
@click.pass_context
def boot_status(_ctx: click.Context, s: Session) -> None:
    """Display boot status."""
    boot_uu, msg = s.boot_status()
    if boot_uu == uuid.UUID(bytes=b"\x00" * 16):
        click.echo("No boot partition is enabled.")
    else:
        click.echo(f"Active boot partition: {boot_uu}")

    if msg:
        click.echo(f"Status message: {msg}")


@boot.command("enable")
@click.argument(
    "part_uuid",
    type=click.UUID,
    shell_complete=_get_part_completion_helper(filt_boot=True),
    required=False,
)
@pb_session
@click.pass_context
def boot_set_boot_part(_ctx: click.Context, s: Session, part_uuid: uuid.UUID) -> None:
    """Configure active boot partition."""
    s.boot_set_boot_part(part_uuid)


@boot.command("disable")
@pb_session
@click.pass_context
def boot_disable(_ctx: click.Context, s: Session) -> None:
    """Disable boot."""
    s.boot_set_boot_part(uuid.UUID(bytes=b"\x00" * 16))


slc_warning: str = """WARNING: This is a permanent change, writing fuses can not be reverted. This could brick your device.

Are you sure?"""  # noqa: E501


@cli.group()
@click.pass_context
def slc(_ctx: click.Context) -> None:
    """Security Life Cycle."""


@slc.command("show")
@pb_session
@click.pass_context
def slc_show(_ctx: click.Context, s: Session) -> None:
    """Show SLC."""
    click.echo(f"Status: {s.slc_get_lifecycle().name}")
    click.echo(f"Active keys: {', '.join(f'0x{k:08x}' for k in s.slc_get_active_keys())}")
    click.echo(f"Revoked keys: {', '.join(f'0x{k:08x}' for k in s.slc_get_revoked_keys())}")


@slc.command("configure")
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@pb_session
@click.pass_context
def slc_configure(_ctx: click.Context, s: Session, force: bool) -> None:
    """Set SLC configure."""
    if force or click.confirm(slc_warning):
        s.slc_set_configuration()


@slc.command("lock")
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@pb_session
@click.pass_context
def slc_lock(_ctx: click.Context, s: Session, force: bool) -> None:
    """Set SLC configuration lock."""
    if force or click.confirm(slc_warning):
        s.slc_set_configuration_lock()


@slc.command("eol")
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@pb_session
@click.pass_context
def slc_eol(_ctx: click.Context, s: Session, force: bool) -> None:
    """Set SLC end of life."""
    if force or click.confirm(slc_warning):
        s.slc_set_end_of_life()


@slc.command("revoke-key")
@click.argument("key-id", type=str)
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@pb_session
@click.pass_context
def slc_revoke_key(_ctx: click.Context, s: Session, key_id: str, force: bool) -> None:
    """Revoke a key."""
    revoke_key_id: Union[int, str] = 0
    try:
        revoke_key_id = int(key_id, 0)
    except ValueError:
        revoke_key_id = key_id

    if force or click.confirm(slc_warning):
        s.slc_revoke_key(revoke_key_id)


@cli.command("completion")
@click.pass_context
def shell_completion_helper(_ctx: click.Context) -> None:
    """Shell completion helpers."""
    help_text = """
    Punchboot supports shell completions for bash, zsh and fish.
    Put one of the following lines in the appropriate .rc -file to enable
    shell completions.

    bash:
        eval "$(_PUNCHBOOT_COMPLETE=bash_source punchboot)"

    zsh:
        eval "$(_PUNCHBOOT_COMPLETE=zsh_source punchboot)"

    fish:
        _PUNCHBOOT_COMPLETE=fish_source punchboot | source
    """

    click.echo(help_text)


@cli.command("list")
def list_devices() -> None:
    """List Punchboot devices attached over USB."""
    for device_uu in list_usb_devices():
        click.echo(f"{device_uu} -- {_get_board_name(device_uu)}")


if __name__ == "__main__":
    cli()
