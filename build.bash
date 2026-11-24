#!/bin/bash
set -euo pipefail
set -x
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --build build --target test
