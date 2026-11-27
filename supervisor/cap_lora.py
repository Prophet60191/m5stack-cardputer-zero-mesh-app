#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Cap LoRa-1262 power and RF-switch bring-up for the CardputerZero.

This is the lowest layer of our app toolkit: it turns the Cap's power on the
*right way* and leaves it in a state where the SX1262 radio can be driven.
`meshtasticd` (the Meshtastic daemon) talks to the SX1262 over SPI, but it does
NOT power the Cap or drive the Cap's antenna/band switch -- that is our job, and
it must happen *before* the daemon opens the radio and *stay done* while it runs.

Why a separate, careful module? The pocket-mesh beta got this wrong and never
powered the radio. MeshTerm (Apache-2.0) got it right; this reimplements the
sequence from the hardware facts we measured ourselves (see
../../docs/software/cap-radio-bringup.md). Linux-only: it uses /sys and /dev,
so it runs ON the device, not on the Mac.

----------------------------------------------------------------------------
The hardware, in plain terms
----------------------------------------------------------------------------
* The Cap's 5 V rail is switched by a kernel "LED" device. Writing "1" to
  /sys/class/leds/ext_5v_out/brightness turns the Cap on; "0" turns it off.
  (The kernel models lots of on/off lines as "LEDs" -- it is just a switch.)
* The EXT header can be in USB mode or GPIO mode. The radio needs GPIO mode:
  /sys/class/leds/ext_usb_gpio_fun/brightness MUST be 0. At 1 the Cap's reset
  line is cut and the chip sits in reset forever.
* The antenna/band path is gated by a small I2C chip on the Cap: a
  PI4IOE5V6408 I/O expander at address 0x43 on bus /dev/i2c-1. It only answers
  once the 5 V rail is on, which is exactly why an earlier "it's not there"
  reading (taken with the Cap unpowered) was wrong. We confirmed it IS there:
  ID register 0x01 reads 0xA2 (its top three bits, 0b101, are the maker code).
