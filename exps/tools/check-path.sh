#!/usr/bin/env bash

# Copyright (c) 2022-2024, Arm Limited.
#
# SPDX-License-Identifier: Apache-2.0

set -e

check_vpp()
{
    echo "Checking VPP binary path..."
    #vpp_binary="/usr/local/bin/vpp"

    if ! [[ $(command -v "${vpp_binary}") ]]; then
          echo
          echo "Can't find vpp at: ${vpp_binary}"
          echo
          exit 1
    fi

    echo "Found VPP binary at: $(command -v "${vpp_binary}")"
}

check_vppctl()
{
    echo "Checking VPPCTL binary path..."
    #vppctl_binary="/usr/local/bin/vppctl"

    if ! [[ $(command -v "${vppctl_binary}") ]]; then
          echo
          echo "Can't find vppctl at: ${vppctl_binary}"
          echo
          exit 1
    fi

    echo "Found VPPCTL binary at: $(command -v "${vppctl_binary}")"
}

check_ldp()
{
    echo "Checking libvcl_ldpreload.so path..."
    LDP_PATH="${DATAPLANE_TOP}/components/vpp/build-root/install-vpp_debug-native/vpp/lib/aarch64-linux-gnu/libvcl_ldpreload.so"

    if ! [[ -e ${LDP_PATH} ]]; then
          echo
          echo "Can't find libvcl_ldpreload.so at: ${LDP_PATH}"
          echo
          exit 1
    fi

    echo "Found libvcl_ldpreload.so at: ${LDP_PATH}"
}
