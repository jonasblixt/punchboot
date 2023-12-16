"""Punchboot CLI."""
import contextlib
import getopt
import logging
import os
import pathlib
import sys
import uuid
from functools import update_wrapper
from importlib.metadata import version
from typing import Iterable, Optional, Union

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
    _skt_path: Union[str, None] = None
    _dev_uuid: Union[uuid.UUID, None] = None
    for o, a in opts:
        if (o == "-t" or o == "--transport") and a == "socket" and _skt_path is None:
            _skt_path = "/tmp/pb.sock"
        if o == "-s" or o == "--socket":
            _skt_path = a
        if o == "-u" or o == "--device-uuid":
            _dev_uuid = uuid.UUID(a)

    return Session(socket_path=_skt_path, device_uuid=_dev_uuid)


class PBPartType(click.ParamType):
    """Helper class for partition UUID shell completion."""

    name = "pbpart"
    _filt_write: bool = False
    _filt_read: bool = False
    _filt_boot: bool = False

    def __init__(  # noqa: D107
        self, filt_write: bool = False, filt_read: bool = False, filt_boot: bool = False
    ):
        self._filt_write = filt_write
        self._filt_read = filt_read
        self._filt_boot = filt_boot

    def shell_complete(self, ctx: click.Context, param: click.Parameter, incomplete):
        """Completion handler for partitions."""
        try:
            s: Session = _completion_helper_init_session()
            _parts: Iterable[Partition] = s.part_get_partitions()

            if self._filt_write:
                _parts = [_p for _p in _parts if _p.writable]
            if self._filt_read:
                _parts = [_p for _p in _parts if _p.readable]
            if self._filt_boot:
                _parts = [_p for _p in _parts if _p.bootable]

            return [CompletionItem(str(_p.uuid), help=_p.description) for _p in _parts]
        except Exception:
            return []  # Intentionally suppress all exceptions


def _print_version(ctx: click.Context, param: click.Option, value: bool):
    if value:
        click.echo(f"Punchboot tool {version('punchboot')}")
        exit(0)


def _get_board_name(uu: uuid.UUID) -> str:
    return Session(device_uuid=uu).device_get_boardname()


def _dev_completion_helper(ctx: click.Context, param: click.Option, incomplete):
    """Completion handler for USB attached devices."""
    return [CompletionItem(str(_uu), help=_get_board_name(_uu)) for _uu in list_usb_devices()]


def pb_session(f):
    """Punchboot Session setup decorator."""

    @click.pass_context
    def new_func(ctx: click.Context, *args, **kwargs):
        device_uuid: uuid.UUID = ctx.obj["device-uuid"]
        socket: str = ctx.obj["socket"]
        transport: str = ctx.obj["transport"]
        try:
            if transport == "usb":
                logger.debug("Using USB transport")
                if device_uuid is None:
                    _attached: Iterable[uuid.UUID] = list_usb_devices()
                    if len(_attached) > 1:
                        click.echo(
                            f"Warning: More than one device is attached, using: {_attached[0]}"
                        )
                return ctx.invoke(f, Session(device_uuid=device_uuid), *args, **kwargs)
            elif transport == "socket":
                if device_uuid is not None:
                    raise click.BadParameter(
                        "It's not possible to address different devices over sockets"
                    )
                logger.debug("Using socket transport")
                return ctx.invoke(f, Session(socket_path=socket), *args, **kwargs)
        except punchboot.GenericError:
            raise click.ClickException("Generic error")
        except punchboot.AuthenticationError:
            raise click.ClickException("Authentication failed")
        except punchboot.NotAuthenticatedError:
            raise click.ClickException("Session requires authentication")
        except punchboot.CommandError:
            raise click.ClickException("Command failed")
        except punchboot.PartVerifyError:
            raise click.ClickException("Partition verification failed")
        except punchboot.PartNotBootableError:
            raise click.ClickException("Partition is not bootable")
        except punchboot.NoMemoryError:
            raise click.ClickException("Out of memory")
        except punchboot.TransferError:
            raise click.ClickException("Transfer")
        except punchboot.TimeoutError:
            raise click.ClickException("Timeout")
        except punchboot.SignatureError:
            raise click.ClickException("Signature verifcation failed")
        except punchboot.MemError:
            raise click.ClickException("Memory")
        except punchboot.ArgumentError:
            raise click.ClickException("Bad argument")
        except punchboot.NotFoundError:
            raise click.ClickException("Could not connect to device")
        except punchboot.NotSupportedError:
            raise click.ClickException("Command not supported")
        except punchboot.KeyRevokedError:
            raise click.ClickException("Key is revoked")
        except punchboot.IOError:
            raise click.ClickException("I/O error")

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
    default=pathlib.Path("/tmp/pb.sock"),
    expose_value=True,
    help="Socket path",
)
@click.pass_context
def cli(
    ctx: click.Context,
    version: bool,
    verbose: bool,
    transport: str,
    device_uuid: uuid.UUID,
    socket: str,
):
    """Punchboot."""
    logging.basicConfig(level=logging.DEBUG if verbose else logging.INFO)
    ctx.obj = {"transport": transport, "device-uuid": device_uuid, "socket": socket}


