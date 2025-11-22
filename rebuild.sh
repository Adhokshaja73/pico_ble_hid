#!/bin/bash
rm -rf build
mkdir build
cd build
cmake .. -DIPCORE_BOARD=pico_w #-DUSB_CDC=ON -DCLASSIC_HID=ON
make -j$(nproc) 