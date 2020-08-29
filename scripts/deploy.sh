#!/bin/bash

set -e
set -x

if [[ "x${PORT}" == "x" ]]; then
    PORT=22
fi

UEFISTORE=/usr/sbin/uefistored
scp -P${PORT} uefistored root@${XCP_NG_IP}:${UEFISTORE}
