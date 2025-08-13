#!/usr/bin/env python3
"""
append_bins_inplace.py
Append the contents of one binary file onto another *in place*.

Usage:
    python append_bins_inplace.py primary.bin to_append.bin
"""

import argparse
import os
import sys

CHUNK_SIZE = 64 * 1024  # 64 KiB – keeps memory usage low for large files


def append_bin_files_inplace(primary_path: str, append_path: str) -> None:
    """Append `append_path` to `primary_path` without creating a new file."""
    # Basic sanity checks
    if not os.path.isfile(primary_path):
        sys.exit(f"Error: primary file '{primary_path}' does not exist.")
    if not os.path.isfile(append_path):
        sys.exit(f"Error: append file '{append_path}' does not exist.")

    try:
        with open(primary_path, "ab") as primary, open(append_path, "rb") as extra:
            while chunk := extra.read(CHUNK_SIZE):
                primary.write(chunk)

        print(
            f"✓ Successfully appended '{append_path}' "
            f"({os.path.getsize(append_path)} bytes) to '{primary_path}'."
        )
    except OSError as e:
        sys.exit(f"I/O error: {e}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Append one binary file to another in place."
    )
    parser.add_argument(
        "primary",
        help="Path to the primary binary file (it will be modified in place).",
    )
    parser.add_argument(
        "append",
        help="Path to the binary file whose contents will be appended.",
    )

    args = parser.parse_args()
    append_bin_files_inplace(args.primary, args.append)


if __name__ == "__main__":
    main()
