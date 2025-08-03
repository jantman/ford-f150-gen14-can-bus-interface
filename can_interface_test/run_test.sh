#!/bin/bash

# CAN Interface Test Builder and Runner
# This script builds and uploads the CAN interface test to help identify
# which physical header (X1 or X2) is connected to your vehicle's CAN bus

echo "=== ESP32-CAN-X2 Interface Test ==="
echo "This test will help identify which header your CAN bus is connected to:"
echo "  X1 (CAN1): Built-in ESP32 controller (GPIO 6/7)"  
echo "  X2 (CAN2): MCP2515 external controller (SPI)"
echo ""

# Build and upload
echo "Building and uploading test..."
cd /home/jantman/GIT/ford-f150-gen14-can-bus-interface/can_interface_test
pio run --target upload --target monitor

echo ""
echo "Test completed. Check the output above for recommendations."
