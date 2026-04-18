#!/usr/bin/env python3
"""Build AnyChat SDK for the native host platform (Linux / macOS / Windows).

This script uses CMake Presets for standardized cross-platform builds.

Usage:
  python3 scripts/build-native.py [options]

Options:
  --preset PRESET              CMake preset name (auto-detect if not specified)
  --compiler {gcc,clang,msvc}  Compiler choice (default: auto-detect)
  --config {debug,release}     Build configuration (default: release)
  -j, --jobs N                 Parallel jobs passed to cmake --build (default: cpu count)
  --test                       Run CTest after a successful build
  --clean                      Clean build directory before building
  --install / --no-install     Run or skip `cmake --install` (default: install)
  --install-prefix DIR         Install prefix (default: build/install)
  --bundle-runtime-deps        Bundle dependent shared libraries (default: enabled)
  --list-presets               List all available CMake presets
"""

import argparse
import filecmp
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
DEFAULT_BINARY_DIR_TEMPLATE = "${sourceDir}/build"


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


def run_host_command(command: list[str]) -> subprocess.CompletedProcess[str]:
    """Run a host command and return captured output."""
    return subprocess.run(
        command,
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        errors="replace",
    )


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


def merge_preset_dict(base: dict, override: dict) -> dict:
    """Merge configure preset dictionaries with cache variable inheritance."""
    result = dict(base)
    for key, value in override.items():
        if key == "inherits":
            continue
        if key in {"cacheVariables", "environment"}:
            merged = dict(result.get(key, {}))
            if isinstance(value, dict):
                merged.update(value)
            result[key] = merged
            continue
        result[key] = value
    return result


def resolve_configure_preset(preset: str) -> dict:
    """Resolve a configure preset with inheritance applied."""
    presets_data = load_presets()
    configure_presets = presets_data.get("configurePresets", [])
    presets_by_name = {item.get("name"): item for item in configure_presets if item.get("name")}

    if preset not in presets_by_name:
        print(f"Error: Preset '{preset}' not found in CMakePresets.json", file=sys.stderr)
        sys.exit(1)

    resolving: set[str] = set()
    resolved_cache: dict[str, dict] = {}

    def resolve_one(name: str) -> dict:
        if name in resolved_cache:
            return dict(resolved_cache[name])
        if name in resolving:
            print(f"Error: Circular preset inheritance detected at '{name}'", file=sys.stderr)
            sys.exit(1)

        preset_data = presets_by_name.get(name)
        if preset_data is None:
            print(f"Error: Inherited preset '{name}' not found in CMakePresets.json", file=sys.stderr)
            sys.exit(1)

        resolving.add(name)
        merged: dict = {}
        inherits = preset_data.get("inherits", [])
        if isinstance(inherits, str):
            inherits = [inherits]

        for parent in inherits:
            merged = merge_preset_dict(merged, resolve_one(parent))

        merged = merge_preset_dict(merged, preset_data)
        resolving.remove(name)
        resolved_cache[name] = merged
        return dict(merged)

    return resolve_one(preset)


def expand_preset_value(value: str, preset: str) -> str:
    """Expand commonly used CMake preset variables in a string."""
    expanded = value.replace("${sourceDir}", str(ROOT))
    expanded = expanded.replace("${presetName}", preset)
    return expanded


def get_build_dir(preset: str) -> Path:
    """Resolve the build directory from the selected configure preset."""
    preset_data = resolve_configure_preset(preset)
    binary_dir_template = preset_data.get("binaryDir", DEFAULT_BINARY_DIR_TEMPLATE)
    if not isinstance(binary_dir_template, str) or not binary_dir_template:
        binary_dir_template = DEFAULT_BINARY_DIR_TEMPLATE

    binary_dir = expand_preset_value(binary_dir_template, preset)
    build_path = Path(binary_dir).expanduser()
    if not build_path.is_absolute():
        build_path = ROOT / build_path
    return build_path


def get_build_config(preset: str, fallback: str) -> str:
    """Resolve build configuration name for cmake --install --config."""
    preset_data = resolve_configure_preset(preset)
    cache_variables = preset_data.get("cacheVariables", {})
    if isinstance(cache_variables, dict):
        build_type = cache_variables.get("CMAKE_BUILD_TYPE")
        if isinstance(build_type, str) and build_type.strip():
            return build_type
    return fallback.capitalize()


