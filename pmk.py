#!/usr/bin/env python3
"""
pmk — PMK (Pico Magnetic Keyboard) build tool

Usage:
    pmk setup                 Download toolchain (first-time setup)
    pmk build -kb <board>     Build firmware
    pmk flash -kb <board>     Build + flash via picotool
    pmk new   -kb <board>     Create new board from template
    pmk clean -kb <board>     Remove build directory
    pmk list                  List available boards
    pmk doctor                Check toolchain dependencies
"""

import argparse
import glob
import io
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BOARDS_DIR = os.path.join(SCRIPT_DIR, "boards")
TOOLCHAIN_DIR = os.path.join(SCRIPT_DIR, ".toolchain")
TEMPLATE_BOARD = "example_60"

# =========================================================================
# Toolchain download URLs (Windows x64 / Linux x64 / macOS)
# =========================================================================

ARCH = platform.machine().lower()
IS_WIN = sys.platform == "win32"
IS_MAC = sys.platform == "darwin"
IS_LINUX = sys.platform.startswith("linux")

# ARM GCC 14.2 (arm-none-eabi)
ARM_GCC_URLS = {
    "win": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip",
    "linux": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz",
    "mac": "https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi.tar.xz",
}

# CMake portable
CMAKE_URLS = {
    "win": "https://github.com/Kitware/CMake/releases/download/v3.31.4/cmake-3.31.4-windows-x86_64.zip",
    "linux": "https://github.com/Kitware/CMake/releases/download/v3.31.4/cmake-3.31.4-linux-x86_64.tar.gz",
    "mac": "https://github.com/Kitware/CMake/releases/download/v3.31.4/cmake-3.31.4-macos-universal.tar.gz",
}

# Ninja build
NINJA_URLS = {
    "win": "https://github.com/nicknisi/ninja/releases/download/v1.12.1/ninja-win.zip",
    "linux": "https://github.com/nicknisi/ninja/releases/download/v1.12.1/ninja-linux.zip",
    "mac": "https://github.com/nicknisi/ninja/releases/download/v1.12.1/ninja-mac.zip",
}

PICO_SDK_URL = "https://github.com/raspberrypi/pico-sdk/archive/refs/tags/2.1.1.zip"
PICO_SDK_VERSION = "2.1.1"

# Standard locations where the Pico VS Code extension installs the SDK
PICO_SDK_SEARCH_PATHS = [
    os.path.join(TOOLCHAIN_DIR, "pico-sdk"),
    os.path.expanduser("~/.pico-sdk/sdk/2.2.0"),
    os.path.expanduser("~/.pico-sdk/sdk/2.1.1"),
    os.path.expanduser("~/.pico-sdk/sdk/2.1.0"),
    os.path.expanduser("~/.pico-sdk/sdk/2.0.0"),
]

PICOTOOL_SEARCH_PATHS = [
    os.path.expanduser("~/.pico-sdk/picotool/2.2.0-a4/picotool/picotool.exe"),
    os.path.expanduser("~/.pico-sdk/picotool/2.2.0-a4/picotool/picotool"),
    os.path.expanduser("~/.pico-sdk/picotool/2.2.0/picotool/picotool.exe"),
    os.path.expanduser("~/.pico-sdk/picotool/2.2.0/picotool/picotool"),
]


def get_platform_key():
    if IS_WIN:
        return "win"
    if IS_MAC:
        return "mac"
    return "linux"


# =========================================================================
# Tool finding — checks .toolchain/, Pico extension, then PATH
# =========================================================================

