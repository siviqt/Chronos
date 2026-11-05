#!/usr/bin/env bash
set -euo pipefail
module=$(cd "$(dirname "$0")/.." && pwd)
output=$(mktemp -d /tmp/nokia5110-tests.XXXXXX)
cc=${CC:-cc}
flags=(-std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer)
"$cc" "${flags[@]}" -I"$module/include" "$module/src/nokia5110.c" "$module/src/canvas.c" "$module/tests/test_core.c" -o "$output/core"
"$output/core"