@cli.group()
@click.pass_context
def dev(ctx: click.Context):
    """Device management."""


@dev.command("reset")
@pb_session
@click.pass_context
def dev_reset(ctx: click.Context, s: Session):
    """Reset the device."""
    s.device_reset()


@dev.command("show")
@pb_session
@click.pass_context
def dev_show(ctx: click.Context, s: Session):
    """Show device info."""
    with contextlib.suppress(punchboot.NotAuthenticatedError):
        click.echo(f"{'Bootloader version:':<20}{s.device_get_punchboot_version()}")
    click.echo(f"{'Device UUID:':<20}{s.device_get_uuid()}")
    click.echo(f"{'Board name:':<20}{s.device_get_boardname()}")


@cli.group()
@click.pass_context
def auth(ctx: click.Context):
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
def auth_password(ctx: click.Context, s: Session, set_flag: bool, force: bool, password: str):
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
def auth_token(ctx: click.Context, s: Session, token: pathlib.Path, key_id: str):
    """DSA token  authentication."""
    _key_id: Union[int, str] = 0

    try:
        _key_id = int(key_id, 0)
    except ValueError:
        _key_id = key_id

    s.authenticate_dsa_token(token, _key_id)


@cli.group()
@click.pass_context
def part(ctx: click.Context):
    """Partition management."""


@part.command("list")
@pb_session
@click.pass_context
def part_list(ctx: click.Context, s: Session):
    """List partitions."""

    def _flag_helper(part: Partition) -> str:
        _flags: Iterable[str] = ("B", "o", "W", "E", "R", "?", "?", "?")
        _part_param: Iterable[bool] = (
            part.bootable,
            part.otp,
            part.writable,
            part.erase_before_write,
            part.readable,
            False,
            False,
            False,
        )
        return "".join([_flag if _set else "-" for _flag, _set in zip(_flags, _part_param)])

    def _size_helper(part: Partition) -> str:
        _part_bytes: int = (part.last_block - part.first_block + 1) * part.block_size
        if _part_bytes > 1024 * 1024:
            return f"{int(_part_bytes / (1024 * 1024)):<4} MB"
        elif _part_bytes > 1024:
            return f"{int(_part_bytes / 1024):<4} kB"
        else:
            return f"{_part_bytes:<4} B"

    click.echo(f"{'Partition UUID':<37}   {'Flags':<8}   {'Size':<7}   {'Name':<16}")
    click.echo(f"{'--------------':<37}   {'-----':<8}   {'----':<7}   {'----':<16}")
    for part in s.part_get_partitions():
        click.echo(
                f"{str(part.uuid):<37}   {_flag_helper(part):<8}   {_size_helper(part):<7}   {part.description:<16}"  # noqa: E501
        )


@part.command("install")
@click.argument(
    "partition",
    type=PBPartType(),
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
def part_install(ctx: click.Context, s: Session, partition: uuid.UUID, variant: int):
    """Install partition table."""
    s.part_table_install(partition, variant)

@part.command("erase")
@click.argument(
    "part_uuid",
    type=PBPartType(),
    required=True,
)
@pb_session
@click.pass_context
def part_erase(ctx: click.Context, s: Session, part_uuid: uuid.UUID):
    """Erase partition."""
    logger.debug(f"Erasing partition {part_uuid}...")
    def _update_pgbar(total: int, remaining: int):
        click.echo(f"\rErasing {total-remaining}/{total}", nl=False)
    s.part_erase(part_uuid, _update_pgbar)
    click.echo("")


@part.command("write")
@click.argument("file", type=click.Path(path_type=pathlib.Path), required=True)
@click.argument(
    "part_uuid",
    type=PBPartType(filt_write=True),
    required=True,
)
@pb_session
@click.pass_context
def part_write(ctx: click.Context, s: Session, part_uuid: uuid.UUID, file: pathlib.Path):
    """Write data to a partiton."""
    logger.debug(f"Writing {file} to partition {part_uuid}...")
    s.part_write(file, part_uuid)


@part.command("read")
@click.argument(
    "part_uuid",
    type=PBPartType(filt_read=True),
    required=True,
)
@click.argument("file", type=click.Path(path_type=pathlib.Path), required=True)
@pb_session
@click.pass_context
def part_read(ctx: click.Context, s: Session, file: pathlib.Path, part_uuid: uuid.UUID):
    """Read data from a partition."""
    s.part_read(file, part_uuid)


@part.command("verify")
@click.argument("file", type=click.Path(path_type=pathlib.Path), required=True)
@click.argument(
    "part_uuid",
    type=PBPartType(),
    required=True,
)
@pb_session
@click.pass_context
def part_verify(ctx: click.Context, s: Session, part_uuid: uuid.UUID, file: pathlib.Path):
    """Verify the contents of a partition."""
    logger.debug(f"Verifying {file} against contents of partition {part_uuid}...")
    s.part_verify(file, part_uuid)


@cli.group()
@click.pass_context
def board(ctx: click.Context):
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
    ctx: click.Context,
    s: Session,
    command: str,
    args: Optional[str],
    out_fmt: str,
    args_from_file: Optional[pathlib.Path],
):
    """Execute a board specific command."""
    _command: Union[int, str] = 0
    _args: bytes = b""

    try:
        _command = int(command, 0)
    except ValueError:
        _command = command

    if args and args_from_file:
        raise click.BadParameter("Use either --args or --args-from-file")
    elif args:
        _args = args.encode("utf-8") if args is not None else b""
    elif args_from_file:
        _args = args_from_file.read_bytes()

    result: bytes = s.board_run_command(_command, _args)

    if out_fmt == "binary":
        sys.stdout.buffer.write(result)
    else:
        click.echo(result.decode("utf-8").strip())


