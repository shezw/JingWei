#!/usr/bin/env python3
# JingWei
# tools test rfb-wpe-e2e.py    2026-07-18
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

"""Exercise the WPE browser through its host-published RFB endpoint."""

from __future__ import annotations

import argparse
import hashlib
import math
import socket
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path


RFB_HOST = "127.0.0.1"
RFB_PORT = 5900
RFB_SERVER_VERSION = b"RFB 003.008\n"
RFB_VERSION_38 = b"RFB 003.008\n"
RFB_VERSION_33 = b"RFB 003.003\n"
EXPECTED_WIDTH = 1280
EXPECTED_HEIGHT = 720
MAX_SERVER_NAME_BYTES = 64 * 1024
MAX_SERVER_TEXT_BYTES = 1024 * 1024
MIN_STATE_CHANGE_PIXELS = 100_000

DES_INITIAL_PERMUTATION = (
    58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17, 9, 1, 59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7,
)
DES_FINAL_PERMUTATION = (
    40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9, 49, 17, 57, 25,
)
DES_EXPANSION = (
    32, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9, 8, 9, 10, 11,
    12, 13, 12, 13, 14, 15, 16, 17, 16, 17, 18, 19, 20, 21, 20, 21,
    22, 23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1,
)
DES_P_PERMUTATION = (
    16, 7, 20, 21, 29, 12, 28, 17, 1, 15, 23, 26, 5, 18, 31, 10,
    2, 8, 24, 14, 32, 27, 3, 9, 19, 13, 30, 6, 22, 11, 4, 25,
)
DES_PC1 = (
    57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18,
    10, 2, 59, 51, 43, 35, 27, 19, 11, 3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15, 7, 62, 54, 46, 38, 30, 22,
    14, 6, 61, 53, 45, 37, 29, 21, 13, 5, 28, 20, 12, 4,
)
DES_PC2 = (
    14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10,
    23, 19, 12, 4, 26, 8, 16, 7, 27, 20, 13, 2,
    41, 52, 31, 37, 47, 55, 30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32,
)
DES_KEY_SHIFTS = (1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1)
DES_SBOXES = (
    (
        (14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7),
        (0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8),
        (4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0),
        (15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13),
    ),
    (
        (15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10),
        (3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5),
        (0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15),
        (13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9),
    ),
    (
        (10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8),
        (13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1),
        (13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7),
        (1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12),
    ),
    (
        (7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15),
        (13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9),
        (10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4),
        (3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14),
    ),
    (
        (2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9),
        (14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6),
        (4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14),
        (11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3),
    ),
    (
        (12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11),
        (10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8),
        (9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6),
        (4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13),
    ),
    (
        (4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1),
        (13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6),
        (1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2),
        (6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12),
    ),
    (
        (13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7),
        (1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2),
        (7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8),
        (2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11),
    ),
)

BUTTON_POINT = (260, 155)
INPUT_POINT = (840, 155)
CLICK_STATE_POINT = (1100, 380)
TEXT_STATE_POINT = (1100, 600)

INITIAL_CLICK_COLOR = (194, 65, 12)
CLICKED_COLOR = (21, 128, 61)
INITIAL_TEXT_COLOR = (29, 78, 216)
TYPED_COLOR = (126, 34, 206)


class RfbError(RuntimeError):
    """Raised when the server violates the expected RFB smoke contract."""


class RfbAuthenticationError(RfbError):
    """Raised when VNC Authentication rejects a challenge response."""


def des_permute(value: int, table: tuple[int, ...], input_bits: int) -> int:
    result = 0
    for position in table:
        result = (result << 1) | ((value >> (input_bits - position)) & 1)
    return result


def des_subkeys(key: bytes) -> tuple[int, ...]:
    key_value = int.from_bytes(key, "big")
    permuted = des_permute(key_value, DES_PC1, 64)
    left = (permuted >> 28) & 0x0FFFFFFF
    right = permuted & 0x0FFFFFFF
    result: list[int] = []
    for shift in DES_KEY_SHIFTS:
        left = ((left << shift) | (left >> (28 - shift))) & 0x0FFFFFFF
        right = ((right << shift) | (right >> (28 - shift))) & 0x0FFFFFFF
        result.append(des_permute((left << 28) | right, DES_PC2, 56))
    return tuple(result)


