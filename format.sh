#!/bin/sh

cd "$(dirname "$0")" || exit 1

find '!' -type l -and '(' -name "*.c" -or -name "*.h" ')' | xargs clang-format -i