def get_install_prefix(preset: str, install_prefix_arg: Optional[str]) -> Path:
    """Resolve installation prefix path."""
    if install_prefix_arg:
        install_prefix = Path(install_prefix_arg).expanduser()
        if not install_prefix.is_absolute():
            install_prefix = ROOT / install_prefix
        return install_prefix
    return ROOT / "build" / "install"


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
        "--install",
        dest="install",
        action="store_true",
        default=True,
        help="Run cmake --install after build (default: enabled)",
    )
    parser.add_argument(
        "--no-install",
        dest="install",
        action="store_false",
        help="Skip cmake --install",
    )
    parser.add_argument(
        "--install-prefix",
        metavar="DIR",
        help="Install prefix for cmake --install (default: build/install)",
    )
    parser.add_argument(
        "--bundle-runtime-deps",
        dest="bundle_runtime_deps",
        action="store_true",
        default=True,
        help="Copy dependent shared libraries into install directory (default: enabled)",
    )
    parser.add_argument(
        "--no-bundle-runtime-deps",
        dest="bundle_runtime_deps",
        action="store_false",
        help="Skip copying dependent shared libraries",
    )
    parser.add_argument(
        "--list-presets",
        action="store_true",
        help="List all available CMake presets and exit",
    )
    return parser.parse_args()


def configure(preset: str, clean: bool, vcvars_path: Optional[Path] = None) -> Path:
    """Configure the project using the specified CMake preset."""
    print(f"[build-native] Configuring with preset: {preset}")

    build_path = get_build_dir(preset)

    # Clean if requested
    if clean and build_path.exists():
        print(f"[build-native] Cleaning build directory: {build_path}")
        shutil.rmtree(build_path)

    # Configure
    cmake_args = ["cmake", "--preset", preset]
    run_build_command(cmake_args, vcvars_path)
    return build_path


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


def install(
    build_dir: Path,
    install_prefix: Path,
    build_config: str,
    vcvars_path: Optional[Path] = None,
) -> None:
    """Run cmake install into a publishable directory."""
    print(f"[build-native] Installing to: {install_prefix}")
    install_prefix.mkdir(parents=True, exist_ok=True)

    cmake_args = [
        "cmake",
        "--install",
        str(build_dir),
        "--prefix",
        str(install_prefix),
        "--config",
        build_config,
    ]
    run_build_command(cmake_args, vcvars_path)