def _pico_extension_bin_dirs():
    """Find bin directories from the Pico VS Code extension (~/.pico-sdk)."""
    dirs = []
    pico_sdk_dir = os.path.expanduser("~/.pico-sdk")
    if not os.path.isdir(pico_sdk_dir):
        return dirs
    # Toolchain (ARM GCC): ~/.pico-sdk/toolchain/<version>/bin
    tc_dir = os.path.join(pico_sdk_dir, "toolchain")
    if os.path.isdir(tc_dir):
        versions = sorted(os.listdir(tc_dir), reverse=True)
        for ver in versions:
            bindir = os.path.join(tc_dir, ver, "bin")
            if os.path.isdir(bindir):
                dirs.append(bindir)
                break
    # CMake: ~/.pico-sdk/cmake/<version>/bin
    cmake_dir = os.path.join(pico_sdk_dir, "cmake")
    if os.path.isdir(cmake_dir):
        versions = sorted(os.listdir(cmake_dir), reverse=True)
        for ver in versions:
            bindir = os.path.join(cmake_dir, ver, "bin")
            if os.path.isdir(bindir):
                dirs.append(bindir)
                break
    # Ninja: ~/.pico-sdk/ninja/<version>/
    ninja_dir = os.path.join(pico_sdk_dir, "ninja")
    if os.path.isdir(ninja_dir):
        versions = sorted(os.listdir(ninja_dir), reverse=True)
        for ver in versions:
            verdir = os.path.join(ninja_dir, ver)
            if os.path.isdir(verdir):
                dirs.append(verdir)
                break
    return dirs


def toolchain_bin_dirs():
    """Return all bin directories: .toolchain/ first, then Pico extension."""
    dirs = []
    # Local .toolchain/ directories
    if os.path.isdir(TOOLCHAIN_DIR):
        for entry in os.listdir(TOOLCHAIN_DIR):
            full = os.path.join(TOOLCHAIN_DIR, entry)
            if not os.path.isdir(full):
                continue
            bindir = os.path.join(full, "bin")
            if os.path.isdir(bindir):
                dirs.append(bindir)
            # CMake nests deeper on macOS
            cmake_bin = os.path.join(full, "CMake.app", "Contents", "bin")
            if os.path.isdir(cmake_bin):
                dirs.append(cmake_bin)
        # Ninja sometimes extracts flat
        dirs.append(TOOLCHAIN_DIR)
    # Pico VS Code extension directories
    dirs.extend(_pico_extension_bin_dirs())
    return dirs


def find_tool(name, extra_paths=None):
    """Find an executable: .toolchain/ first, Pico extension, PATH, extras."""
    ext = ".exe" if IS_WIN else ""
    # Check .toolchain/ and Pico extension bin directories
    for bindir in toolchain_bin_dirs():
        candidate = os.path.join(bindir, name + ext)
        if os.path.isfile(candidate):
            return candidate
        candidate = os.path.join(bindir, name)
        if os.path.isfile(candidate):
            return candidate
    # Fall back to system PATH
    path = shutil.which(name)
    if path:
        return path
    # Try known extra locations
    for p in (extra_paths or []):
        if os.path.isfile(p):
            return p
    return None


def find_pico_sdk():
    """Find Pico SDK: env var first, then .toolchain/, then standard locations."""
    env = os.environ.get("PICO_SDK_PATH")
    if env and os.path.isdir(env):
        return env
    for p in PICO_SDK_SEARCH_PATHS:
        if os.path.isdir(p):
            return p
    return None


def get_board_dir(board_name):
    path = os.path.join(BOARDS_DIR, board_name)
    if not os.path.isdir(path):
        print(f"Error: Board '{board_name}' not found.")
        print(f"  Run 'pmk list' to see available boards.")
        sys.exit(1)
    return path


def get_build_dir(board_dir):
    return os.path.join(board_dir, "build")


def find_uf2(build_dir):
    """Find the .uf2 file in the build directory."""
    files = glob.glob(os.path.join(build_dir, "*.uf2"))
    return files[0] if files else None


# =========================================================================
# Download helpers
# =========================================================================

def download_file(url, dest_path):
    """Download a file with progress indication."""
    filename = os.path.basename(url.split("?")[0])
    print(f"  Downloading {filename}...")
    try:
        urllib.request.urlretrieve(url, dest_path, _progress_hook)
        print()  # newline after progress
    except Exception as e:
        print(f"\n  Error downloading: {e}")
        sys.exit(1)


