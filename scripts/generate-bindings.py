#!/usr/bin/env python3
"""Generate SWIG bindings for all or a specific platform.

Usage:
  python tools/generate-bindings.py [android|ios|flutter|web|all]

Requirements:
  swig in PATH
"""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SWIG_MAIN = ROOT / "swig" / "anychat.i"
INCLUDE_DIR = ROOT / "core" / "include"


def swig(*args: str) -> None:
    subprocess.run(["swig", "-c++", *args, f"-I{INCLUDE_DIR}", str(SWIG_MAIN)], check=True)


def generate_android() -> None:
    print("[bindings] Generating Android (Java/JNI) ...")
    out_jni  = ROOT / "bindings" / "android" / "jni"
    out_java = ROOT / "bindings" / "android" / "java" / "com" / "anychat" / "sdk"
    out_jni.mkdir(parents=True, exist_ok=True)
    out_java.mkdir(parents=True, exist_ok=True)
    swig(
        "-java",
        "-package", "com.anychat.sdk",
        "-outdir", str(out_java),
        "-o", str(out_jni / "anychat_wrap.cpp"),
    )
    print(f"  -> {out_jni / 'anychat_wrap.cpp'}")


def generate_ios() -> None:
    print("[bindings] Generating iOS (ObjC) ...")
    out = ROOT / "bindings" / "ios" / "objc"
    out.mkdir(parents=True, exist_ok=True)
    swig("-objc", "-o", str(out / "anychat_wrap.mm"))
    print(f"  -> {out / 'anychat_wrap.mm'}")


def generate_flutter() -> None:
    print("[bindings] Generating Flutter (C wrapper for Dart FFI) ...")
    out = ROOT / "bindings" / "flutter" / "lib" / "src" / "generated"
    out.mkdir(parents=True, exist_ok=True)
    swig("-java", "-o", str(out / "anychat_wrap.cpp"))
    print(f"  -> {out / 'anychat_wrap.cpp'}")


def generate_web() -> None:
    print("[bindings] Generating Web (Emscripten/WASM) ...")
    out = ROOT / "bindings" / "web" / "src"
    out.mkdir(parents=True, exist_ok=True)
    swig("-javascript", "-node", "-o", str(out / "anychat_wrap.cpp"))
    print(f"  -> {out / 'anychat_wrap.cpp'}")


TARGETS = {
    "android": generate_android,
    "ios":     generate_ios,
    "flutter": generate_flutter,
    "web":     generate_web,
}


def main() -> None:
    target = sys.argv[1] if len(sys.argv) > 1 else "all"

    if target == "all":
        for fn in TARGETS.values():
            fn()
        print("\n[bindings] All bindings generated.")
    elif target in TARGETS:
        TARGETS[target]()
    else:
        sys.exit(
            f"Unknown target: {target}\n"
            f"Usage: {sys.argv[0]} [{'|'.join(TARGETS)}|all]"
        )


if __name__ == "__main__":
    main()