def des_encrypt_block(block: bytes, subkeys: tuple[int, ...]) -> bytes:
    permuted = des_permute(int.from_bytes(block, "big"), DES_INITIAL_PERMUTATION, 64)
    left = (permuted >> 32) & 0xFFFFFFFF
    right = permuted & 0xFFFFFFFF
    for subkey in subkeys:
        expanded = des_permute(right, DES_EXPANSION, 32) ^ subkey
        substituted = 0
        for index, sbox in enumerate(DES_SBOXES):
            chunk = (expanded >> (42 - index * 6)) & 0x3F
            row = ((chunk & 0x20) >> 4) | (chunk & 1)
            column = (chunk >> 1) & 0x0F
            substituted = (substituted << 4) | sbox[row][column]
        transformed = des_permute(substituted, DES_P_PERMUTATION, 32)
        left, right = right, left ^ transformed
    encrypted = des_permute((right << 32) | left, DES_FINAL_PERMUTATION, 64)
    return encrypted.to_bytes(8, "big")


def verify_des_implementation() -> None:
    key = bytes.fromhex("133457799BBCDFF1")
    plaintext = bytes.fromhex("0123456789ABCDEF")
    expected = bytes.fromhex("85E813540F0AB405")
    if des_encrypt_block(plaintext, des_subkeys(key)) != expected:
        raise RfbError("DES implementation failed its known-answer test")


def vnc_auth_response(challenge: bytes, password: bytes) -> bytes:
    if len(challenge) != 16:
        raise RfbError("VNC Authentication challenge must be 16 bytes")
    if not 1 <= len(password) <= 8 or any(
        value < 0x21 or value > 0x7E for value in password
    ):
        raise RfbError("VNC password file must contain 1..8 printable ASCII bytes")
    reverse_bits = bytes(int(f"{value:08b}"[::-1], 2) for value in password)
    key = reverse_bits.ljust(8, b"\x00")
    subkeys = des_subkeys(key)
    return b"".join(
        des_encrypt_block(challenge[offset : offset + 8], subkeys)
        for offset in (0, 8)
    )


def read_password_file(path: Path) -> bytes:
    try:
        password = path.read_bytes()
    except OSError as error:
        raise RfbError("unable to read VNC password file") from error
    if password.endswith(b"\n"):
        password = password[:-1]
        if password.endswith(b"\r"):
            password = password[:-1]
    if not 1 <= len(password) <= 8 or any(
        value < 0x21 or value > 0x7E for value in password
    ):
        raise RfbError("VNC password file must contain 1..8 printable ASCII bytes")
    return password


@dataclass(frozen=True)
class Frame:
    """A framebuffer in the requested little-endian BGRX wire format."""

    width: int
    height: int
    pixels: bytes

    def sha256(self) -> str:
        return hashlib.sha256(self.pixels).hexdigest()

    def rgb_at(self, x: int, y: int) -> tuple[int, int, int]:
        if not (0 <= x < self.width and 0 <= y < self.height):
            raise RfbError(f"pixel coordinate is outside the frame: ({x}, {y})")
        offset = (y * self.width + x) * 4
        blue, green, red = self.pixels[offset : offset + 3]
        return red, green, blue

    def write_ppm(self, path: Path) -> None:
        rgb = bytearray(self.width * self.height * 3)
        rgb[0::3] = self.pixels[2::4]
        rgb[1::3] = self.pixels[1::4]
        rgb[2::3] = self.pixels[0::4]
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("wb") as output:
            output.write(f"P6\n{self.width} {self.height}\n255\n".encode("ascii"))
            output.write(rgb)


