#!/usr/bin/env bash
set -e

CC=clang CXX=clang++ cmake -S . -B build-termux -DCMAKE_BUILD_TYPE=Release
cmake --build build-termux --target shared --parallel 2
