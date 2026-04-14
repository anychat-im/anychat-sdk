#!/usr/bin/env python3
"""Build AnyChat SDK for Android using CMake Presets.

This script builds the native library for Android using the Android NDK toolchain.

Requirements:
  - Android NDK (set ANDROID_NDK_HOME environment variable)
  - CMake 3.20+
  - Ninja build system

Usage:
  python3 tools/build-android.py [options]

Options:
  --abi {arm64-v8a,armeabi-v7a,all}  Target ABI (default: arm64-v8a)
  --config {debug,release}           Build configuration (default: release)
  -j, --jobs N                       Parallel jobs (default: cpu count)
  --clean                            Clean build directory before building
  --list-presets                     List available Android presets
"""

import argparse
import multiprocessing
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def check_ndk() -> None:
    """Check if Android NDK is available."""
    ndk_home = os.environ.get("ANDROID_NDK_HOME")
    if not ndk_home:
        print("Error: ANDROID_NDK_HOME environment variable not set", file=sys.stderr)
        print("\nPlease set ANDROID_NDK_HOME to your Android NDK installation path:", file=sys.stderr)
        print("  export ANDROID_NDK_HOME=/path/to/android-ndk", file=sys.stderr)
        sys.exit(1)

    ndk_path = Path(ndk_home)
    if not ndk_path.exists():
        print(f"Error: Android NDK not found at: {ndk_home}", file=sys.stderr)
        sys.exit(1)

    print(f"[build-android] Using Android NDK: {ndk_home}")


def list_presets() -> None:
    """List available Android CMake presets."""
    print("Available Android CMake Presets:")
    print("=" * 70)
    print("  android-arm64-debug          - Android ARM64 Debug")
    print("  android-arm64-release        - Android ARM64 Release")
    print("  android-armv7-debug          - Android ARMv7 Debug")
    print("  android-armv7-release        - Android ARMv7 Release")
    print()


def get_preset_name(abi: str, config: str) -> str:
    """Get CMake preset name for the given ABI and config."""
    config_lower = config.lower()

    if abi == "arm64-v8a":
        return f"android-arm64-{config_lower}"
    elif abi == "armeabi-v7a":
        return f"android-armv7-{config_lower}"
    else:
        print(f"Error: Unsupported ABI: {abi}", file=sys.stderr)
        sys.exit(1)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build AnyChat SDK for Android using CMake Presets.",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--abi",
        choices=["arm64-v8a", "armeabi-v7a", "all"],
        default="arm64-v8a",
        help="Target Android ABI (default: arm64-v8a)",
    )
    parser.add_argument(
        "--config",
        choices=["debug", "release"],
        default="release",
        help="Build configuration (default: release)",
    )
    parser.add_argument(
        "-j", "--jobs",
        type=int,
        default=multiprocessing.cpu_count(),
        metavar="N",
        help="Number of parallel build jobs (default: cpu count)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build directory before building",
    )
    parser.add_argument(
        "--list-presets",
        action="store_true",
        help="List available Android CMake presets and exit",
    )
    return parser.parse_args()


def build_abi(abi: str, config: str, jobs: int, clean: bool) -> None:
    """Build for a specific ABI."""
    preset = get_preset_name(abi, config)
    build_dir = ROOT / "build" / preset

    print(f"\n[build-android] Building {abi} ({config})")
    print(f"[build-android] Preset: {preset}")
    print(f"[build-android] Build directory: {build_dir}")

    # Clean if requested
    if clean and build_dir.exists():
        print(f"[build-android] Cleaning: {build_dir}")
        shutil.rmtree(build_dir)

    # Configure
    print(f"[build-android] Configuring...")
    subprocess.run(
        ["cmake", "--preset", preset],
        cwd=ROOT,
        check=True
    )

    # Build
    print(f"[build-android] Building (jobs={jobs})...")
    subprocess.run(
        ["cmake", "--build", "--preset", preset, "--parallel", str(jobs)],
        cwd=ROOT,
        check=True
    )

    print(f"[build-android] ✓ {abi} build complete")


def main() -> None:
    args = parse_args()

    if args.list_presets:
        list_presets()
        return

    # Check NDK
    check_ndk()

    # Determine which ABIs to build
    if args.abi == "all":
        abis = ["arm64-v8a", "armeabi-v7a"]
    else:
        abis = [args.abi]

    print(f"[build-android] Building for ABIs: {', '.join(abis)}")
    print(f"[build-android] Configuration: {args.config}")
    print()

    # Build each ABI
    for abi in abis:
        build_abi(abi, args.config, args.jobs, args.clean)

    print()
    print("[build-android] All builds complete!")
    print(f"[build-android] Output directory: {ROOT / 'build'}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\n[build-android] Error: Command failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        print("\n[build-android] Build cancelled by user", file=sys.stderr)
        sys.exit(130)
