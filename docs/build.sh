#!/usr/bin/env sh
set -eu

curl -fsSLO https://github.com/doxygen/doxygen/releases/download/Release_1_14_0/doxygen-1.14.0.linux.bin.tar.gz
tar -xzf doxygen-1.14.0.linux.bin.tar.gz
curl -fsSL https://docs.nsmbu.net/sys/Sys.tag -o Sys.tag
./doxygen-1.14.0/bin/doxygen ./docs/Doxyfile