"""

from __future__ import annotations

import contextlib
import errno
import fcntl
import os
import struct
import time


# --- Wiring facts for THIS board + Cap (measured; see the hardware sheet). ----
# Keeping them as named constants here is the start of the "board facts as data"
# seam the toolkit is built around: logic never hard-codes a pin inline.
POWER_LED = "/sys/class/leds/ext_5v_out/brightness"        # Cap 5 V rail switch
MUX_LED = "/sys/class/leds/ext_usb_gpio_fun/brightness"    # EXT selector (0=GPIO)
I2C_BUS = "/dev/i2c-1"                                      # bus the expander is on
SPI_DEV = "/dev/spidev0.1"                                  # the SX1262 (kernel CS1)
GPIO_CHIP = "/dev/gpiochip0"                                # GPIO lines for the radio

EXPANDER_ADDR = 0x43        # PI4IOE5V6408 I/O expander (the RF/band switch)
EXPANDER_RF_BIT = 0         # P0 is the line that enables the RF path
POWER_RECOVERY_OFF_S = 1.0       # bounded recovery of an unresponsive Cap
POWER_RECOVERY_SETTLE_S = 1.0
POWER_SETTLE_S = 0.10       # let the regulator + expander wake after power-on

# PI4IOE5V6408 register addresses (from its datasheet):
REG_CHIP_ID = 0x01          # top 3 bits = maker code 0b101 on a real part
REG_DIRECTION = 0x03        # 1 = pin is an output
REG_OUTPUT = 0x05           # value driven when a pin is an output
REG_HIGH_Z = 0x07           # 1 = pin is high-impedance (floating/disabled)

# The Linux ioctl number that says "I want to talk to THIS I2C address".
I2C_SLAVE = 0x0703


class CapError(Exception):
    """A bring-up failure with a machine-usable `kind` and a human message.

    `kind` lets a caller react (e.g. show a specific hint); the message is what
    we would print to a person. This "classify, don't just crash" habit is the
    single biggest difference between MeshTerm (robust) and pocket-mesh (brittle).
    """

    def __init__(self, kind: str, message: str):
        super().__init__(message)
        self.kind = kind


def switch_value(value: str) -> str:
    """Normalize the vendor LED driver's logical GPIO state.

    Stock tools can leave brightness=255 although max_brightness reports 1.
    These are binary switches: any valid 8-bit nonzero value means enabled.
    Keep recovery records canonical and reject malformed/out-of-range data.
    """
    if not isinstance(value, str) or not value or any(c not in "0123456789" for c in value) or len(value) > 3:
        raise ValueError("invalid power switch state")
    number = int(value)
    if number > 255: raise ValueError("invalid power switch state")
    return "1" if number else "0"


# --- tiny sysfs helpers ------------------------------------------------------
def _read_text(path: str) -> str:
    with open(path) as f:
        return f.read().strip()


def _write_text(path: str, value: str) -> None:
    # Opening in "w" and writing the number is how you set a sysfs switch.
    with open(path, "w") as f:
        f.write(value)


# --- tiny I2C helpers --------------------------------------------------------
# I2C has no file-per-register API; you open the bus, point it at a chip address
# with an ioctl, then write the register number and read/write its bytes.
def _i2c_read_register(fd: int, reg: int) -> int:
    os.write(fd, bytes([reg]))     # "I want register <reg>"
    value = os.read(fd, 1)
    if len(value) != 1:
        raise OSError(errno.EIO, "short Cap I2C register read")
    return value[0]


def _i2c_write_register(fd: int, reg: int, value: int) -> None:
    os.write(fd, bytes([reg, value & 0xFF]))


def preflight() -> None:
    """Fail early, with a clear reason, if the kernel interfaces we need are
    missing or we lack permission -- before we power anything on."""
    for path, kind in ((SPI_DEV, "no-spi"), (GPIO_CHIP, "no-gpio"), (I2C_BUS, "no-i2c")):
        if not os.path.exists(path):
            raise CapError(kind, f"{path} is missing -- is the Cap seated and the "
                                 f"overlay loaded?")
        if not os.access(path, os.R_OK | os.W_OK):
            raise CapError("permission",
                           f"no read/write access to {path}. Add your user to the "
                           f"spi, gpio and i2c groups and log in again "
                           f"(sudo usermod -aG spi,gpio,i2c $USER).")
    claimed = gpio_claims()
    if claimed:
        line = next(iter(claimed))
        hint = ("NFC SPI-CS2 overlay reserves GPIO22; reboot after using NFC before starting LoRa"
                if line == 22 and claimed[line] == "backing" else
                f"GPIO{line} is already claimed; close the other hardware app before starting LoRa")
        raise CapError("gpio-busy", hint)


def gpio_claims() -> dict:
    """Read-only GPIO character ABI v1 query on the qualified Debian image.

    Never unbind an unrelated kernel driver or remove another app's overlay.
    The hardware guardian repeats preflight after obtaining its own lease.
    """
    claimed = {}
    fd = os.open(GPIO_CHIP, os.O_RDONLY | os.O_CLOEXEC)
    try:
        for offset in (22, 23, 26):
            info = bytearray(72)  # struct gpioline_info: offset, flags, name, consumer
            struct.pack_into("I", info, 0, offset)
            fcntl.ioctl(fd, 0xC048B402, info, True)  # GPIO_GET_LINEINFO_IOCTL, no mutation
            if struct.unpack_from("I", info, 4)[0] & 1:  # GPIOLINE_FLAG_KERNEL
                claimed[offset] = bytes(info[40:72]).split(b"\0", 1)[0].decode("utf-8", "replace")
    except OSError as exc:
        raise CapError("gpio-check", "cannot verify GPIO availability on this kernel; keep radio off") from exc
    finally:
        os.close(fd)
    return claimed


def verify_expander(fd: int) -> int:
    """Read the expander's ID register and confirm it is a real PI4IOE5V6408.
    Returns the raw ID byte. Raises CapError('absent'/'wrong-chip') otherwise."""
    try:
        chip_id = _i2c_read_register(fd, REG_CHIP_ID)
    except OSError as exc:
        if exc.errno in (errno.EACCES, errno.EPERM):
            raise CapError("permission", "no access to the Cap I2C control chip") from exc
        if exc.errno == errno.EBUSY:
            raise CapError("busy", f"{EXPANDER_ADDR:#04x} on {I2C_BUS} is held by a "
                                   f"kernel driver.") from exc
        raise CapError("absent", f"nothing answered at {EXPANDER_ADDR:#04x} on "
                                 f"{I2C_BUS} (is the Cap powered and seated?).") from exc
    maker_bits = (chip_id >> 5) & 0b111
    if maker_bits != 0b101:
        raise CapError("wrong-chip", f"chip at {EXPANDER_ADDR:#04x} is not a "
                                     f"PI4IOE5V6408 (ID {chip_id:#04x}).")
    return chip_id


def enable_rf_path(fd: int) -> None:
    """Drive the expander's P0 high to enable the Cap's RF path.

    Order matters: set the OUTPUT value *before* switching the pin to an output,
    so it comes up already-high with no brief low glitch on the RF-enable line.
    We use read-modify-write so we only touch bit P0 and leave the rest alone."""
    mask = 1 << EXPANDER_RF_BIT

    out = _i2c_read_register(fd, REG_OUTPUT)
    _i2c_write_register(fd, REG_OUTPUT, out | mask)          # P0 output value = high

    direction = _i2c_read_register(fd, REG_DIRECTION)
    _i2c_write_register(fd, REG_DIRECTION, direction | mask)  # P0 = output

    high_z = _i2c_read_register(fd, REG_HIGH_Z)
    _i2c_write_register(fd, REG_HIGH_Z, high_z & ~mask)       # P0 actively driven


@contextlib.contextmanager
def powered_cap(*, drive_rf: bool = True):
    """Context manager: power the Cap and enable the RF path, then restore switches
    when the context exits, including ordinary exceptions. SIGKILL cannot run
    Python finally blocks; the supervisor uses a separate guardian and service
    cleanup for that case.

        with powered_cap():
            ...run meshtasticd here, Cap stays powered...
        # Cap is powered off again here, originals restored.

    `yield`s the expander's ID byte (handy for logging/verification).
    """
    preflight()

    # Save the switches exactly as we found them, so we leave no trace.
    power_before = switch_value(_read_text(POWER_LED))
    mux_before = switch_value(_read_text(MUX_LED))

    powered = False
    try:
        try:
            _write_text(POWER_LED, "1")               # Cap 5 V on
            powered = True
            if mux_before != "0":
                _write_text(MUX_LED, "0")             # EXT header -> GPIO mode
        except PermissionError as exc:
            raise CapError("permission",
                           f"cannot write {POWER_LED} / {MUX_LED} -- your user needs "
                           f"the gpio group.") from exc

        time.sleep(POWER_SETTLE_S)                     # let the rail + expander wake

        chip_id = None
        if drive_rf:
            # Recover once, before any RF configuration write, if the control
            # chip does not answer. The guardian owns the hardware lease and
            # recovery record; no other owner's GPIO/overlay is taken over.
            for attempt in range(2):
                fd = os.open(I2C_BUS, os.O_RDWR | os.O_CLOEXEC)
                try:
                    fcntl.ioctl(fd, I2C_SLAVE, EXPANDER_ADDR)
                    try:
                        chip_id = verify_expander(fd)
                    except CapError as exc:
                        if exc.kind != "absent" or attempt:
                            raise
                    else:
                        enable_rf_path(fd)
                        break
                finally:
                    os.close(fd)
                _write_text(POWER_LED, "0")
                time.sleep(POWER_RECOVERY_OFF_S)
                _write_text(POWER_LED, "1")
                time.sleep(POWER_RECOVERY_SETTLE_S)

        yield chip_id
    finally:
        # Reverse order: EXT selector back first, then power off.
        with contextlib.suppress(OSError):
            if mux_before != "0":
                _write_text(MUX_LED, mux_before)
        if powered:
            with contextlib.suppress(OSError):
                _write_text(POWER_LED, power_before)


def _main(argv: list[str]) -> int:
    """`python3 cap_lora.py --probe` powers the Cap, confirms the expander, prints
    what it saw, and powers back down. It does NOT touch the SX1262 or transmit,
    so it is safe to run any time the antenna is attached."""
    import argparse

    parser = argparse.ArgumentParser(description="CardputerZero Cap LoRa-1262 power/RF bring-up")
    parser.add_argument("--probe", action="store_true",
                        help="power the Cap, verify the 0x43 expander, then power down")
    parser.add_argument("--hold", type=float, default=0.0, metavar="SECONDS",
                        help="keep the Cap powered this long before powering down")
    args = parser.parse_args(argv)

    if not (args.probe or args.hold):
        parser.print_help()
        return 2

    try:
        with powered_cap() as chip_id:
            print(f"Cap powered. RF-switch expander at {EXPANDER_ADDR:#04x}: "
                  f"ID {chip_id:#04x} (PI4IOE5V6408 confirmed). RF path enabled.")
            if args.hold:
                print(f"holding power for {args.hold:g}s ...")
                time.sleep(args.hold)
    except CapError as exc:
        print(f"bring-up failed [{exc.kind}]: {exc}")
        return 1
    print("Cap powered down, switches restored.")
    return 0


if __name__ == "__main__":
    import sys

    raise SystemExit(_main(sys.argv[1:]))
