#!/bin/bash

# Get the directory where the script is located (i.e., cross_common)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Define the main working directory (parent of the script directory)
MAIN_WORK_DIR="$(dirname "$SCRIPT_DIR")"

# Define the target Buildroot directory
BUILDROOT_DIR="${MAIN_WORK_DIR}/rd3588_buildroot/buildroot"

# Define the source library and target path
SOURCE_LIB="${BUILDROOT_DIR}/output/rockchip_rk3588/build/lib-rtsdk-1.0/librtsdk.a"
TARGET_DIR="${SCRIPT_DIR}"

echo "=========================================="
echo "  Starting RTOnBoot SDK Update (lib-rtsdk)..."
echo "=========================================="

# 1. Enter the Buildroot directory
echo "[1/4] Entering Buildroot directory: ${BUILDROOT_DIR}"
cd "$BUILDROOT_DIR" || { echo "Error: Unable to enter the Buildroot directory. Please check the path!"; exit 1; }

# 2. Execute clean operation
echo "[2/4] Executing make lib-rtsdk-dirclean ..."
make lib-rtsdk-dirclean
if [ $? -ne 0 ]; then
    echo "Error: Failed to clean lib-rtsdk!"
    cd "$SCRIPT_DIR"
    exit 1
fi

# 3. Execute rebuild
echo "[3/4] Executing make lib-rtsdk ..."
make lib-rtsdk
if [ $? -ne 0 ]; then
    echo "Error: Failed to build lib-rtsdk!"
    cd "$SCRIPT_DIR"
    exit 1
fi

# 4. Copy the generated library file to the cross_common directory
echo "[4/4] Copying librtsdk.a to ${TARGET_DIR} ..."
if [ -f "$SOURCE_LIB" ]; then
    cp "$SOURCE_LIB" "$TARGET_DIR/"
    if [ $? -eq 0 ]; then
        echo "Success: librtsdk.a has been updated to the cross_common directory."
    else
        echo "Error: Failed to copy the file!"
    fi
else
    echo "Error: Generated library file not found at ${SOURCE_LIB}. Please check the build output!"
fi

# 5. Return to the script directory
echo "=========================================="
echo "  Returning to script directory: ${SCRIPT_DIR}"
echo "=========================================="
cd "$SCRIPT_DIR"

exit 0
