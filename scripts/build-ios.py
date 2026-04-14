#!/usr/bin/env python3
"""Build AnyChat SDK for iOS using CMake Presets.

This script builds the native library for iOS devices and simulator using Xcode.

Requirements:
  - macOS with Xcode installed
  - CMake 3.20+

Usage:
  python3 tools/build-ios.py [options]

Options:
  --config {debug,release}  Build configuration (default: release)
  --clean                   Clean build directory before building
  --list-presets            List available iOS presets
"""

import argparse
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def check_macos() -> None:
    """Check if running on macOS."""
    if platform.system() != "Darwin":
        print("Error: iOS builds require macOS with Xcode", file=sys.stderr)
        sys.exit(1)

    # Check if xcodebuild is available
    try:
        subprocess.run(
            ["xcodebuild", "-version"],
            check=True,
            capture_output=True
        )
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("Error: Xcode not found. Please install Xcode from App Store", file=sys.stderr)
        sys.exit(1)


def list_presets() -> None:
    """List available iOS CMake presets."""
    print("Available iOS CMake Presets:")
    print("=" * 70)
    print("  ios-debug                    - iOS Debug")
    print("  ios-release                  - iOS Release")
    print()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build AnyChat SDK for iOS using CMake Presets.",
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
        help="List available iOS CMake presets and exit",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if args.list_presets:
        list_presets()
        return

    # Check macOS
    check_macos()

    preset = f"ios-{args.config}"
    build_dir = ROOT / "build" / preset

    print(f"[build-ios] Building for iOS ({args.config})")
    print(f"[build-ios] Preset: {preset}")
    print(f"[build-ios] Build directory: {build_dir}")
    print()

    # Clean if requested
    if args.clean and build_dir.exists():
        print(f"[build-ios] Cleaning: {build_dir}")
        shutil.rmtree(build_dir)

    # Configure
    print(f"[build-ios] Configuring...")
    subprocess.run(
        ["cmake", "--preset", preset],
        cwd=ROOT,
        check=True
    )

    # Build
    print(f"[build-ios] Building...")
    subprocess.run(
        ["cmake", "--build", "--preset", preset, "--config", args.config.capitalize()],
        cwd=ROOT,
        check=True
    )

    print()
    print(f"[build-ios] Build complete!")
    print(f"[build-ios] Output directory: {build_dir}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\n[build-ios] Error: Command failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        print("\n[build-ios] Build cancelled by user", file=sys.stderr)
        sys.exit(130)
