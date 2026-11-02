#!/usr/bin/env bash
# Create and start the Grafana container for the designated TURNER Grafana host.
# Run after turner_setup.sh, as the lti user.

set -euo pipefail

readonly TURNER_USER="lti"
readonly TURNER_HOME="/home/${TURNER_USER}"
readonly TURNER_DIR="${TURNER_HOME}/Turner"
readonly SCRIPTS_DIR="${TURNER_DIR}/Scripts"
readonly CONTAINER_NAME="lti-grafana-1"
readonly VOLUME_NAME="lti-grafana-data"
readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

if [[ "$(id -un)" != "${TURNER_USER}" ]]; then
    echo "Run this script as ${TURNER_USER}, not $(id -un)." >&2
    exit 1
fi

if [[ "${SCRIPT_DIR}" != "${SCRIPTS_DIR}" ]]; then
    echo "Expected this repository at ${TURNER_DIR}; found the script at ${SCRIPT_DIR}." >&2
    exit 1
fi

if ! command -v docker > /dev/null; then
    echo "Docker is not installed. Run turner_pi_setup.sh first." >&2
    exit 1
fi

if [[ -z "${GRAFANA_ADMIN_PASSWORD:-}" ]]; then
    read -r -s -p "Grafana admin password: " GRAFANA_ADMIN_PASSWORD
    echo
    read -r -s -p "Confirm Grafana admin password: " GRAFANA_ADMIN_PASSWORD_CONFIRM
    echo
    if [[ "${GRAFANA_ADMIN_PASSWORD}" != "${GRAFANA_ADMIN_PASSWORD_CONFIRM}" ]]; then
        echo "Passwords do not match." >&2
        exit 1
    fi
fi

if [[ -z "${GRAFANA_ADMIN_PASSWORD}" ]]; then
    echo "Grafana admin password cannot be empty." >&2
    exit 1
fi

echo "--- Ensuring Docker is available ---"
sudo systemctl enable --now docker.service

if sudo docker container inspect "${CONTAINER_NAME}" > /dev/null 2>&1; then
    echo "Grafana container '${CONTAINER_NAME}' already exists; leaving it unchanged."
else
    echo "--- Creating Grafana container ---"
    sudo docker volume create "${VOLUME_NAME}" > /dev/null
    sudo docker create \
        --name "${CONTAINER_NAME}" \
        --publish 3000:3000 \
        --volume "${VOLUME_NAME}:/var/lib/grafana" \
        --env GF_SECURITY_ADMIN_USER=admin \
        --env "GF_SECURITY_ADMIN_PASSWORD=${GRAFANA_ADMIN_PASSWORD}" \
        grafana/grafana-oss:latest > /dev/null
fi

echo "--- Starting Grafana through systemd ---"
sudo systemctl daemon-reload
if [[ "$(sudo docker inspect --format '{{.State.Running}}' "${CONTAINER_NAME}")" == "true" ]]; then
    sudo systemctl enable grafana.service
else
    sudo systemctl enable --now grafana.service
fi

echo "Waiting for Grafana..."
for _ in {1..60}; do
    if curl --fail --silent http://127.0.0.1:3000/api/health > /dev/null; then
        echo "Grafana is ready at http://$(hostname -I | awk '{print $1}'):3000"
        echo "Create a Grafana service-account token and place it in the required grafana.key files."
        exit 0
    fi
    sleep 1
done

echo "Grafana did not become healthy within 60 seconds." >&2
echo "Inspect it with: sudo journalctl -u grafana.service -f" >&2
exit 1
