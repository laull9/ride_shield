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

current_ubuntu_codename() {
    if [[ ! -r /etc/os-release ]]; then
        return 1
    fi

    # shellcheck disable=SC1091
    . /etc/os-release
    if [[ "${ID:-}" != "ubuntu" || -z "${VERSION_CODENAME:-}" ]]; then
        return 1
    fi

    printf '%s\n' "${VERSION_CODENAME}"
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

check_apt_source_consistency() {
    if [[ "${RIDESHIELD_SKIP_APT_SOURCE_CHECK:-0}" == "1" ]]; then
        return
    fi

    local current_codename
    if ! current_codename="$(current_ubuntu_codename)"; then
        return
    fi

    local mismatches=()
    local wrong_arm64=()
    local file enabled uris architectures line suite base_suite

    shopt -s nullglob
    for file in /etc/apt/sources.list.d/*.sources; do
        enabled=1
        uris=""
        architectures=""

        while IFS= read -r line || [[ -n "${line}" ]]; do
            case "${line}" in
                Enabled:*)
                    if [[ "${line#Enabled: }" == "no" ]]; then
                        enabled=0
                    fi
                    ;;
                URIs:*)
                    uris="${line#URIs: }"
                    ;;
                Architectures:*)
                    architectures="${line#Architectures: }"
                    ;;
                Suites:*)
                    if [[ ${enabled} -eq 0 ]]; then
                        continue
                    fi

                    for suite in ${line#Suites: }; do
                        base_suite="${suite%%-*}"

                        if [[ "${uris}" == *ubuntu* || "${uris}" == *launchpadcontent.net* ]]; then
                            if [[ "${base_suite}" != "${current_codename}" ]]; then
                                mismatches+=("${file}: ${suite} (${uris})")
                            fi
                        fi
                    done
                    ;;
            esac
        done < "${file}"

        if [[ ${enabled} -eq 1 ]] && [[ " ${architectures} " == *" arm64 "* ]]; then
            if [[ "${uris}" == *archive.ubuntu.com/ubuntu* || "${uris}" == *security.ubuntu.com/ubuntu* ]]; then
                wrong_arm64+=("${file}: arm64 should use http://ports.ubuntu.com/ubuntu-ports/ instead of ${uris}")
            fi
        fi
    done
    shopt -u nullglob

    if [[ ${#mismatches[@]} -eq 0 && ${#wrong_arm64[@]} -eq 0 ]]; then
        return
    fi

    warn "Detected inconsistent apt source configuration for Ubuntu ${current_codename}."

    if [[ ${#mismatches[@]} -gt 0 ]]; then
        warn "Enabled Ubuntu or Launchpad sources whose suite does not match ${current_codename}:"
        printf '  %s\n' "${mismatches[@]}" >&2
    fi

    if [[ ${#wrong_arm64[@]} -gt 0 ]]; then
        warn "Detected arm64 Ubuntu entries that do not use ubuntu-ports:"
        printf '  %s\n' "${wrong_arm64[@]}" >&2
    fi

    warn "Expected Ubuntu sources for this machine should use suites based on ${current_codename}."
    warn "For Ubuntu main archives, amd64/i386 should use archive.ubuntu.com + security.ubuntu.com, and arm64 should use ports.ubuntu.com/ubuntu-ports."
    warn "Disable or rename any enabled '*-noble.sources' PPAs on a ${current_codename} system before retrying."
    warn "Fix the apt sources before running this installer."
    warn "Set RIDESHIELD_SKIP_APT_SOURCE_CHECK=1 only if you have intentionally configured mixed sources."
    exit 1
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
    check_apt_source_consistency

    local packages=(
        build-essential
        cmake
        ninja-build
        pkg-config
        git
        curl
        ca-certificates
        libopencv-dev
        qt6-declarative-dev
        qt6-shadertools-dev
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
        qt6-qtdeclarative-devel
        qt6-qtquickcontrols2-devel
        qt6-qtshadertools-devel
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
        qt6-declarative
        qt6-quickcontrols2
        qt6-shadertools
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
        qt6-declarative-devel
        qt6-quickcontrols2-devel
        qt6-shadertools-devel
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