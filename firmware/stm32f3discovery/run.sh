#!/bin/bash
# badprog.com
# Build, flash and monitor script for STM32F3Discovery

set -e

TOOLCHAIN="cmake/arm-none-eabi.cmake"
BUILD_DIR="build"

usage() {
    echo "Usage: $0 [build|flash|monitor|all]"
    echo ""
    echo "  build   - configure and build the firmware"
    echo "  flash   - flash the firmware on the board"
    echo "  monitor - open OpenOCD semihosting console"
    echo "  all     - build + flash + monitor"
    exit 1
}

build() {
    echo "--- Build ---"
    rm -rf ${BUILD_DIR}
    cmake -B ${BUILD_DIR} \
        -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN} \
        -DCMAKE_BUILD_TYPE=Debug \
        -G Ninja
    ninja -C ${BUILD_DIR}
}

flash() {
    echo "--- Flash ---"
    sudo chmod 666 /dev/bus/usb/001/* 2>/dev/null || true
    openocd -f interface/stlink.cfg \
            -f target/stm32f3x.cfg \
            -c "program ${BUILD_DIR}/stm32f3discovery.bin verify reset exit 0x08000000"
}

monitor() {
    echo "--- Semihosting monitor ---"
    echo "Press Ctrl+C to quit"
    # sudo chmod 666 /dev/bus/usb/001/* 2>/dev/null || true
    openocd -f interface/stlink.cfg \
            -f target/stm32f3x.cfg \
            -c "init" \
            -c "arm semihosting enable" \
            -c "reset run"
}

case "$1" in
    build)   build ;;
    flash)   flash ;;
    monitor) monitor ;;
    all)     build && flash && monitor ;;
    *)       usage ;;
esac