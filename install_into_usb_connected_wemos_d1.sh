#!/bin/bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
python3 get-platformio.py

cd fingerprint-mqtt && $HOME/.platformio/penv/bin/platformio run -t upload
