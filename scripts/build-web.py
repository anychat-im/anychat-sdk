#!/usr/bin/env python3
"""Build AnyChat SDK for WebAssembly using CMake Presets.

This script builds the native library for WebAssembly using Emscripten.

Requirements:
  - Emscripten SDK (set EMSDK environment variable)
  - CMake 3.20+
  - Ninja build system

Usage:
  python3 tools/build-web.py [options]

Options:
  --config {debug,release}  Build configuration (default: release)
  --clean                   Clean build directory before building
  --list-presets            List available Web presets
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def check_emscripten() -> None:
    """Check if Emscripten SDK is available."""
    emsdk = os.environ.get("EMSDK")
    if not emsdk:
        print("Error: EMSDK environment variable not set", file=sys.stderr)
        print("\nPlease set EMSDK to your Emscripten SDK installation path:", file=sys.stderr)
        print("  export EMSDK=/path/to/emsdk", file=sys.stderr)
        print("\nOr activate Emscripten SDK:", file=sys.stderr)
        print("  source /path/to/emsdk/emsdk_env.sh", file=sys.stderr)
        sys.exit(1)

    # Check if emcc is available
    try:
        subprocess.run(
            ["emcc", "--version"],
            check=True,
            capture_output=True
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Error: emcc not found. Please activate Emscripten SDK", file=sys.stderr)
        sys.exit(1)

    print(f"[build-web] Using Emscripten SDK: {emsdk}")


def list_presets() -> None:
    """List available WebAssembly CMake presets."""
    print("Available WebAssembly CMake Presets:")
    print("=" * 70)
    print("  web-debug                    - WebAssembly Debug")
    print("  web-release                  - WebAssembly Release")
    print()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build AnyChat SDK for WebAssembly using CMake Presets.",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--config",
        choices=["debug", "release"],
        default="release",
        help="Build configuration (default: release)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build directory before building",
    )
    parser.add_argument(
        "--list-presets",
        action="store_true",
        help="List available WebAssembly CMake presets and exit",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.list_presets:
        list_presets()
        return

    # Check Emscripten
    check_emscripten()

    preset = f"web-{args.config}"
    build_dir = ROOT / "build" / preset

    print(f"[build-web] Building for WebAssembly ({args.config})")
    print(f"[build-web] Preset: {preset}")
    print(f"[build-web] Build directory: {build_dir}")
    print()

    # Clean if requested
    if args.clean and build_dir.exists():
        print(f"[build-web] Cleaning: {build_dir}")
        shutil.rmtree(build_dir)

    # Configure
    print(f"[build-web] Configuring...")
    subprocess.run(
        ["cmake", "--preset", preset],
        cwd=ROOT,
        check=True
    )

    # Build
    print(f"[build-web] Building...")
    subprocess.run(
        ["cmake", "--build", "--preset", preset],
        cwd=ROOT,
        check=True
    )

    print()
    print(f"[build-web] Build complete!")
    print(f"[build-web] Output directory: {build_dir}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\n[build-web] Error: Command failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        print("\n[build-web] Build cancelled by user", file=sys.stderr)
        sys.exit(130)