def copy_file_if_changed(src: Path, dst: Path) -> bool:
    """Copy a file only if content is different or destination is missing."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        try:
            if filecmp.cmp(src, dst, shallow=False):
                return False
        except OSError:
            pass
    shutil.copy2(src, dst)
    return True


def parse_linux_runtime_dependencies(binary: Path) -> set[Path]:
    """Parse shared library dependencies for a binary via ldd."""
    result = run_host_command(["ldd", str(binary)])
    if result.returncode != 0:
        print(
            f"[build-native] Warning: failed to inspect dependencies with ldd: {binary}",
            file=sys.stderr,
        )
        return set()

    dependencies: set[Path] = set()
    for raw_line in result.stdout.splitlines():
        line = raw_line.strip()
        if not line:
            continue

        dep_path = ""
        if "=>" in line:
            left, right = line.split("=>", 1)
            right = right.strip()
            if right.startswith("not found"):
                print(
                    f"[build-native] Warning: unresolved dependency for {binary}: {left.strip()}",
                    file=sys.stderr,
                )
                continue
            dep_path = right.split("(", 1)[0].strip()
        elif line.startswith("/"):
            dep_path = line.split("(", 1)[0].strip()

        if dep_path.startswith("/"):
            candidate = Path(dep_path)
            if candidate.is_file():
                dependencies.add(candidate)

    return dependencies


def parse_macos_runtime_dependencies(binary: Path) -> set[Path]:
    """Parse shared library dependencies for a binary via otool."""
    result = run_host_command(["otool", "-L", str(binary)])
    if result.returncode != 0:
        print(
            f"[build-native] Warning: failed to inspect dependencies with otool: {binary}",
            file=sys.stderr,
        )
        return set()

    dependencies: set[Path] = set()
    lines = result.stdout.splitlines()
    for raw_line in lines[1:]:
        line = raw_line.strip()
        if not line:
            continue
        dep_path = line.split(" (", 1)[0].strip()
        if dep_path.startswith("@"):
            continue
        candidate = Path(dep_path)
        if candidate.is_file():
            dependencies.add(candidate)

    return dependencies


def should_bundle_dependency(dep: Path, platform_name: str) -> bool:
    """Filter out platform runtime libraries that should not be bundled."""
    dep_name = dep.name.lower()
    dep_path = str(dep).lower()

    if platform_name == "linux":
        excluded = {
            "ld-linux-x86-64.so.2",
            "libc.so.6",
            "libdl.so.2",
            "libgcc_s.so.1",
            "libm.so.6",
            "libpthread.so.0",
            "libresolv.so.2",
            "librt.so.1",
            "libstdc++.so.6",
            "libutil.so.1",
        }
        if dep_name in excluded or dep_name.startswith("ld-linux"):
            return False
        return True

    if platform_name == "macos":
        return not (dep_path.startswith("/usr/lib/") or dep_path.startswith("/system/library/"))

    if platform_name == "windows":
        return "windows\\system32" not in dep_path

    return True


def find_installed_anychat_shared_artifacts(install_prefix: Path, platform_name: str) -> list[Path]:
    """Find installed shared artifacts for dependency scanning."""
    if platform_name == "linux":
        patterns = ["lib/libanychat.so*"]
    elif platform_name == "macos":
        patterns = ["lib/libanychat*.dylib"]
    elif platform_name == "windows":
        patterns = ["bin/anychat*.dll", "bin/libanychat*.dll"]
    else:
        return []

    candidates: list[Path] = []
    for pattern in patterns:
        for item in install_prefix.glob(pattern):
            if item.is_file():
                candidates.append(item)

    non_symlink = [item for item in candidates if not item.is_symlink()]
    return non_symlink if non_symlink else candidates


def bundle_unix_runtime_dependencies(install_prefix: Path, platform_name: str) -> int:
    """Bundle Linux/macOS runtime shared-library dependencies."""
    artifacts = find_installed_anychat_shared_artifacts(install_prefix, platform_name)
    if not artifacts:
        return 0

    resolved_deps: set[Path] = set()
    for artifact in artifacts:
        if platform_name == "linux":
            resolved_deps.update(parse_linux_runtime_dependencies(artifact))
        elif platform_name == "macos":
            resolved_deps.update(parse_macos_runtime_dependencies(artifact))

    install_root = install_prefix.resolve()
    bundled_count = 0
    destination_dir = install_prefix / "lib"

    for dep in sorted(resolved_deps):
        dep_resolved = dep.resolve()
        try:
            dep_resolved.relative_to(install_root)
            continue
        except ValueError:
            pass

        if not should_bundle_dependency(dep_resolved, platform_name):
            continue

        destination = destination_dir / dep_resolved.name
        if copy_file_if_changed(dep_resolved, destination):
            bundled_count += 1
            print(f"[build-native] Bundled runtime dependency: {dep_resolved.name}")

    return bundled_count


def bundle_windows_runtime_dependencies(build_dir: Path, install_prefix: Path) -> int:
    """Bundle runtime DLLs from build output into install/bin on Windows."""
    runtime_dir = build_dir / "bin"
    if not runtime_dir.exists():
        return 0

    destination_dir = install_prefix / "bin"
    bundled_count = 0
    for dll_file in sorted(runtime_dir.glob("*.dll")):
        if dll_file.name.lower().startswith("anychat"):
            continue
        destination = destination_dir / dll_file.name
        if copy_file_if_changed(dll_file, destination):
            bundled_count += 1
            print(f"[build-native] Bundled runtime dependency: {dll_file.name}")
    return bundled_count


def bundle_runtime_dependencies(install_prefix: Path, build_dir: Path) -> int:
    """Bundle dependent shared libraries into install prefix."""
    platform_name = detect_platform()
    if platform_name == "windows":
        return bundle_windows_runtime_dependencies(build_dir, install_prefix)
    if platform_name in {"linux", "macos"}:
        return bundle_unix_runtime_dependencies(install_prefix, platform_name)
    return 0


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
    build_config = get_build_config(preset, args.config)
    install_prefix = get_install_prefix(preset, args.install_prefix)

    # Configure
    build_dir = configure(preset, args.clean, vcvars_path)

    # Build
    build(preset, args.jobs, vcvars_path)

    # Test
    if args.test:
        run_tests(preset, vcvars_path)

    # Install
    bundled_count = 0
    if args.install:
        install(build_dir, install_prefix, build_config, vcvars_path)
        if args.bundle_runtime_deps:
            bundled_count = bundle_runtime_dependencies(install_prefix, build_dir)

    print()
    print(f"[build-native] Build complete!")
    print(f"[build-native] Preset: {preset}")
    print(f"[build-native] Build directory: {build_dir}")
    if args.install:
        print(f"[build-native] Install directory: {install_prefix}")
        if args.bundle_runtime_deps:
            print(f"[build-native] Bundled runtime dependencies: {bundled_count}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as e:
        print(f"\n[build-native] Error: Command failed with exit code {e.returncode}", file=sys.stderr)
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        print("\n[build-native] Build cancelled by user", file=sys.stderr)
        sys.exit(130)
