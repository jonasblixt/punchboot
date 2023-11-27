"""Security Life Cycle."""

from enum import IntEnum


class SLC(IntEnum):
    """Security Life Cycle.

    Fields:
    SLC_NOT_CONFIGURED -- No security related configuration has been written
    SLC_NOT_CONFIGURED -- Device is in configured state
    SLC_CONFIGURATION_LOCKED -- Security configuration has been lock, no further
        changes are permitted.
    SLC_EOL -- Device is in EOL, End of Life state.
    """

    SLC_INVALID = (0,)
    SLC_NOT_CONFIGURED = (1,)
    SLC_CONFIGURATION = (2,)
    SLC_CONFIGURATION_LOCKED = (3,)
    SLC_EOL = (4,)
