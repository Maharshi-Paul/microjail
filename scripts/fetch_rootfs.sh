#!/bin/sh
set -e
BASE=https://dl-cdn.alpinelinux.org/alpine/latest-stable/releases/x86_64
FILE=$(wget -qO- $BASE/ | grep -o 'alpine-minirootfs-[0-9.]*-x86_64\.tar\.gz' | sort -uV | tail -1)
mkdir -p rootfs
wget -O /tmp/$FILE $BASE/$FILE
sudo tar xzf /tmp/$FILE -C rootfs
echo "rootfs ready: $FILE"
