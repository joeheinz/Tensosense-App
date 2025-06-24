#!/usr/bin/env python3
"""
Dual Bluetooth + Serial receiver for Dark Knight Programmer
Receives both Bluetooth messages and Serial/USB logs from the embedded device
"""

import asyncio
import sys
import threading
import time
from datetime import datetime
from bleak import BleakScanner, BleakClient
import serial
import serial.tools.list_ports

# Device name or partial name to look for
DEVICE_NAME = "Blinky Example"

class DualReceiver:
    def __init__(self):
        self.client = None
        self.device = None
        self.serial_port = None
        self.running = True
        
    def print_timestamped(self, source, message):
        """Print message with timestamp and source"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"[{timestamp}] {source}: {message}")
        
    def find_serial_port(self):
        """Find the embedded device serial port"""
        ports = serial.tools.list_ports.comports()
        
        self.print_timestamped("SERIAL", f"Found {len(ports)} serial ports:")
        for i, port in enumerate(ports):
            self.print_timestamped("SERIAL", f"  {i}: {port.device} - {port.description}")
            self.print_timestamped("SERIAL", f"      VID:PID = {port.vid:04X}:{port.pid:04X}" if port.vid else "      VID:PID = None")
        
        # Look for common Silicon Labs/EFR32/Segger devices
        for port in ports:
            description_lower = port.description.lower()
            if any(keyword in description_lower for keyword in 
                   ['silicon labs', 'j-link', 'efr32', 'jlink', 'segger', 'slab', 'cp210', 'uart']):
                self.print_timestamped("SERIAL", f"Auto-selected device port: {port.device} - {port.description}")
                return port.device
        
        # Look for specific VID/PIDs
        if ports:
            for port in ports:
                # Segger J-Link: 1366:0101, Silicon Labs: 10C4:EA60, etc.
                if port.vid in [0x1366, 0x10C4, 0x0403]:  # Common embedded device VIDs
                    self.print_timestamped("SERIAL", f"Selected by VID: {port.device} - {port.description}")
                    return port.device
            
            # If nothing specific found, ask user to choose
            self.print_timestamped("SERIAL", "No auto-detected device. Please manually select:")
            self.print_timestamped("SERIAL", "Available options:")
            for i, port in enumerate(ports):
                self.print_timestamped("SERIAL", f"  {i}: {port.device}")
            
            # For now, try the first port
            selected_port = ports[0].device
            self.print_timestamped("SERIAL", f"Trying first port: {selected_port}")
            return selected_port
        
        return None
    
    def serial_reader_thread(self):
        """Thread function to read serial data"""
        port_name = self.find_serial_port()
        
        if not port_name:
            self.print_timestamped("SERIAL", "No serial ports found")
            return
            
        try:
            # Try different baud rates commonly used by Silicon Labs devices
            baud_rates = [115200, 460800, 921600, 38400, 19200, 9600]
            
            for baud in baud_rates:
                try:
                    self.print_timestamped("SERIAL", f"Trying to connect to {port_name} at {baud} baud...")
                    self.serial_port = serial.Serial(port_name, baud, timeout=1)
                    time.sleep(0.5)  # Give it time to connect
                    
                    # Try to read a line to see if we get data
                    test_line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                    if test_line:
                        self.print_timestamped("SERIAL", f"Connected successfully at {baud} baud")
                        self.print_timestamped("SERIAL", f"First line: {test_line}")
                        break
                    else:
                        self.serial_port.close()
                        
                except serial.SerialException:
                    if self.serial_port and self.serial_port.is_open:
                        self.serial_port.close()
                    continue
            
            if not self.serial_port or not self.serial_port.is_open:
                self.print_timestamped("SERIAL", "Could not establish serial connection")
                return
                
            self.print_timestamped("SERIAL", "=== SERIAL MONITOR STARTED ===")
            
            # Read serial data continuously
            while self.running:
                try:
                    if self.serial_port.in_waiting > 0:
                        line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                        if line:
                            self.print_timestamped("SERIAL", line)
                    else:
                        time.sleep(0.01)  # Small delay to prevent CPU spinning
                        
                except serial.SerialException as e:
                    self.print_timestamped("SERIAL", f"Serial error: {e}")
                    break
                except Exception as e:
                    self.print_timestamped("SERIAL", f"Unexpected error: {e}")
                    break
                    
        except Exception as e:
            self.print_timestamped("SERIAL", f"Serial setup error: {e}")
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
                self.print_timestamped("SERIAL", "Serial connection closed")

    async def scan_and_connect_bluetooth(self):
        """Scan for the device and connect to it via Bluetooth"""
        self.print_timestamped("BLE", "Scanning for Bluetooth devices...")
        
        devices = await BleakScanner.discover(timeout=10.0)
        
        # Look for our device
        for device in devices:
            if device.name and DEVICE_NAME.lower() in device.name.lower():
                self.print_timestamped("BLE", f"Found device: {device.name} ({device.address})")
                self.device = device
                break
        
        if not self.device:
            self.print_timestamped("BLE", f"Device containing '{DEVICE_NAME}' not found")
            return False
            
        # Connect to the device
        try:
            self.client = BleakClient(self.device.address)
            await self.client.connect()
            self.print_timestamped("BLE", f"Connected to {self.device.name}")
            return True
        except Exception as e:
            self.print_timestamped("BLE", f"Failed to connect: {e}")
            return False
    
    async def bluetooth_notification_handler(self, sender, data):
        """Handle incoming Bluetooth notifications"""
        try:
            # Try to decode as UTF-8, removing null terminators
            message = data.decode('utf-8').rstrip('\x00')
            self.print_timestamped("BLE", f"Received: '{message}'")
            self.print_timestamped("BLE", f"Raw data: {data.hex()} (length: {len(data)})")
            
            if message == "Boton Apretado":
                self.print_timestamped("BLE", "*** BUTTON PRESSED DETECTED! ***")
                
        except UnicodeDecodeError:
            self.print_timestamped("BLE", f"Received binary data: {data.hex()}")
            # Try as ASCII
            try:
                message = data.decode('ascii').rstrip('\x00')
                self.print_timestamped("BLE", f"ASCII decoded: '{message}'")
            except UnicodeDecodeError:
                self.print_timestamped("BLE", "Could not decode as ASCII either")
    
    async def start_bluetooth_listening(self):
        """Start listening for Bluetooth notifications"""
        if not self.client or not self.client.is_connected:
            self.print_timestamped("BLE", "Not connected to device")
            return
            
        try:
            # Get all services and characteristics
            services = self.client.services
            
            # Find the correct characteristic
            characteristic_found = False
            for service in services:
                self.print_timestamped("BLE", f"Service: {service.uuid}")
                for char in service.characteristics:
                    self.print_timestamped("BLE", f"  Characteristic: {char.uuid} - {char.properties}")
                    
                    # Look for notification-capable characteristics
                    if "notify" in char.properties:
                        try:
                            await self.client.start_notify(char.uuid, self.bluetooth_notification_handler)
                            self.print_timestamped("BLE", f"Started notifications on {char.uuid}")
                            characteristic_found = True
                        except Exception as e:
                            self.print_timestamped("BLE", f"Could not start notifications on {char.uuid}: {e}")
            
            if not characteristic_found:
                self.print_timestamped("BLE", "No notification characteristics found")
                return
                
            self.print_timestamped("BLE", "=== BLUETOOTH MONITOR STARTED ===")
            
            # Keep the program running
            while self.running:
                await asyncio.sleep(1)
                
        except Exception as e:
            self.print_timestamped("BLE", f"Error during listening: {e}")
    
    async def disconnect_bluetooth(self):
        """Disconnect from the Bluetooth device"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            self.print_timestamped("BLE", "Bluetooth disconnected")
    
    def stop(self):
        """Stop both receivers"""
        self.running = False

async def main():
    receiver = DualReceiver()
    
    print("=" * 60)
    print("Dark Knight Programmer - Dual Monitor (Bluetooth + Serial)")
    print("Shows both Bluetooth messages and Serial/USB debug logs")
    print("=" * 60)
    
    try:
        # Start serial reader in a separate thread
        serial_thread = threading.Thread(target=receiver.serial_reader_thread)
        serial_thread.daemon = True
        serial_thread.start()
        
        # Give serial thread a moment to start
        await asyncio.sleep(1)
        
        # Scan and connect to Bluetooth
        if await receiver.scan_and_connect_bluetooth():
            # Start listening for Bluetooth notifications
            await receiver.start_bluetooth_listening()
            
    except KeyboardInterrupt:
        print("\n" + "=" * 60)
        print("Program interrupted by user")
    except Exception as e:
        print(f"\nUnexpected error: {e}")
    finally:
        receiver.stop()
        await receiver.disconnect_bluetooth()
        print("All connections closed. Exiting...")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nExiting...")