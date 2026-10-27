#!/bin/sh
# Builds the memory/CPU hog test programs (static, so they run inside Alpine)
# and installs them into rootfs/usr/local/bin.
set -e
mkdir -p /tmp/mj-build
g++ -static -o /tmp/mj-build/memhog tests/memhog.cpp
g++ -static -o /tmp/mj-build/cpuhog tests/cpuhog.cpp
sudo mkdir -p rootfs/usr/local/bin
sudo cp /tmp/mj-build/memhog /tmp/mj-build/cpuhog rootfs/usr/local/bin/
echo "test programs installed in rootfs/usr/local/bin"
