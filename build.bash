#!/usr/bin/env bash
set -e

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target shared --parallel 2
cmake --build build --target test   --parallel 2
