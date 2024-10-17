from pathlib import Path
import posixpath
import os

# For more details on these options, run 'fbt -h'

FIRMWARE_ORIGIN = "Momentum"

# Default hardware target
TARGET_HW = 7

# Optimization flags
## Optimize for size
COMPACT = 1
## Optimize for debugging experience
DEBUG = 0

# Suffix to add to files when building distribution
# If OS environment has DIST_SUFFIX set, it will be used instead

if not os.environ.get("DIST_SUFFIX"):
    # Check scripts/get_env.py to mirror CI naming
    def git(*args):
        import subprocess

        return (
            subprocess.check_output(["git", *args], stderr=subprocess.DEVNULL)
            .decode()
            .strip()
        )

    try:
        # For tags, dist name is just the tag name: mntm-(ver)
        DIST_SUFFIX = git("describe", "--tags", "--abbrev=0", "--exact-match")
    except Exception:
        # If not a tag, dist name is: mntm-(branch)-(commmit)
        branch_name = git("rev-parse", "--abbrev-ref", "HEAD").removeprefix("mntm-")
        commit_sha = git("rev-parse", "HEAD")[:8]
        DIST_SUFFIX = f"mntm-{branch_name}-{commit_sha}"
    # Dist name is only for naming of output files
    DIST_SUFFIX = DIST_SUFFIX.replace("/", "-")
    # Instead, FW version uses tag name (mntm-xxx), or "mntm-dev" if not a tag (see scripts/version.py)
    # You can get commit and branch info in firmware with appropriate version_get_*() calls

# Skip external apps by default
SKIP_EXTERNAL = False

# Appid's to include even when skipping externals
EXTRA_EXT_APPS = []

# Coprocessor firmware
COPRO_OB_DATA = "scripts/ob.data"

# Must match lib/stm32wb_copro version
COPRO_CUBE_VERSION = "1.20.0"

COPRO_CUBE_DIR = "lib/stm32wb_copro"

# Default radio stack
COPRO_STACK_BIN = "stm32wb5x_BLE_Stack_light_fw.bin"
# Firmware also supports "ble_full", but it might not fit into debug builds
COPRO_STACK_TYPE = "ble_light"

# Leave 0 to let scripts automatically calculate it
COPRO_STACK_ADDR = "0x0"

# If you override COPRO_CUBE_DIR on commandline, override this as well
COPRO_STACK_BIN_DIR = posixpath.join(COPRO_CUBE_DIR, "firmware")

# Supported toolchain versions
# Also specify in scripts/ufbt/SConstruct
FBT_TOOLCHAIN_VERSIONS = (" 12.3.", " 13.2.")

OPENOCD_OPTS = [
    "-f",
    "interface/stlink.cfg",
    "-c",
    "transport select hla_swd",
    "-f",
    "${FBT_DEBUG_DIR}/stm32wbx.cfg",
    "-c",
    "stm32wbx.cpu configure -rtos auto",
]

SVD_FILE = "${FBT_DEBUG_DIR}/STM32WB55_CM4.svd"

# Look for blackmagic probe on serial ports and local network
BLACKMAGIC = "auto"

# Application to start on boot
LOADER_AUTOSTART = ""

FIRMWARE_APPS = {
    "default": [
        # Svc
        "basic_services",
        # Apps
        "main_apps",
        "system_apps",
        # Settings
        "settings_apps",
    ],
    "unit_tests": [
        # Svc
        "basic_services",
        # Apps
        "main_apps",
        "system_apps",
        # Settings
        "settings_apps",
        # Tests
        "unit_tests",
    ],
    "unit_tests_min": [
        "basic_services",
        "updater_app",
        "radio_device_cc1101_ext",
        "unit_tests",
        "js_app",
    ],
}

FIRMWARE_APP_SET = "default"

custom_options_fn = "fbt_options_local.py"

if Path(custom_options_fn).exists():
    exec(compile(Path(custom_options_fn).read_text(), custom_options_fn, "exec"))
