#!/usr/bin/env python3
"""
Simple launcher for MGD Engine prototype.
"""

import subprocess
import sys
import os
from pathlib import Path


def find_executable() -> Path:
    candidates = [
        Path("build/mgd_prototype"),
        Path("build/Debug/mgd_prototype.exe"),
        Path("build/Release/mgd_prototype.exe"),
        Path("mgd_prototype"),
        Path("mgd_prototype.exe"),
        Path("../build/mgd_prototype"),
        Path("../build/Debug/mgd_prototype.exe"),
        Path("../build/Release/mgd_prototype.exe"),
    ]
    for c in candidates:
        if c.exists():
            return c.absolute()
    return None


def main():
    exe = find_executable()

    if exe is None:
        print("ERROR: MGD executable not found!")
        print("Please build the project first:")
        print("  mkdir build && cd build && cmake .. && cmake --build .")
        return 1

    print(f"Found executable: {exe}")
    print("Launching MGD Engine Prototype...")
    print("Controls:")
    print("  WASD/Arrows - Move player")
    print("  Q/E         - Move player up/down (Z axis)")
    print("  +/-         - Zoom camera")
    print("  R/F         - Rotate camera")
    print("  ESC         - Quit")
    print()

    try:
        result = subprocess.run([str(exe)], check=False)
        return result.returncode
    except KeyboardInterrupt:
        print("\nStopped by user")
        return 0
    except Exception as e:
        print(f"Error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())