def _progress_hook(block_num, block_size, total_size):
    if total_size > 0:
        downloaded = block_num * block_size
        pct = min(100, int(downloaded * 100 / total_size))
        mb = downloaded / (1024 * 1024)
        total_mb = total_size / (1024 * 1024)
        sys.stdout.write(f"\r  {mb:.1f}/{total_mb:.1f} MB ({pct}%)")
        sys.stdout.flush()


def extract_archive(archive_path, dest_dir):
    """Extract zip, tar.gz, or tar.xz archive."""
    print(f"  Extracting to {os.path.basename(dest_dir)}...")
    os.makedirs(dest_dir, exist_ok=True)

    if archive_path.endswith(".zip"):
        with zipfile.ZipFile(archive_path, "r") as zf:
            zf.extractall(dest_dir)
    elif archive_path.endswith((".tar.gz", ".tar.xz", ".tar.bz2")):
        with tarfile.open(archive_path, "r:*") as tf:
            tf.extractall(dest_dir)
    else:
        print(f"  Unknown archive format: {archive_path}")
        sys.exit(1)


def flatten_single_subdir(dest_dir):
    """If extraction created a single subdirectory, move contents up."""
    entries = os.listdir(dest_dir)
    if len(entries) == 1:
        subdir = os.path.join(dest_dir, entries[0])
        if os.path.isdir(subdir):
            # Move all contents up one level
            for item in os.listdir(subdir):
                src = os.path.join(subdir, item)
                dst = os.path.join(dest_dir, item)
                shutil.move(src, dst)
            os.rmdir(subdir)


# =========================================================================
# Commands
# =========================================================================

def cmd_setup(_args):
    """Download and install the complete toolchain."""
    print("PMK Toolchain Setup\n")
    os.makedirs(TOOLCHAIN_DIR, exist_ok=True)
    plat = get_platform_key()
    tmp_dir = os.path.join(TOOLCHAIN_DIR, "_tmp")
    os.makedirs(tmp_dir, exist_ok=True)

    steps = [
        ("ARM GCC", ARM_GCC_URLS.get(plat), "arm-gcc"),
        ("CMake", CMAKE_URLS.get(plat), "cmake"),
        ("Ninja", NINJA_URLS.get(plat), "ninja"),
    ]

    for name, url, folder in steps:
        dest = os.path.join(TOOLCHAIN_DIR, folder)
        if os.path.isdir(dest) and os.listdir(dest):
            print(f"[SKIP] {name} (already installed)")
            continue
        if not url:
            print(f"[SKIP] {name} (no URL for {plat})")
            continue

        print(f"\n[INSTALL] {name}")
        ext = ".zip" if url.endswith(".zip") else ".tar.xz" if url.endswith(".tar.xz") else ".tar.gz"
        archive = os.path.join(tmp_dir, folder + ext)
        download_file(url, archive)
        extract_archive(archive, dest)
        flatten_single_subdir(dest)
        os.remove(archive)

    # Pico SDK
    sdk_dest = os.path.join(TOOLCHAIN_DIR, "pico-sdk")
    existing_sdk = find_pico_sdk()
    if existing_sdk:
        print(f"\n[SKIP] Pico SDK (found at {existing_sdk})")
    else:
        print(f"\n[INSTALL] Pico SDK {PICO_SDK_VERSION}")
        archive = os.path.join(tmp_dir, "pico-sdk.zip")
        download_file(PICO_SDK_URL, archive)
        extract_archive(archive, sdk_dest)
        flatten_single_subdir(sdk_dest)
        os.remove(archive)
        # Initialize tinyusb submodule (required for USB)
        tinyusb_dir = os.path.join(sdk_dest, "lib", "tinyusb")
        if not os.listdir(tinyusb_dir) if os.path.isdir(tinyusb_dir) else True:
            print("  Initializing TinyUSB submodule...")
            subprocess.run(
                ["git", "submodule", "update", "--init", "lib/tinyusb"],
                cwd=sdk_dest,
            )

    # Cleanup
    if os.path.isdir(tmp_dir):
        shutil.rmtree(tmp_dir)

    print("\n" + "=" * 50)
    print("Setup complete! Run 'pmk doctor' to verify.")
    print("=" * 50)


