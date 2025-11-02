#!/bin/bash

MAC="80:96:98:C8:69:17"

# Start bluetoothctl in background and send commands
{
    sleep 1
    echo "power on"
    sleep 1
    echo "agent NoInputNoOutput"
    sleep 1
    echo "default-agent"
    sleep 1
    echo "discoverable on"
    sleep 1
    echo "pairable on"
    sleep 2
    echo "trust $MAC"
    sleep 1
    echo "pair $MAC"
    sleep 5
    echo "connect $MAC"
    sleep 2
    echo "info $MAC"
    sleep 2
    echo "quit"
} | bluetoothctl
