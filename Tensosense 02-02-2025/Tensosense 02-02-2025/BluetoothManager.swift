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
    private let accelerationUUID = CBUUID(string: "c4c1f6e2-4be5-11e5-885d-feff819cdc9f") // UUID de aceleración

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: .main)
    }

    // ✅ Iniciar escaneo de dispositivos
    func startScanning() {
        if centralManager.state == .poweredOn {
            devices.removeAll()
            centralManager.scanForPeripherals(withServices: nil, options: nil)
        }
    }

    // ✅ Manejar cambio de estado del Bluetooth
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            startScanning()
        }
    }

    // ✅ Descubrir dispositivos Bluetooth cercanos
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let deviceName = peripheral.name ?? "Unknown Device"
        if !devices.contains(where: { $0.0.identifier == peripheral.identifier }) {
            DispatchQueue.main.async {
                self.devices.append((peripheral, deviceName))
            }
        }
    }

    // ✅ Conectar a un dispositivo Bluetooth
    func connectToDevice(_ peripheral: CBPeripheral) {
        print("Connecting to \(peripheral.name ?? "Unknown Device")...")
        connectedPeripheral = peripheral
        connectedPeripheral?.delegate = self
        centralManager.stopScan()
        centralManager.connect(peripheral, options: nil)
    }

    // ✅ Manejar conexión exitosa
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("✅ Connected to \(peripheral.name ?? "Unknown Device")")
        DispatchQueue.main.async {
            self.isConnected = true
            self.connectedPeripheral = peripheral
        }
        peripheral.discoverServices(nil) // Buscar servicios
    }

    // ✅ Manejar desconexión
    func disconnect() {
        if let peripheral = connectedPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
            DispatchQueue.main.async {
                self.isConnected = false
                self.connectedPeripheral = nil
            }
        }
    }

    // ✅ Descubrir servicios en el dispositivo
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }

    // ✅ Descubrir características dentro de un servicio
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            if characteristic.uuid == accelerationUUID {
                targetCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic) // Habilitar notificaciones
            }
        }
    }
    func startReadingAcceleration() {
        guard let peripheral = connectedPeripheral else { return }
        peripheral.discoverServices(nil)
    }

    // ✅ Recibir datos de aceleración desde el Bluetooth
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value else { return }
        if data.count >= 12 {
            let x = data.withUnsafeBytes { $0.load(fromByteOffset: 0, as: Float.self) }
            let y = data.withUnsafeBytes { $0.load(fromByteOffset: 4, as: Float.self) }
            let z = data.withUnsafeBytes { $0.load(fromByteOffset: 8, as: Float.self) }

            let norm = normAcceleration(x, y, z) // ✅ Usa la función en C
            let isFalling = detectFall(norm) > 0.5 // ✅ Convertimos Float a Bool
            let isOscillating = detectOscillation(norm) > 0.5 // ✅ Convertimos Float a Bool

            DispatchQueue.main.async {
                self.accelerationData.append(AccelerationPoint(time: Date(), norm: Double(norm)))

                // Limitar la cantidad de datos almacenados
                if self.accelerationData.count > 50 {
                    self.accelerationData.removeFirst()
                }

                self.showFallWarning = isFalling
                self.showOscillationWarning = isOscillating

                // Ocultar advertencias después de 2 segundos
                if isFalling || isOscillating {
                    DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
                        self.showFallWarning = false
                        self.showOscillationWarning = false
                    }
                }
            }
        }
    }
}

// ✅ Estructura para el gráfico de aceleración
struct AccelerationPoint: Identifiable {
    let id = UUID()
    let time: Date
    let norm: Double
}