def cmd_list(_args):
    """List available boards."""
    if not os.path.isdir(BOARDS_DIR):
        print("No boards directory found.")
        return
    boards = sorted(
        d for d in os.listdir(BOARDS_DIR)
        if os.path.isdir(os.path.join(BOARDS_DIR, d))
        and not d.startswith(".")
    )
    if not boards:
        print("No boards found. Create one with: pmk new -kb <name>")
        return
    print("Available boards:")
    for b in boards:
        uf2 = find_uf2(get_build_dir(os.path.join(BOARDS_DIR, b)))
        status = " (built)" if uf2 else ""
        print(f"  {b}{status}")


def cmd_doctor(_args):
    """Check all toolchain dependencies."""
    all_ok = True

    def check(name, path, required=True):
        nonlocal all_ok
        if path:
            print(f"  [OK]   {name} -> {path}")
        else:
            tag = "MISS" if required else "SKIP"
            print(f"  [{tag}] {name}")
            if required:
                all_ok = False

    print("Checking toolchain:\n")

    check("cmake", find_tool("cmake"))
    check("ninja", find_tool("ninja"))
    check("arm-none-eabi-gcc", find_tool("arm-none-eabi-gcc"))
    check("Pico SDK", find_pico_sdk())
    check("picotool", find_tool("picotool", PICOTOOL_SEARCH_PATHS), required=False)

    print()
    if all_ok:
        print("All required tools found. Ready to build!")
    else:
        print("Missing tools. Run 'pmk setup' to install them automatically.")


def cmake_configure(board_dir, build_dir):
    """Run cmake configure step."""
    cmake = find_tool("cmake")
    if not cmake:
        print("Error: cmake not found. Run 'pmk setup' to install.")
        sys.exit(1)

    if not find_tool("ninja"):
        print("Error: ninja not found. Run 'pmk setup' to install.")
        sys.exit(1)

    os.makedirs(build_dir, exist_ok=True)

    cmd = [cmake, "-G", "Ninja", "-S", board_dir, "-B", build_dir]

    # Auto-detect Pico SDK
    sdk_path = find_pico_sdk()
    if sdk_path:
        cmd.append(f"-DPICO_SDK_PATH={sdk_path}")
    else:
        print("Warning: Pico SDK not found. Run 'pmk setup'.")

    # Ensure arm-gcc is in PATH for cmake to find
    env = os.environ.copy()
    extra_path = os.pathsep.join(toolchain_bin_dirs())
    if extra_path:
        env["PATH"] = extra_path + os.pathsep + env.get("PATH", "")

    board_name = os.path.basename(board_dir)
    print(f"Configuring {board_name}...")
    result = subprocess.run(cmd, cwd=board_dir, env=env)
    if result.returncode != 0:
        print("\nCMake configure failed.")
        sys.exit(1)


def ninja_build(build_dir):
    """Run ninja build."""
    ninja = find_tool("ninja")
    if not ninja:
        print("Error: ninja not found. Run 'pmk setup' to install.")
        sys.exit(1)

    # Ensure arm-gcc is in PATH for ninja to find
    env = os.environ.copy()
    extra_path = os.pathsep.join(toolchain_bin_dirs())
    if extra_path:
        env["PATH"] = extra_path + os.pathsep + env.get("PATH", "")

    print("Building...")
    result = subprocess.run([ninja, "-C", build_dir], env=env)
    if result.returncode != 0:
        print("\nBuild failed.")
        sys.exit(1)


def cmd_build(args):
    """Build firmware for a board."""
    board_dir = get_board_dir(args.kb)
    build_dir = get_build_dir(board_dir)

    # Reconfigure if no build.ninja exists
    if not os.path.isfile(os.path.join(build_dir, "build.ninja")):
        cmake_configure(board_dir, build_dir)

    ninja_build(build_dir)

    uf2 = find_uf2(build_dir)
    if uf2:
        print(f"\nFirmware ready: {os.path.relpath(uf2, SCRIPT_DIR)}")
    else:
        print("\nBuild completed but no .uf2 found.")


