#!/usr/bin/env python3
"""
Bluetooth receiver program for Dark Knight Programmer
Receives "Boton Apretado" messages from the embedded device
"""

import asyncio
import sys
from bleak import BleakScanner, BleakClient

# Device name or partial name to look for
DEVICE_NAME = "Blinky Example"
# Service UUID - you may need to adjust this based on your GATT configuration
REPORT_BUTTON_CHARACTERISTIC_UUID = "61a885a4-41c3-60d0-9a53-6d652a70d29c"

class BluetoothReceiver:
    def __init__(self):
        self.client = None
        self.device = None
        
    async def scan_and_connect(self):
        """Scan for the device and connect to it"""
        print("Scanning for Bluetooth devices...")
        
        devices = await BleakScanner.discover(timeout=10.0)
        
        # Look for our device
        for device in devices:
            if device.name and DEVICE_NAME.lower() in device.name.lower():
                print(f"Found device: {device.name} ({device.address})")
                self.device = device
                break
        
        if not self.device:
            print(f"Device containing '{DEVICE_NAME}' not found")
            return False
            
        # Connect to the device
        try:
            self.client = BleakClient(self.device.address)
            await self.client.connect()
            print(f"Connected to {self.device.name}")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
    
    async def notification_handler(self, sender, data):
        """Handle incoming notifications"""
        try:
            # Try to decode as UTF-8, removing null terminators
            message = data.decode('utf-8').rstrip('\x00')
            print(f"Received: '{message}'")
            print(f"Raw data: {data.hex()} (length: {len(data)})")
            
            if message == "Boton Apretado":
                print("Button pressed detected!")
                # Add your custom actions here
                
        except UnicodeDecodeError:
            print(f"Received binary data: {data.hex()}")
            # Try as ASCII
            try:
                message = data.decode('ascii').rstrip('\x00')
                print(f"ASCII decoded: '{message}'")
            except UnicodeDecodeError:
                print("Could not decode as ASCII either")
    
    async def start_listening(self):
        """Start listening for notifications"""
        if not self.client or not self.client.is_connected:
            print("Not connected to device")
            return
            
        try:
            # Get all services and characteristics
            services = self.client.services
            
            # Find the correct characteristic
            characteristic_found = False
            for service in services:
                print(f"Service: {service.uuid}")
                for char in service.characteristics:
                    print(f"  Characteristic: {char.uuid} - {char.properties}")
                    
                    # Look for notification-capable characteristics
                    if "notify" in char.properties:
                        try:
                            await self.client.start_notify(char.uuid, self.notification_handler)
                            print(f"Started notifications on {char.uuid}")
                            characteristic_found = True
                        except Exception as e:
                            print(f"Could not start notifications on {char.uuid}: {e}")
            
            if not characteristic_found:
                print("No notification characteristics found")
                return
                
            print("Listening for button presses... Press Ctrl+C to stop")
            
            # Keep the program running
            while True:
                await asyncio.sleep(1)
                
        except KeyboardInterrupt:
            print("\nStopping...")
        except Exception as e:
            print(f"Error during listening: {e}")
    
    async def disconnect(self):
        """Disconnect from the device"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("Disconnected")

async def main():
    receiver = BluetoothReceiver()
    
    try:
        # Scan and connect
        if await receiver.scan_and_connect():
            # Start listening for notifications
            await receiver.start_listening()
    except KeyboardInterrupt:
        print("\nProgram interrupted")
    finally:
        await receiver.disconnect()

if __name__ == "__main__":
    print("Dark Knight Programmer - Bluetooth Message Receiver")
    print("Make sure your device is advertising and in range")
    print("=" * 50)
    
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nExiting...")
