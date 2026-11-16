#!/bin/bash
# Script to check C++ code formatting locally
# Usage: ./scripts/check-clang-format.sh

set -euo pipefail

CLANG_FORMAT_VERSION="22"
CLANG_FORMAT_CMD="clang-format-${CLANG_FORMAT_VERSION}"

# Check if clang-format is available
if ! command -v "$CLANG_FORMAT_CMD" &> /dev/null; then
    echo "Error: $CLANG_FORMAT_CMD not found."
    echo "Install it with: sudo apt-get install -y clang-format-${CLANG_FORMAT_VERSION}"
    exit 1
fi

echo "Using $($CLANG_FORMAT_CMD --version)"

# Find all source directories
dirs=()
for d in operators applications tests src; do
    if [ -d "$d" ]; then
        dirs+=("$d")
    fi
done

if [ ${#dirs[@]} -eq 0 ]; then
    echo "No source directories found for clang-format check"
    exit 0
fi

# Run clang-format check
echo "Checking C++ formatting in: ${dirs[*]}"
find "${dirs[@]}" -type f \( -name '*.cpp' -o -name '*.hpp' \) ! -path 'src/version.hpp' -print0 \
    | xargs -0 -r "$CLANG_FORMAT_CMD" --dry-run --Werror

echo "✓ All files are properly formatted!"