class RfbClient:
    """Minimal authenticated RFB client supporting 3.3, 3.8, and RAW frames."""

    def __init__(
        self,
        host: str,
        port: int,
        timeout: float,
        password: bytes,
        client_version: bytes = RFB_VERSION_38,
    ) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
        self.password = password
        self.client_version = client_version
        self.socket: socket.socket | None = None
        self.width = 0
        self.height = 0
        self.desktop_name = ""

    def __enter__(self) -> "RfbClient":
        try:
            self.socket = socket.create_connection(
                (self.host, self.port), timeout=self.timeout
            )
            self.socket.settimeout(self.timeout)
            self._handshake()
            self._set_raw_little_endian_pixel_format()
            self._set_raw_encoding()
            return self
        except Exception:
            self.close()
            raise

    def __exit__(self, _type: object, _value: object, _traceback: object) -> None:
        self.close()

    def close(self) -> None:
        if self.socket is not None:
            self.socket.close()
            self.socket = None

    def _operation_deadline(self) -> float:
        return time.monotonic() + self.timeout

    def _require_socket(self) -> socket.socket:
        if self.socket is None:
            raise RfbError("RFB socket is not connected")
        return self.socket

    def _receive_exact(
        self, size: int, context: str, deadline: float | None = None
    ) -> bytes:
        operation_deadline = (
            deadline if deadline is not None else self._operation_deadline()
        )
        connection = self._require_socket()
        result = bytearray(size)
        view = memoryview(result)
        received = 0
        while received < size:
            remaining = operation_deadline - time.monotonic()
            if remaining <= 0:
                raise RfbError(f"timed out while reading {context}")
            connection.settimeout(remaining)
            try:
                count = connection.recv_into(view[received:], size - received)
            except socket.timeout as error:
                raise RfbError(f"timed out while reading {context}") from error
            if count == 0:
                raise RfbError(f"RFB server closed the connection while reading {context}")
            received += count
        return bytes(result)

    def _send(
        self, message: bytes, context: str, deadline: float | None = None
    ) -> None:
        operation_deadline = (
            deadline if deadline is not None else self._operation_deadline()
        )
        connection = self._require_socket()
        view = memoryview(message)
        sent = 0
        while sent < len(message):
            remaining = operation_deadline - time.monotonic()
            if remaining <= 0:
                raise RfbError(f"timed out while sending {context}")
            connection.settimeout(remaining)
            try:
                count = connection.send(view[sent:])
            except socket.timeout as error:
                raise RfbError(f"timed out while sending {context}") from error
            if count == 0:
                raise RfbError(f"RFB server closed the connection while sending {context}")
            sent += count

    def _read_reason(self, context: str, deadline: float) -> str:
        length = struct.unpack("!I", self._receive_exact(4, context, deadline))[0]
        if length > MAX_SERVER_TEXT_BYTES:
            raise RfbError(f"RFB {context} is too long: {length} bytes")
        return self._receive_exact(length, context, deadline).decode(
            "utf-8", errors="replace"
        )

    def _handshake(self) -> None:
        deadline = self._operation_deadline()
        server_version = self._receive_exact(12, "protocol version", deadline)
        if server_version != RFB_SERVER_VERSION:
            raise RfbError(
                f"expected RFB 3.8, received {server_version!r}"
            )
        if self.client_version not in (RFB_VERSION_33, RFB_VERSION_38):
            raise RfbError("RFB client version must be 3.3 or 3.8")
        self._send(self.client_version, "protocol version", deadline)

        if self.client_version == RFB_VERSION_33:
            security_type = struct.unpack(
                "!I", self._receive_exact(4, "security type", deadline)
            )[0]
            if security_type == 0:
                reason = self._read_reason("security failure reason", deadline)
                raise RfbError(
                    f"RFB server rejected security negotiation: {reason}"
                )
            if security_type != 2:
                raise RfbError(
                    "RFB 3.3 server must require VNC Authentication; "
                    f"received security type {security_type}"
                )
        else:
            security_count = self._receive_exact(
                1, "security type count", deadline
            )[0]
            if security_count == 0:
                reason = self._read_reason("security failure reason", deadline)
                raise RfbError(
                    f"RFB server rejected security negotiation: {reason}"
                )
            security_types = self._receive_exact(
                security_count, "security types", deadline
            )
            if 2 not in security_types or 1 in security_types:
                raise RfbError(
                    "RFB server must offer VNC Authentication without None: "
                    f"{list(security_types)}"
                )
            self._send(b"\x02", "VNC Authentication selection", deadline)
        challenge = self._receive_exact(16, "VNC Authentication challenge", deadline)
        self._send(
            vnc_auth_response(challenge, self.password),
            "VNC Authentication response",
            deadline,
        )

        security_status = struct.unpack(
            "!I", self._receive_exact(4, "security result", deadline)
        )[0]
        if security_status != 0:
            reason = "authentication rejected"
            if self.client_version == RFB_VERSION_38:
                reason = self._read_reason("security result reason", deadline)
            raise RfbAuthenticationError(
                f"RFB VNC Authentication failed ({security_status}): {reason}"
            )

        self._send(b"\x01", "shared ClientInit", deadline)
        server_init = self._receive_exact(24, "ServerInit", deadline)
        self.width, self.height = struct.unpack("!HH", server_init[:4])
        name_length = struct.unpack("!I", server_init[20:24])[0]
        if name_length > MAX_SERVER_NAME_BYTES:
            raise RfbError(f"RFB desktop name is too long: {name_length} bytes")
        self.desktop_name = self._receive_exact(
            name_length, "desktop name", deadline
        ).decode("utf-8", errors="replace")
        if (self.width, self.height) != (EXPECTED_WIDTH, EXPECTED_HEIGHT):
            raise RfbError(
                "expected a 1280x720 framebuffer, received "
                f"{self.width}x{self.height}"
            )

    def _set_raw_little_endian_pixel_format(self) -> None:
        message = struct.pack(
            "!B3xBBBBHHHBBB3x",
            0,
            32,
            24,
            0,
            1,
            255,
            255,
            255,
            16,
            8,
            0,
        )
        self._send(message, "SetPixelFormat")

    def _set_raw_encoding(self) -> None:
        self._send(struct.pack("!BBHi", 2, 0, 1, 0), "SetEncodings RAW")

    def send_pointer(
        self,
        x: int,
        y: int,
        button_mask: int,
        deadline: float | None = None,
    ) -> None:
        self._send(
            struct.pack("!BBHH", 5, button_mask, x, y),
            f"pointer event at ({x}, {y})",
            deadline,
        )

    def click(self, x: int, y: int) -> None:
        deadline = self._operation_deadline()
        self.send_pointer(x, y, 0, deadline)
        self.send_pointer(x, y, 1, deadline)
        self.send_pointer(x, y, 0, deadline)

    def send_ascii(self, text: str) -> None:
        try:
            encoded = text.encode("ascii")
        except UnicodeEncodeError as error:
            raise RfbError("keyboard token must contain ASCII only") from error
        deadline = self._operation_deadline()
        for character in encoded:
            self._send(
                struct.pack("!BB2xI", 4, 1, character),
                "key down",
                deadline,
            )
            self._send(
                struct.pack("!BB2xI", 4, 0, character),
                "key up",
                deadline,
            )

    def capture(self, deadline: float | None = None) -> Frame:
        operation_deadline = (
            deadline if deadline is not None else self._operation_deadline()
        )
        request = struct.pack(
            "!BBHHHH", 3, 0, 0, 0, self.width, self.height
        )
        self._send(
            request,
            "non-incremental FramebufferUpdateRequest",
            operation_deadline,
        )

        while True:
            message_type = self._receive_exact(
                1, "server message type", operation_deadline
            )[0]
            if message_type == 0:
                return self._receive_framebuffer_update(operation_deadline)
            if message_type == 2:
                continue
            if message_type == 3:
                header = self._receive_exact(
                    7, "ServerCutText header", operation_deadline
                )
                length = struct.unpack("!I", header[3:7])[0]
                if length > MAX_SERVER_TEXT_BYTES:
                    raise RfbError(f"ServerCutText is too long: {length} bytes")
                self._receive_exact(length, "ServerCutText", operation_deadline)
                continue
            raise RfbError(f"unsupported RFB server message type: {message_type}")

    def _receive_framebuffer_update(self, deadline: float) -> Frame:
        update_header = self._receive_exact(
            3, "FramebufferUpdate header", deadline
        )
        rectangle_count = struct.unpack("!H", update_header[1:3])[0]
        if rectangle_count == 0:
            raise RfbError("FramebufferUpdate contained no rectangles")

        pixels = bytearray(self.width * self.height * 4)
        coverage = bytearray(self.width * self.height)
        for _index in range(rectangle_count):
            rectangle = self._receive_exact(12, "rectangle header", deadline)
            x, y, width, height, encoding = struct.unpack("!HHHHi", rectangle)
            if encoding != 0:
                raise RfbError(f"server returned non-RAW encoding: {encoding}")
            if width == 0 or height == 0:
                raise RfbError("server returned an empty RAW rectangle")
            if x + width > self.width or y + height > self.height:
                raise RfbError(
                    "RAW rectangle lies outside the framebuffer: "
                    f"x={x} y={y} width={width} height={height}"
                )
            rectangle_pixels = self._receive_exact(
                width * height * 4, "RAW rectangle pixels", deadline
            )
            for row in range(height):
                source_start = row * width * 4
                target_start = ((y + row) * self.width + x) * 4
                pixels[target_start : target_start + width * 4] = (
                    rectangle_pixels[source_start : source_start + width * 4]
                )
                coverage_start = (y + row) * self.width + x
                coverage[coverage_start : coverage_start + width] = b"\x01" * width

        if 0 in coverage:
            missing = coverage.count(0)
            raise RfbError(
                f"non-incremental update omitted {missing} framebuffer pixels"
            )
        return Frame(self.width, self.height, bytes(pixels))


