//
//  BluetoothManager.swift
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
//
import CoreBluetooth
import SwiftUI

class BluetoothManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    @Published var devices: [(CBPeripheral, String)] = []
    @Published var isConnected: Bool = false
    @Published var connectedPeripheral: CBPeripheral?
    @Published var accelerationData: [AccelerationPoint] = []
    @Published var showFallWarning = false
    @Published var showOscillationWarning = false

    private var centralManager: CBCentralManager!
    private var targetCharacteristic: CBCharacteristic?
    private let accelerationUUID = CBUUID(string: "c4c1f6e2-4be5-11e5-885d-feff819cdc9f") // ✅ UUID de aceleración

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: .main)
    }

    func startScanning() {
        if centralManager.state == .poweredOn {
            devices.removeAll()
            centralManager.scanForPeripherals(withServices: nil, options: nil)
        }
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            startScanning()
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let deviceName = peripheral.name ?? "Unknown Device"
        if deviceName.hasPrefix("Tensosense"), !devices.contains(where: { $0.0.identifier == peripheral.identifier }) {
            DispatchQueue.main.async {
                self.devices.append((peripheral, deviceName))
            }
        }
    }

    func connectToDevice(_ peripheral: CBPeripheral) {
        print("🔗 Connecting to \(peripheral.name ?? "Unknown Device")...")
        connectedPeripheral = peripheral
        connectedPeripheral?.delegate = self
        centralManager.stopScan()
        centralManager.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("✅ Connected to \(peripheral.name ?? "Unknown Device")")
        DispatchQueue.main.async {
            self.isConnected = true
            self.connectedPeripheral = peripheral
        }
        peripheral.discoverServices(nil) // ✅ Busca servicios al conectar
    }

    func disconnect() {
        if let peripheral = connectedPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
            DispatchQueue.main.async {
                self.isConnected = false
                self.connectedPeripheral = nil
                self.clearAccelerationData() // ✅ Limpia la gráfica
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            if characteristic.uuid == accelerationUUID {
                print("📡 Found acceleration characteristic!")
                targetCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic) // ✅ Habilita notificaciones de datos
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value, data.count >= 12 else { return }

        let x = data.withUnsafeBytes { $0.load(fromByteOffset: 0, as: Float.self) }
        let y = data.withUnsafeBytes { $0.load(fromByteOffset: 4, as: Float.self) }
        let z = data.withUnsafeBytes { $0.load(fromByteOffset: 8, as: Float.self) }

        let norm = normAcceleration(x, y, z) // ✅ Usa la función en C
        let isFalling = detectFall(norm) > 0.5
        let isOscillating = detectOscillation(norm) > 0.5

        DispatchQueue.main.async {
            self.accelerationData.append(AccelerationPoint(time: Date(), norm: Double(norm)))

            if self.accelerationData.count > 50 {
                self.accelerationData.removeFirst()
            }

            self.showFallWarning = isFalling
            self.showOscillationWarning = isOscillating

            if isFalling || isOscillating {
                DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
                    self.showFallWarning = false
                    self.showOscillationWarning = false
                }
            }
        }
    }

    func clearAccelerationData() {
        DispatchQueue.main.async {
            self.accelerationData.removeAll()
        }
    }
}

// ✅ Estructura para los puntos del gráfico
struct AccelerationPoint: Identifiable {
    let id = UUID()
    let time: Date
    let norm: Double
}