def cmd_flash(args):
    """Build and flash firmware."""
    cmd_build(args)

    build_dir = get_build_dir(get_board_dir(args.kb))
    uf2 = find_uf2(build_dir)
    if not uf2:
        print("Error: No .uf2 file to flash.")
        sys.exit(1)

    picotool = find_tool("picotool", PICOTOOL_SEARCH_PATHS)
    if not picotool:
        print("Error: picotool not found.")
        print(f"  Manually flash: drag {os.path.basename(uf2)} onto the RPI-RP2 drive.")
        sys.exit(1)

    print(f"Flashing {os.path.basename(uf2)}...")
    result = subprocess.run([picotool, "load", "-fx", uf2])
    if result.returncode != 0:
        print("Flash failed. Is the device connected?")
        sys.exit(1)
    print("Flashed and running.")


def cmd_new(args):
    """Create a new board from the template."""
    name = args.kb
    dest = os.path.join(BOARDS_DIR, name)

    if os.path.exists(dest):
        print(f"Error: Board '{name}' already exists.")
        sys.exit(1)

    template = os.path.join(BOARDS_DIR, TEMPLATE_BOARD)
    if not os.path.isdir(template):
        print(f"Error: Template board '{TEMPLATE_BOARD}' not found.")
        sys.exit(1)

    shutil.copytree(template, dest, ignore=shutil.ignore_patterns("build"))

    # Update CMakeLists.txt target name
    cmake_path = os.path.join(dest, "CMakeLists.txt")
    if os.path.isfile(cmake_path):
        with open(cmake_path, "r") as f:
            content = f.read()
        content = content.replace(TEMPLATE_BOARD, name)
        with open(cmake_path, "w") as f:
            f.write(content)

    print(f"Created board '{name}' at boards/{name}/\n")
    print("Next steps:")
    print(f"  1. Edit boards/{name}/config.h         — pins, features, sensors")
    print(f"  2. Edit boards/{name}/pmk_keymap.h — MUX channel wiring")
    print(f"  3. Edit boards/{name}/keymap.c          — default keycodes")
    print(f"  4. pmk build -kb {name}")


def cmd_clean(args):
    """Remove build directory."""
    board_dir = get_board_dir(args.kb)
    build_dir = get_build_dir(board_dir)

    if os.path.isdir(build_dir):
        shutil.rmtree(build_dir)
        print(f"Cleaned build for '{args.kb}'.")
    else:
        print(f"Nothing to clean for '{args.kb}'.")


# =========================================================================
# CLI entry point
# =========================================================================

def main():
    parser = argparse.ArgumentParser(
        prog="pmk",
        description="PMK (Pico Magnetic Keyboard) build tool",
        epilog="First time? Run 'pmk setup' to install the toolchain.",
    )
    sub = parser.add_subparsers(dest="command")

    sub.add_parser("setup", help="Download and install toolchain")

    p = sub.add_parser("build", help="Build firmware for a board")
    p.add_argument("-kb", required=True, help="Board name (folder in boards/)")

    p = sub.add_parser("flash", help="Build and flash via picotool")
    p.add_argument("-kb", required=True, help="Board name")

    p = sub.add_parser("new", help="Create a new board from template")
    p.add_argument("-kb", required=True, help="New board name")

    p = sub.add_parser("clean", help="Remove build artifacts")
    p.add_argument("-kb", required=True, help="Board name")

    sub.add_parser("list", help="List available boards")
    sub.add_parser("doctor", help="Check toolchain dependencies")

    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        sys.exit(0)

    commands = {
        "setup": cmd_setup,
        "build": cmd_build,
        "flash": cmd_flash,
        "new": cmd_new,
        "clean": cmd_clean,
        "list": cmd_list,
        "doctor": cmd_doctor,
    }
    commands[args.command](args)


if __name__ == "__main__":
    main()