def changed_pixel_count(before: Frame, after: Frame) -> int:
    if (before.width, before.height) != (after.width, after.height):
        raise RfbError("cannot compare frames with different dimensions")
    changed = 0
    for offset in range(0, len(before.pixels), 4):
        if (
            before.pixels[offset] != after.pixels[offset]
            or before.pixels[offset + 1] != after.pixels[offset + 1]
            or before.pixels[offset + 2] != after.pixels[offset + 2]
        ):
            changed += 1
    return changed


def wait_for_colors(
    client: RfbClient,
    expected: dict[tuple[int, int], tuple[int, int, int]],
    timeout: float,
    state_name: str,
) -> Frame:
    deadline = time.monotonic() + timeout
    last_colors: dict[tuple[int, int], tuple[int, int, int]] = {}
    while time.monotonic() < deadline:
        frame = client.capture(deadline)
        last_colors = {point: frame.rgb_at(*point) for point in expected}
        if all(last_colors[point] == color for point, color in expected.items()):
            return frame
        time.sleep(min(0.05, max(0.0, deadline - time.monotonic())))
    raise RfbError(
        f"timed out waiting for {state_name}; landmark colors were {last_colors}"
    )


def verify_transition(
    before: Frame, after: Frame, transition_name: str
) -> tuple[str, str, int]:
    before_hash = before.sha256()
    after_hash = after.sha256()
    changed = changed_pixel_count(before, after)
    if before_hash == after_hash:
        raise RfbError(f"{transition_name} did not change the framebuffer hash")
    if changed < MIN_STATE_CHANGE_PIXELS:
        raise RfbError(
            f"{transition_name} changed only {changed} pixels; "
            f"expected at least {MIN_STATE_CHANGE_PIXELS}"
        )
    return before_hash, after_hash, changed