@board.command("status")
@pb_session
@click.pass_context
def board_status(ctx: click.Context, s: Session):
    """Read board status."""
    click.echo(s.board_read_status())


@cli.group()
@click.pass_context
def boot(ctx: click.Context):
    """Boot commands."""


@boot.command("partition")
@click.argument(
    "partition",
    type=PBPartType(filt_boot=True),
    required=True,
)
@click.option(
    "verbose", "--verbose-boot", type=bool, default=False, is_flag=True, help="Verbose boot output"
)
@pb_session
@click.pass_context
def boot_partition(ctx: click.Context, s: Session, partition: uuid.UUID, verbose: bool):
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
    type=PBPartType(filt_boot=True),
    help="Pretend like we're booting from this block device",
)
@click.option(
    "verbose", "--verbose-boot", type=bool, default=False, is_flag=True, help="Verbose boot output"
)
@pb_session
@click.pass_context
def boot_bpak(
    ctx: click.Context, s: Session, file: pathlib.Path, pretend_part: uuid.UUID, verbose: bool
):
    """Load a bpak file into ram and run it."""
    _uu = pretend_part if pretend_part is not None else uuid.UUID(bytes=b"\x00" * 16)
    s.boot_bpak(file, _uu, verbose)


@boot.command("status")
@pb_session
@click.pass_context
def boot_status(ctx: click.Context, s: Session):
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
    type=PBPartType(filt_boot=True),
    required=False,
)
@pb_session
@click.pass_context
def boot_set_boot_part(ctx: click.Context, s: Session, part_uuid: uuid.UUID):
    """Configure active boot partition."""
    s.boot_set_boot_part(part_uuid)


@boot.command("disable")
@pb_session
@click.pass_context
def boot_disable(ctx: click.Context, s: Session):
    """Disable boot."""
    s.boot_set_boot_part(uuid.UUID(bytes=b"\x00" * 16))


_slc_warning: str = """WARNING: This is a permanent change, writing fuses can not be reverted. This could brick your device.

Are you sure?"""  # noqa: E501


@cli.group()
@click.pass_context
def slc(ctx: click.Context):
    """Security Life Cycle."""


@slc.command("show")
@pb_session
@click.pass_context
def slc_show(ctx: click.Context, s: Session):
    """Show SLC."""
    click.echo(f"Status: {s.slc_get_lifecycle().name}")
    click.echo(f"Active keys: {', '.join(hex(k) for k in s.slc_get_active_keys())}")
    click.echo(f"Revoked keys: {', '.join(hex(k) for k in s.slc_get_revoked_keys())}")


@slc.command("configure")
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@pb_session
@click.pass_context
def slc_configure(ctx: click.Context, s: Session, force: bool):
    """Set SLC configure."""
    if force or click.confirm(_slc_warning):
        s.slc_set_configuration()


@slc.command("lock")
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@pb_session
@click.pass_context
def slc_lock(ctx: click.Context, s: Session, force: bool):
    """Set SLC configuration lock."""
    if force or click.confirm(_slc_warning):
        s.slc_set_configuration_lock()


@slc.command("eol")
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@pb_session
@click.pass_context
def slc_eol(ctx: click.Context, s: Session, force: bool):
    """Set SLC end of life."""
    if force or click.confirm(_slc_warning):
        s.slc_set_end_of_life()


@slc.command("revoke-key")
@click.argument("key-id", type=str)
@click.option(
    "force", "--force", is_flag=True, default=False, help="Force operation without confirmation."
)
@pb_session
@click.pass_context
def slc_revoke_key(ctx: click.Context, s: Session, key_id: str, force: bool):
    """Revoke a key."""
    _key_id: Union[int, str] = 0
    try:
        _key_id = int(key_id, 0)
    except ValueError:
        _key_id = key_id

    if force or click.confirm(_slc_warning):
        s.slc_revoke_key(_key_id)


@cli.command("completion")
@click.pass_context
def shell_completion_helper(ctx: click.Context):
    """Shell completion helpers."""
    _help_text = """
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

    click.echo(_help_text)


@cli.command("list")
def list_devices():
    """List Punchboot devices attached over USB."""
    for _uu in list_usb_devices():
        click.echo(f"{_uu} -- {_get_board_name(_uu)}")


if __name__ == "__main__":
    cli()
