#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-function \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    tests/power-policy.c -o build/tests/power-policy
build/tests/power-policy
aarch64-linux-gnu-gcc -std=gnu11 -O2 -static -Wall -Wextra -Werror \
    -Wno-unused-function tests/power-policy.c -o build/tests/power-policy-arm64
qemu-aarch64 build/tests/power-policy-arm64