def parse_arguments() -> argparse.Namespace:
    repository_dir = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(
        description="Drive the 1280x720 WPE fixture through host RFB and save PPM evidence."
    )
    parser.add_argument(
        "--password-file",
        type=Path,
        required=True,
        help="path to a 1..8 byte printable ASCII VNC password file",
    )
    parser.add_argument(
        "--token",
        required=True,
        help="exact ASCII token expected by the fixture",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=RFB_PORT,
        help="Mac loopback RFB port (default: 5900)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repository_dir / "test-results" / "rfb-wpe-e2e",
        help="directory for initial.ppm, clicked.ppm, and typed.ppm",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="hard timeout in seconds for each network/state operation",
    )
    arguments = parser.parse_args()
    if not math.isfinite(arguments.timeout) or arguments.timeout <= 0:
        parser.error("--timeout must be a finite positive number")
    if not 1 <= arguments.port <= 65535:
        parser.error("--port must be in 1..65535")
    if not 1 <= len(arguments.token) <= 64 \
        or not arguments.token.isascii() \
        or not arguments.token.isalnum():
        parser.error("--token must be 1..64 ASCII alphanumeric characters")
    return arguments


def wrong_password(password: bytes) -> bytes:
    replacement = ord("X") if password[-1] != ord("X") else ord("Y")
    return password[:-1] + bytes((replacement,))


