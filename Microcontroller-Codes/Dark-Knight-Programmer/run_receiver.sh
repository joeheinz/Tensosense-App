#!/bin/bash

echo "Setting up Bluetooth + Serial receiver for Dark Knight Programmer..."

# Check if Python3 is available
if ! command -v python3 &> /dev/null; then
    echo "Python3 is required but not installed."
    exit 1
fi

# Install requirements if they don't exist
if ! python3 -c "import bleak" &> /dev/null; then
    echo "Installing required packages..."
    pip3 install -r requirements.txt
fi

if ! python3 -c "import serial" &> /dev/null; then
    echo "Installing serial support..."
    pip3 install pyserial
fi

# Run the dual receiver
echo "Starting Bluetooth + Serial receiver..."
python3 dual_receiver.py