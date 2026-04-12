#!/usr/bin/env python3
"""Build AnyChat SDK for the native host platform (Linux / macOS / Windows).

This script uses CMake Presets for standardized cross-platform builds.

Usage:
  python3 tools/build-native.py [options]

Options:
  --preset PRESET            CMake preset name (auto-detect if not specified)
  --compiler {gcc,clang,msvc} Compiler choice (default: auto-detect)
  --config {debug,release}   Build configuration (default: release)
  -j, --jobs N               Parallel jobs passed to cmake --build (default: cpu count)
  --test                     Run CTest after a successful build
  --clean                    Clean build directory before building
  --list-presets             List all available CMake presets
"""

import argparse
import json
import multiprocessing
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

ROOT = Path(__file__).resolve().parent.parent
PRESETS_FILE = ROOT / "CMakePresets.json"


def find_vswhere() -> Optional[Path]:
    """Locate vswhere.exe on Windows."""
    candidates = [
        Path(os.environ.get("ProgramFiles(x86)", "")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe",
        Path(os.environ.get("ProgramFiles", "")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def find_visual_studio_vcvars() -> Optional[Path]:
    """Locate vcvars64.bat for a local Visual Studio installation."""
    if detect_platform() != "windows":
        return None

    env_override = os.environ.get("VCVARS64_BAT")
    if env_override:
        candidate = Path(env_override).expanduser()
        if candidate.is_file():
            return candidate

    vs_install_dir = os.environ.get("VSINSTALLDIR")
    if vs_install_dir:
        candidate = Path(vs_install_dir) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
        if candidate.is_file():
            return candidate

    vswhere = find_vswhere()
    if vswhere is not None:
        result = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-find",
                r"VC\Auxiliary\Build\vcvars64.bat",
            ],
            capture_output=True,
            text=True,
            errors="replace",
            check=False,
        )
        if result.returncode == 0:
            for line in result.stdout.splitlines():
                candidate = Path(line.strip())
                if candidate.is_file():
                    return candidate

    install_roots = [
        Path(os.environ.get("ProgramFiles", "")) / "Microsoft Visual Studio",
        Path(os.environ.get("ProgramFiles(x86)", "")) / "Microsoft Visual Studio",
    ]
    versions = ["2022", "2019", "2017"]
    editions = ["BuildTools", "Community", "Professional", "Enterprise"]

    for root in install_roots:
        for version in versions:
            for edition in editions:
                candidate = root / version / edition / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
                if candidate.is_file():
                    return candidate

    return None


def run_command_with_vcvars(command: list[str], vcvars_path: Path) -> None:
    """Run a command inside a temporary batch file after calling vcvars64.bat."""
    temp_batch_path: Optional[Path] = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            suffix=".bat",
            delete=False,
            encoding="utf-8",
        ) as temp_batch:
            temp_batch.write("@echo off\n")
            temp_batch.write(f'call "{vcvars_path}" >nul\n')
            temp_batch.write("if errorlevel 1 exit /b %errorlevel%\n")
            temp_batch.write(f'cd /d "{ROOT}"\n')
            temp_batch.write(f"{subprocess.list2cmdline(command)}\n")
            temp_batch.write("exit /b %errorlevel%\n")
            temp_batch_path = Path(temp_batch.name)

        subprocess.run(
            ["cmd.exe", "/d", "/c", str(temp_batch_path)],
            check=True,
        )
    finally:
        if temp_batch_path is not None and temp_batch_path.exists():
            temp_batch_path.unlink()

def resolve_vcvars_path(preset: str) -> Optional[Path]:
    """Resolve the vcvars64.bat path needed by the selected preset."""
    if detect_platform() != "windows" or not preset.startswith("windows-msvc-"):
        return None

    current_env = os.environ
    current_path = current_env.get("PATH") or current_env.get("Path") or ""
    if (
        shutil.which("cl", path=current_path)
        and current_env.get("INCLUDE")
        and current_env.get("LIB")
    ):
        print("[build-native] MSVC environment already initialized")
        return None

    vcvars_path = find_visual_studio_vcvars()
    if vcvars_path is None:
        print(
            "Error: Could not find Visual Studio vcvars64.bat. "
            "Install Visual Studio Build Tools or set VCVARS64_BAT.",
            file=sys.stderr,
        )
        sys.exit(1)

    print(f"[build-native] Initializing MSVC environment: {vcvars_path}")
    return vcvars_path


def run_build_command(command: list[str], vcvars_path: Optional[Path] = None) -> None:
    """Run a build command, using a temporary vcvars batch on Windows when needed."""
    if vcvars_path is not None:
        run_command_with_vcvars(command, vcvars_path)
        return

    subprocess.run(command, cwd=ROOT, check=True)


def load_presets() -> dict:
    """Load CMakePresets.json and return parsed data."""
    if not PRESETS_FILE.exists():
        print(f"Error: {PRESETS_FILE} not found", file=sys.stderr)
        sys.exit(1)

    with open(PRESETS_FILE, "r", encoding="utf-8") as f:
        return json.load(f)


