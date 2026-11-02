#!/usr/bin/env bash
# Prepare a Raspberry Pi running the WANDA software.
#
# This script expects the repository at /home/lti/Wanda and is intended to be
# run by the lti user. It installs dependencies and service units, but does not
# enable control or data-acquisition services.

set -euo pipefail

readonly WANDA_USER="lti"
readonly WANDA_HOME="/home/${WANDA_USER}"
readonly WANDA_DIR="${WANDA_HOME}/Wanda"
readonly SCRIPTS_DIR="${WANDA_DIR}/Scripts"
readonly VENV_DIR="${WANDA_HOME}/venv"
readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [[ "$(id -un)" != "${WANDA_USER}" ]]; then
    echo "Run this script as ${WANDA_USER}, not $(id -un)." >&2
    exit 1
fi

if [[ "${SCRIPT_DIR}" != "${SCRIPTS_DIR}" ]]; then
    echo "Expected this repository at ${WANDA_DIR}; found the script at ${SCRIPT_DIR}." >&2
    echo "Update the service-unit paths before using a different location." >&2
    exit 1
fi

echo "=== WANDA Pi setup ==="

echo "--- Installing system dependencies ---"
sudo apt-get update
sudo apt-get install -y \
    ca-certificates \
    curl \
    python3-dev \
    python3-pip \
    python3-venv \
    swig \
    liblgpio-dev

echo "--- Creating Python environment ---"
python3 -m venv "${VENV_DIR}"
"${VENV_DIR}/bin/python" -m pip install --upgrade pip
"${VENV_DIR}/bin/python" -m pip install -r "${WANDA_DIR}/requirements.txt"

echo "--- Installing WANDA systemd units ---"
sudo install -m 0644 "${WANDA_DIR}"/Systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now wanda_status_server.service

echo "--- Configuring SPI ---"
if [[ -f /boot/firmware/config.txt ]]; then
    BOOT_CONFIG=/boot/firmware/config.txt
elif [[ -f /boot/config.txt ]]; then
    BOOT_CONFIG=/boot/config.txt
else
    echo "Could not find Raspberry Pi boot configuration file." >&2
    exit 1
fi

sudo sed -i 's/^dtparam=spi=on/#dtparam=spi=on/' "${BOOT_CONFIG}"
if ! grep -q '^dtoverlay=spi0-0cs' "${BOOT_CONFIG}"; then
    echo 'dtoverlay=spi0-0cs' | sudo tee -a "${BOOT_CONFIG}" > /dev/null
fi

echo "=== Setup complete ==="
echo "Reboot before using the ADS1256 hardware."
