#!/bin/sh

cd `dirname $0`
xasm /o:bootloader.obx /l:/tmp/bootloader.lst bootloader.asx || exit 1
xasm /o:bootloader_hs.obx /l:/tmp/bootloader_hs.lst bootloader_hs.asx || exit 1
./obx2h.py bootloader.obx bootloader> ../uc/src/bootloader.h
./obx2h.py bootloader_hs.obx bootloader_hs > ../uc/src/bootloader_hs.h