def list_presets() -> None:
    """List all available CMake configure presets."""
    presets_data = load_presets()
    configure_presets = presets_data.get("configurePresets", [])

    print("Available CMake Configure Presets:")
    print("=" * 70)
    for preset in configure_presets:
        if preset.get("hidden", False):
            continue
        name = preset.get("name", "")
        display_name = preset.get("displayName", "")
        description = preset.get("description", "")
        print(f"  {name:30s} - {display_name}")
        if description:
            print(f"    {description}")
    print()


def detect_platform() -> str:
    """Detect the current platform."""
    system = platform.system()
    if system == "Linux":
        return "linux"
    elif system == "Darwin":
        return "macos"
    elif system == "Windows":
        return "windows"
    else:
        print(f"Error: Unsupported platform: {system}", file=sys.stderr)
        sys.exit(1)


def auto_detect_preset(compiler: Optional[str], config: str) -> str:
    """Auto-detect the appropriate CMake preset based on platform and options."""
    plat = detect_platform()
    config_lower = config.lower()

    # Platform-specific compiler defaults
    if plat == "linux":
        if compiler is None:
            # Default to GCC on Linux
            compiler = "gcc"
        if compiler not in ["gcc", "clang"]:
            print(f"Error: Invalid compiler '{compiler}' for Linux (must be gcc or clang)", file=sys.stderr)
            sys.exit(1)
        return f"linux-{compiler}-{config_lower}"

    elif plat == "macos":
        # macOS always uses clang
        return f"macos-clang-{config_lower}"

    elif plat == "windows":
        # Windows always uses MSVC
        return f"windows-msvc-{config_lower}"

    return ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build AnyChat SDK for the native host platform using CMake Presets.",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--preset",
        metavar="PRESET",
        help="CMake preset name (auto-detect if not specified)",
    )
    parser.add_argument(
        "--compiler",
        choices=["gcc", "clang", "msvc"],
        help="Compiler choice (default: auto-detect based on platform)",
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
        "--test",
        action="store_true",
        help="Run CTest after a successful build",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build directory before building",
    )
    parser.add_argument(
        "--list-presets",
        action="store_true",
        help="List all available CMake presets and exit",
    )
    return parser.parse_args()


def configure(preset: str, clean: bool, vcvars_path: Optional[Path] = None) -> None:
    """Configure the project using the specified CMake preset."""
    print(f"[build-native] Configuring with preset: {preset}")

    # Determine build directory from preset
    presets_data = load_presets()
    configure_presets = presets_data.get("configurePresets", [])
    preset_data = None
    for p in configure_presets:
        if p.get("name") == preset:
            preset_data = p
            break

    if not preset_data:
        print(f"Error: Preset '{preset}' not found in CMakePresets.json", file=sys.stderr)
        sys.exit(1)

    # Get binary dir (expand variables)
    binary_dir_template = preset_data.get("binaryDir", "")
    binary_dir = binary_dir_template.replace("${sourceDir}", str(ROOT))
    binary_dir = binary_dir.replace("${presetName}", preset)
    build_path = Path(binary_dir)

    # Clean if requested
    if clean and build_path.exists():
        print(f"[build-native] Cleaning build directory: {build_path}")
        shutil.rmtree(build_path)

    # Configure
    cmake_args = ["cmake", "--preset", preset]
    run_build_command(cmake_args, vcvars_path)


def build(preset: str, jobs: int, vcvars_path: Optional[Path] = None) -> None:
    """Build the project using the specified preset."""
    print(f"[build-native] Building with preset: {preset} (jobs={jobs})")

    cmake_args = [
        "cmake", "--build", "--preset", preset,
        "--parallel", str(jobs)
    ]
    run_build_command(cmake_args, vcvars_path)


def run_tests(preset: str, vcvars_path: Optional[Path] = None) -> None:
    """Run tests using the specified preset."""
    print(f"[build-native] Running tests with preset: {preset}")

    cmake_args = ["ctest", "--preset", preset]
    run_build_command(cmake_args, vcvars_path)


def main() -> None:
    args = parse_args()

    if args.list_presets:
        list_presets()
        return

    # Determine the preset to use
    if args.preset:
        preset = args.preset
    else:
        preset = auto_detect_preset(args.compiler, args.config)

    print(f"[build-native] Using CMake preset: {preset}")
    print(f"[build-native] Platform: {detect_platform()}")
    print()

    vcvars_path = resolve_vcvars_path(preset)

    # Configure
    configure(preset, args.clean, vcvars_path)

    # Build
    build(preset, args.jobs, vcvars_path)

    # Test
    if args.test:
        run_tests(preset, vcvars_path)

    print()
    print(f"[build-native] Build complete!")
    print(f"[build-native] Preset: {preset}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\n[build-native] Error: Command failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        print("\n[build-native] Build cancelled by user", file=sys.stderr)
        sys.exit(130)
