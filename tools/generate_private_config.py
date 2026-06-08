#!/usr/bin/env python3
# Copyright 2026 clesiorki2018
# SPDX-License-Identifier: Apache-2.0
"""Generate private build configuration from the local .env file."""

from __future__ import annotations

import argparse
from pathlib import Path


def parse_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}

    if not path.exists():
        return values

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue

        key, value = line.split("=", 1)
        values[key.strip()] = value.strip().strip('"').strip("'")

    return values


def env_bool(values: dict[str, str], key: str, default: bool = False) -> bool:
    raw_value = values.get(key)
    if raw_value is None or raw_value == "":
        return default

    return raw_value.lower() in {"1", "true", "yes", "on", "enabled"}


def env_int(values: dict[str, str], key: str, default: int) -> int:
    raw_value = values.get(key)
    if raw_value is None or raw_value == "":
        return default

    return int(raw_value, 0)


def c_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def hex_bytes(value: str, expected_len: int, label: str) -> list[int]:
    if value == "":
        return [0] * expected_len

    if len(value) != expected_len * 2:
        raise ValueError(f"{label} must have {expected_len * 2} hex characters")

    try:
        return [int(value[index : index + 2], 16) for index in range(0, len(value), 2)]
    except ValueError as exc:
        raise ValueError(f"{label} must contain only hexadecimal characters") from exc


def mac_bytes(value: str, label: str) -> tuple[bool, list[int]]:
    if value == "":
        return False, [0] * 6

    parts = value.split(":")
    if len(parts) != 6:
        raise ValueError(f"{label} must use AA:BB:CC:DD:EE:FF format")

    try:
        return True, [int(part, 16) for part in parts]
    except ValueError as exc:
        raise ValueError(f"{label} must contain only hexadecimal bytes") from exc


def c_bytes(values: list[int]) -> str:
    return ", ".join(f"0x{value:02x}" for value in values)


def first_value(values: dict[str, str], *keys: str, default: str = "") -> str:
    for key in keys:
        value = values.get(key)
        if value is not None and value != "":
            return value

    return default


def generate_header(values: dict[str, str]) -> str:
    has_env = bool(values)
    has_receiver_mac, receiver_mac = mac_bytes(
        first_value(values, "HEAD_EMITTER_RECEIVER_MAC", "HEAD_CLICK_RECEIVER_WIFI_STA_MAC"),
        "HEAD_EMITTER_RECEIVER_MAC",
    )
    pmk = hex_bytes(
        first_value(values, "ESP_NOW_PMK_HEX", "HEAD_CLICK_ESP_NOW_PMK_HEX"),
        16,
        "ESP_NOW_PMK_HEX",
    )
    lmk = hex_bytes(
        first_value(values, "ESP_NOW_LMK_HEX", "HEAD_CLICK_SENDER_COMBO_LMK_HEX"),
        16,
        "ESP_NOW_LMK_HEX",
    )
    auth_key = hex_bytes(
        first_value(values, "APP_AUTH_KEY_HEX", "HEAD_CLICK_APP_AUTH_KEY_HEX"),
        32,
        "APP_AUTH_KEY_HEX",
    )
    wifi_channel = int(first_value(values, "ESP_NOW_WIFI_CHANNEL", "HEAD_CLICK_ESP_NOW_WIFI_CHANNEL", default="6"), 0)
    encryption_enabled = env_bool(
        {"value": first_value(values, "ESP_NOW_ENCRYPTION_ENABLED", "HEAD_CLICK_ESP_NOW_ENCRYPTION_ENABLED", default="1")},
        "value",
        True,
    )

    return "\n".join(
        [
            "/* Generated file. Do not edit or commit. */",
            "#pragma once",
            "",
            f"#define HEAD_EMITTER_CONFIG_HAS_ENV {1 if has_env else 0}",
            f"#define HEAD_EMITTER_RECEIVER_HAS_MAC {1 if has_receiver_mac else 0}",
            f"#define HEAD_EMITTER_RECEIVER_MAC_BYTES {c_bytes(receiver_mac)}",
            f"#define HEAD_EMITTER_ESP_NOW_WIFI_CHANNEL {wifi_channel}",
            f"#define HEAD_EMITTER_ESP_NOW_ENCRYPTION_ENABLED {1 if encryption_enabled else 0}",
            f"#define HEAD_EMITTER_ESP_NOW_PMK_BYTES {c_bytes(pmk)}",
            f"#define HEAD_EMITTER_ESP_NOW_LMK_BYTES {c_bytes(lmk)}",
            f"#define HEAD_EMITTER_APP_AUTH_KEY_BYTES {c_bytes(auth_key)}",
            f"#define HEAD_EMITTER_APP_REPLAY_PROTECTION_ENABLED {1 if env_bool(values, 'HEAD_CLICK_APP_REPLAY_PROTECTION_ENABLED', True) else 0}",
            f"#define HEAD_EMITTER_APP_SEQUENCE_WINDOW {env_int(values, 'HEAD_CLICK_APP_SEQUENCE_WINDOW', 32)}",
            f"#define HEAD_EMITTER_APP_SEQUENCE_NAMESPACE {c_string(first_value(values, 'APP_SEQUENCE_NAMESPACE', default='head_emitter'))}",
            f"#define HEAD_EMITTER_APP_SEQUENCE_KEY {c_string(first_value(values, 'APP_SEQUENCE_KEY', default='combo_seq'))}",
            f"#define HEAD_EMITTER_SENDER_NAME {c_string(values.get('HEAD_CLICK_SENDER_COMBO_NAME', 'combo'))}",
            f"#define HEAD_EMITTER_SENDER_ENABLED {1 if env_bool(values, 'HEAD_CLICK_SENDER_COMBO_ENABLED', True) else 0}",
            f"#define HEAD_EMITTER_SENDER_CAPABILITIES {c_string(values.get('HEAD_CLICK_SENDER_COMBO_CAPABILITIES', 'mouse,keyboard,joystick'))}",
            "",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--env-file", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    header = generate_header(parse_env(args.env_file))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(header + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