def verify_wrong_password_rejected(
    timeout: float,
    port: int,
    password: bytes,
    client_version: bytes = RFB_VERSION_38,
) -> None:
    try:
        with RfbClient(
            RFB_HOST,
            port,
            timeout,
            wrong_password(password),
            client_version,
        ):
            pass
    except RfbAuthenticationError:
        return
    raise RfbError("RFB server accepted an incorrect VNC password")


def run(
    output_dir: Path,
    timeout: float,
    token: str,
    port: int,
    password: bytes,
) -> None:
    verify_wrong_password_rejected(
        timeout, port, password, RFB_VERSION_33
    )
    with RfbClient(
        RFB_HOST, port, timeout, password, RFB_VERSION_33
    ):
        pass
    print("macOS probe: RFB 3.3 Type 2 authentication succeeded.")

    verify_wrong_password_rejected(timeout, port, password)
    print("authentication: Type 2 required and incorrect password rejected.")
    with RfbClient(RFB_HOST, port, timeout, password) as client:
        initial = wait_for_colors(
            client,
            {
                CLICK_STATE_POINT: INITIAL_CLICK_COLOR,
                TEXT_STATE_POINT: INITIAL_TEXT_COLOR,
            },
            timeout,
            "initial fixture state",
        )
        initial.write_ppm(output_dir / "initial.ppm")

        client.click(*BUTTON_POINT)
        clicked = wait_for_colors(
            client,
            {
                CLICK_STATE_POINT: CLICKED_COLOR,
                TEXT_STATE_POINT: INITIAL_TEXT_COLOR,
            },
            timeout,
            "pointer-click fixture state",
        )
        clicked.write_ppm(output_dir / "clicked.ppm")
        initial_hash, clicked_hash, click_changed = verify_transition(
            initial, clicked, "pointer click"
        )

        client.click(*INPUT_POINT)
        time.sleep(0.1)
        client.send_ascii(token)
        typed = wait_for_colors(
            client,
            {
                CLICK_STATE_POINT: CLICKED_COLOR,
                TEXT_STATE_POINT: TYPED_COLOR,
            },
            timeout,
            "keyboard-input fixture state",
        )
        typed.write_ppm(output_dir / "typed.ppm")
        _, typed_hash, type_changed = verify_transition(
            clicked, typed, "keyboard input"
        )

    with RfbClient(RFB_HOST, port, timeout, password) as reconnect_client:
        reconnected = wait_for_colors(
            reconnect_client,
            {
                CLICK_STATE_POINT: CLICKED_COLOR,
                TEXT_STATE_POINT: TYPED_COLOR,
            },
            timeout,
            "reconnected fixture state",
        )
        reconnect_hash = reconnected.sha256()

    print(
        f"RFB 3.8 VNC Authentication: {client.desktop_name!r} "
        f"{client.width}x{client.height} RAW 32bpp little-endian"
    )
    print(
        f"pointer: {initial_hash} -> {clicked_hash}; "
        f"changed_pixels={click_changed} landmark="
        f"{INITIAL_CLICK_COLOR}->{CLICKED_COLOR}"
    )
    print(
        f"keyboard token={token!r}: {clicked_hash} -> {typed_hash}; "
        f"changed_pixels={type_changed} landmark="
        f"{INITIAL_TEXT_COLOR}->{TYPED_COLOR}"
    )
    print(
        "reconnect: second RFB handshake and full framebuffer succeeded; "
        f"hash={reconnect_hash}"
    )
    print(f"PPM evidence: {output_dir.resolve()}")
    print(
        "true: authenticated host RFB framebuffer, pointer input, "
        "keyboard input, and reconnect verified."
    )


def main() -> int:
    arguments = parse_arguments()
    try:
        verify_des_implementation()
        password = read_password_file(arguments.password_file)
        run(
            arguments.output_dir,
            arguments.timeout,
            arguments.token,
            arguments.port,
            password,
        )
    except (OSError, RfbError) as error:
        print(f"false: RFB WPE end-to-end test failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
