#!/usr/bin/env bash
# Author: Raghuram Kasturi Sami
# Copyright (c) 2026 Raghuram Kasturi Sami
# One-time setup: adds the third-party dependencies as shallow git submodules pinned to
# known tags. Run it once in your repo, then commit. After that, fresh clones only need:
#   git clone --recurse-submodules <your-repo-url>
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

add_pinned() {
  local url="$1" path="$2" tag="$3"
  if [[ ! -e "$path/.git" ]]; then
    git submodule add --depth 1 "$url" "$path"
    git config -f .gitmodules "submodule.$path.shallow" true
  fi
  git -C "$path" fetch --quiet --depth 1 origin "refs/tags/$tag:refs/tags/$tag"
  git -C "$path" -c advice.detachedHead=false checkout --quiet "$tag"
  printf '  %-36s %s\n' "$path" "$tag"
}

echo "Pinning third-party dependencies:"
add_pinned https://github.com/FreeRTOS/FreeRTOS-Kernel.git \
  third_party/FreeRTOS-Kernel V11.2.0
# CMSIS Core, CMSIS Device and HAL must be a consistent set. These are the versions ST
# shipped together in STM32CubeU5 v1.4.0. Bump all three together, never one alone.
add_pinned https://github.com/ARM-software/CMSIS_5.git \
  third_party/CMSIS_5 5.9.0
add_pinned https://github.com/STMicroelectronics/cmsis_device_u5.git \
  third_party/cmsis_device_u5 v1.3.1
add_pinned https://github.com/STMicroelectronics/stm32u5xx_hal_driver.git \
  third_party/stm32u5xx_hal_driver v1.4.0

git add .gitmodules third_party
echo
echo "Done. Commit the pins with:"
echo "  git commit -m \"Pin third-party dependencies as submodules\""
