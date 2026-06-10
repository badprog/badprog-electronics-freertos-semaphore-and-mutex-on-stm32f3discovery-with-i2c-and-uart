# badprog.com
FROM ubuntu:24.04

ARG USERNAME=badprog
ARG USER_ID=1000
ARG USER_GID=${USER_ID}
ARG ARM_TOOLCHAIN_VERSION=13.3.rel1

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    tree \
    cmake \
    ninja-build \
    git \
    curl \
    ca-certificates \
    xz-utils \
    usbutils \
    libusb-1.0-0 \
    gdb-multiarch \
    openocd \
    sudo \
    minicom \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Official ARM GNU Toolchain (includes newlib — required for system headers on bare metal)
# The apt package gcc-arm-none-eabi ships without newlib on Ubuntu 24.04
RUN curl -Lo /tmp/arm-toolchain.tar.xz \
    "https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_TOOLCHAIN_VERSION}/binrel/arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi.tar.xz" \
    && tar -xf /tmp/arm-toolchain.tar.xz -C /opt \
    && rm /tmp/arm-toolchain.tar.xz

ENV PATH="/opt/arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi/bin:${PATH}"

# FreeRTOS Kernel V11.1.0
# Stored in /opt so all projects can reference it without duplicating sources
# Use ENV FREERTOS_DIR in CMakeLists.txt to locate the kernel
RUN curl -Lo /tmp/freertos.tar.gz \
    "https://github.com/FreeRTOS/FreeRTOS-Kernel/archive/refs/tags/V11.3.0.tar.gz" \
    && tar -xzf /tmp/freertos.tar.gz -C /opt \
    && rm /tmp/freertos.tar.gz

ENV FREERTOS_DIR=/opt/FreeRTOS-Kernel-11.3.0

# udev rule for ST-Link V2 (idProduct 3748 — STM32F3Discovery)
# udev does not run inside Docker; the rule is applied at runtime via postStartCommand
RUN mkdir -p /etc/udev/rules.d \
    && echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="3748", MODE="0666"' \
    > /etc/udev/rules.d/99-stlink.rules

# Ubuntu 24.04 ships with a user already at UID/GID 1000 — rename instead of recreate
RUN mkdir -p /etc/sudoers.d \
    && if getent group ${USER_GID} >/dev/null; then \
        groupmod -n ${USERNAME} ubuntu; \
    else \
        groupadd --gid ${USER_GID} ${USERNAME}; \
    fi \
    && if getent passwd ${USER_ID} >/dev/null; then \
        usermod -d /home/${USERNAME} -m -l ${USERNAME} ubuntu; \
    else \
        useradd --uid ${USER_ID} --gid ${USER_GID} -m ${USERNAME} -s /bin/bash; \
    fi \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}

USER ${USERNAME}
ENV HOME=/home/${USERNAME}

WORKDIR /workspaces
