#!/usr/bin/env python3
"""Tag a release and push to trigger CI publishing for all platforms.

Usage:
  python tools/release.py 0.2.0
"""

import re
import subprocess
import sys


SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+$")


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, check=True, **kwargs)


def git_is_clean() -> bool:
    result = subprocess.run(
        ["git", "status", "--porcelain"],
        capture_output=True, text=True,
    )
    return result.stdout.strip() == ""


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} <version>  (e.g. 0.2.0)")

    version = sys.argv[1]
    if not SEMVER_RE.match(version):
        sys.exit(f"Error: version must be semver (e.g. 1.2.3), got: {version}")

    if not git_is_clean():
        sys.exit("Error: working tree has uncommitted changes. Commit or stash first.")

    tag = f"v{version}"
    print(f"Tagging release {tag} ...")
    run(["git", "tag", "-a", tag, "-m", f"Release {tag}"])
    run(["git", "push", "origin", tag])

    print(f"\nTag {tag} pushed. CI will build and publish:")
    print("  Android AAR       -> Maven Central")
    print("  iOS XCFramework   -> CocoaPods / SPM")
    print("  Flutter package   -> pub.dev")
    print("  Web package       -> npm")


if __name__ == "__main__":
    main()
