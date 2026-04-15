#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"
MIN_CMAKE_VERSION="3.30.0"
INSTALL_ROS2="${INSTALL_ROS2:-1}"
ROS_DISTRO_NAME="${ROS_DISTRO:-humble}"
ROS2_PACKAGES_CONFIGURED=0

log() {
    printf '[install] %s\n' "$*"
}

warn() {
    printf '[warn] %s\n' "$*" >&2
}

have_command() {
    command -v "$1" >/dev/null 2>&1
}

version_ge() {
    local lhs="$1"
    local rhs="$2"
    [[ "$(printf '%s\n%s\n' "$rhs" "$lhs" | sort -V | tail -n 1)" == "$lhs" ]]
}

require_sudo() {
    if [[ ${EUID} -ne 0 ]] && ! have_command sudo; then
        warn "This script needs root privileges. Install sudo or run as root."
        exit 1
    fi
}

run_as_root() {
    if [[ ${EUID} -eq 0 ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

apt_has_package() {
    apt-cache show "$1" >/dev/null 2>&1
}

dnf_has_package() {
    dnf list --available "$1" >/dev/null 2>&1
}

pacman_has_package() {
    pacman -Si "$1" >/dev/null 2>&1
}

zypper_has_package() {
    zypper --non-interactive search --match-exact "$1" >/dev/null 2>&1
}

append_ros2_apt_packages() {
    local -n package_ref="$1"
    local ros_packages=(
        "ros-${ROS_DISTRO_NAME}-rclcpp"
        "ros-${ROS_DISTRO_NAME}-sensor-msgs"
        "ros-${ROS_DISTRO_NAME}-std-msgs"
    )

    if [[ "${INSTALL_ROS2}" != "1" ]]; then
        return
    fi

    if apt_has_package "${ros_packages[0]}"; then
        package_ref+=("${ros_packages[@]}")
        if apt_has_package ros-dev-tools; then
            package_ref+=(ros-dev-tools)
        fi
        ROS2_PACKAGES_CONFIGURED=1
    else
        warn "ROS2 packages for distro ${ROS_DISTRO_NAME} were not found in the current apt sources."
        warn "If you need ROS2, add the ROS apt repository first, then rerun with ROS_DISTRO=${ROS_DISTRO_NAME}."
    fi
}

warn_ros2_unavailable() {
    if [[ "${INSTALL_ROS2}" == "1" ]]; then
        warn "Automatic ROS2 package installation is only configured for apt-based systems in this script."
        warn "Install ROS2 ${ROS_DISTRO_NAME} manually for this distribution if you need ROS support."
    fi
}

install_with_apt() {
    require_sudo

    local packages=(
        build-essential
        cmake
        ninja-build
        pkg-config
        git
        curl
        ca-certificates
        libopencv-dev
    )

    if apt_has_package libonnxruntime-dev; then
        packages+=(libonnxruntime-dev)
    else
        warn "Package libonnxruntime-dev is not available in the current apt sources."
        warn "You can still build with source dependencies via CMake preset host-debug-source-deps."
    fi

    append_ros2_apt_packages packages

    log "Updating apt package index"
    run_as_root apt-get update

    log "Installing packages: ${packages[*]}"
    run_as_root apt-get install -y "${packages[@]}"
}

install_with_dnf() {
    require_sudo

    local packages=(
        gcc-c++
        gcc
        make
        cmake
        ninja-build
        pkgconf-pkg-config
        git
        curl
        ca-certificates
        opencv-devel
    )

    if dnf_has_package onnxruntime-devel; then
        packages+=(onnxruntime-devel)
    else
        warn "Package onnxruntime-devel is not available in the current dnf repositories."
        warn "You can still build with source dependencies via CMake preset host-debug-source-deps."
    fi

    warn_ros2_unavailable

    log "Installing packages: ${packages[*]}"
    run_as_root dnf install -y "${packages[@]}"
}

install_with_pacman() {
    require_sudo

    local packages=(
        base-devel
        cmake
        ninja
        pkgconf
        git
        curl
        ca-certificates
        opencv
    )

    if pacman_has_package onnxruntime; then
        packages+=(onnxruntime)
    else
        warn "Package onnxruntime is not available in the current pacman repositories."
        warn "You can still build with source dependencies via CMake preset host-debug-source-deps."
    fi

    warn_ros2_unavailable

    log "Refreshing pacman database"
    run_as_root pacman -Sy --noconfirm

    log "Installing packages: ${packages[*]}"
    run_as_root pacman -S --needed --noconfirm "${packages[@]}"
}

install_with_zypper() {
    require_sudo

    local packages=(
        gcc-c++
        gcc
        make
        cmake
        ninja
        pkg-config
        git
        curl
        ca-certificates
        opencv-devel
    )

    if zypper_has_package onnxruntime-devel; then
        packages+=(onnxruntime-devel)
    else
        warn "Package onnxruntime-devel is not available in the current zypper repositories."
        warn "You can still build with source dependencies via CMake preset host-debug-source-deps."
    fi

    warn_ros2_unavailable

    log "Refreshing zypper repositories"
    run_as_root zypper --non-interactive refresh

    log "Installing packages: ${packages[*]}"
    run_as_root zypper --non-interactive install --no-recommends "${packages[@]}"
}

check_cmake_version() {
    if ! have_command cmake; then
        warn "cmake is not installed after package setup."
        return
    fi

    local installed_version
    installed_version="$(cmake --version | awk 'NR==1 {print $3}')"

    if version_ge "$installed_version" "$MIN_CMAKE_VERSION"; then
        log "cmake ${installed_version} satisfies the minimum required version ${MIN_CMAKE_VERSION}"
    else
        warn "cmake ${installed_version} is older than the project's minimum ${MIN_CMAKE_VERSION}"
        warn "Use Kitware's apt repo or install a newer cmake manually if configure fails."
    fi
}

print_next_steps() {
    cat <<EOF

Next steps:
  cd "${PROJECT_ROOT}"
EOF

        if [[ "${ROS2_PACKAGES_CONFIGURED}" == "1" ]]; then
                cat <<EOF
    source /opt/ros/${ROS_DISTRO_NAME}/setup.bash
EOF
        elif [[ "${INSTALL_ROS2}" == "1" ]]; then
                cat <<EOF
    # ROS2 packages were not installed automatically; configure ROS2 separately if needed.
EOF
        fi

        cat <<EOF
  cmake --preset host-debug
  cmake --build --preset build-host-debug

If ONNX Runtime development headers were not available from your package manager:
  cmake --preset host-debug-source-deps
  cmake --build --preset deps-build-host
  cmake --preset host-debug-source-deps
  cmake --build --preset build-host-debug
EOF
}

main() {
    if have_command apt-get; then
        install_with_apt
    elif have_command dnf; then
        install_with_dnf
    elif have_command pacman; then
        install_with_pacman
    elif have_command zypper; then
        install_with_zypper
    else
        warn "Unsupported package manager. Supported: apt, dnf, pacman, zypper."
        exit 1
    fi

    check_cmake_version
    print_next_steps
}

main "$@"