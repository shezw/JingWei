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
RFB_VERSION = b"RFB 003.008\n"
EXPECTED_WIDTH = 1280
EXPECTED_HEIGHT = 720
MAX_SERVER_NAME_BYTES = 64 * 1024
MAX_SERVER_TEXT_BYTES = 1024 * 1024
MIN_STATE_CHANGE_PIXELS = 100_000

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
    """Minimal RFB 3.8 client supporting None security and RAW frames."""

    def __init__(self, host: str, port: int, timeout: float) -> None:
        self.host = host
        self.port = port
        self.timeout = timeout
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
        if server_version != RFB_VERSION:
            raise RfbError(
                f"expected RFB 3.8, received {server_version!r}"
            )
        self._send(RFB_VERSION, "protocol version", deadline)

        security_count = self._receive_exact(1, "security type count", deadline)[0]
        if security_count == 0:
            reason = self._read_reason("security failure reason", deadline)
            raise RfbError(f"RFB server rejected security negotiation: {reason}")
        security_types = self._receive_exact(
            security_count, "security types", deadline
        )
        if 1 not in security_types:
            raise RfbError(
                f"RFB server does not offer None security: {list(security_types)}"
            )
        self._send(b"\x01", "None security selection", deadline)

        security_status = struct.unpack(
            "!I", self._receive_exact(4, "security result", deadline)
        )[0]
        if security_status != 0:
            reason = self._read_reason("security result reason", deadline)
            raise RfbError(f"RFB None security failed ({security_status}): {reason}")

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


def run(output_dir: Path, timeout: float, token: str, port: int) -> None:
    with RfbClient(RFB_HOST, port, timeout) as client:
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

    with RfbClient(RFB_HOST, port, timeout) as reconnect_client:
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
        f"RFB 3.8 None security: {client.desktop_name!r} "
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
    print("true: host RFB framebuffer, pointer input, and keyboard input verified.")


def main() -> int:
    arguments = parse_arguments()
    try:
        run(
            arguments.output_dir,
            arguments.timeout,
            arguments.token,
            arguments.port,
        )
    except (OSError, RfbError) as error:
        print(f"false: RFB WPE end-to-end test failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
