//
//  BluetoothManager.swift
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
//
import CoreBluetooth
import SwiftUI
import Accelerate // Para FFT

class BluetoothManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    @Published var buttonColor: Color = .blue //Define el color principal del Boton Almacena el color
    // 📡 Estado del Bluetooth
    private var centralManager: CBCentralManager!
    @Published var selectedPeripheral: CBPeripheral?
    private var characteristic: CBCharacteristic?

    // 🔴 Buffer para detectar caídas
    private var fallDetectionBuffer: [AccelerationData] = []
    private var isCapturingFall = false

    // 📊 Variables observables para la UI
    @Published var accelerations: [AccelerationData] = []
    @Published var isFreeFall = false
    @Published var isOscillating = false
    @Published var showFreeFallWarning = false
    @Published var connectionStatus = "Buscando dispositivos..."
    @Published var isConnected = false
    @Published var discoveredDevices: [CBPeripheral] = []

    // 🏷 UUID del sensor de aceleración
    private let targetUUID = CBUUID(string: "c4c1f6e2-4be5-11e5-885d-feff819cdc9f")
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    func changeButtonColor() {
        DispatchQueue.main.async{
            self.buttonColor = self.buttonColor == .blue ? .red : .blue
        }
    }

    // 🔍 **Escaneo de dispositivos**
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            connectionStatus = "Escaneando dispositivos..."
            centralManager.scanForPeripherals(withServices: nil, options: nil)
        } else {
            connectionStatus = "Bluetooth no disponible"
        }
    }

    // 📡 **Descubrir dispositivos BLE**
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        if peripheral.name?.hasPrefix("Tensosense") == true && !discoveredDevices.contains(where: { $0.identifier == peripheral.identifier }) {
            discoveredDevices.append(peripheral)
        }
    }

    // 🔗 **Conectar a un dispositivo**
    func connectToDevice(device: CBPeripheral) {
        centralManager.stopScan()
        selectedPeripheral = device
        selectedPeripheral?.delegate = self
        connectionStatus = "Conectando a \(device.name ?? "Desconocido")..."
        centralManager.connect(device, options: nil)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        connectionStatus = "Conectado a \(peripheral.name ?? "Dispositivo")"
        isConnected = true
        peripheral.discoverServices(nil)
    }

    // ❌ **Desconectar y reanudar escaneo**
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        connectionStatus = "Desconectado"
        isConnected = false
        centralManager.scanForPeripherals(withServices: nil, options: nil)
    }

    // 📡 **Descubrir servicios**
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }

    // 🔍 **Buscar característica de aceleración**
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            if characteristic.uuid == targetUUID {
                self.characteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
    }

    // 📊 **Recibir datos de aceleración**
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        if let data = characteristic.value {
            processSensorData(data)
        }
    }
    
    // 🧮 **Procesar datos del sensor**
    func processSensorData(_ data: Data) {
        let values = data.withUnsafeBytes {
            Array(UnsafeBufferPointer<Int16>(start: $0.bindMemory(to: Int16.self).baseAddress!, count: 3))
        }
        let x = Double(values[0]) / 1000.0
        let y = Double(values[1]) / 1000.0
        let z = Double(values[2]) / 1000.0
        let magnitude = sqrt(x*x + y*y + z*z)
        let time = Double(Date().timeIntervalSince1970)

        accelerations.append(AccelerationData(time: time, value: magnitude))
        if accelerations.count > 100 { accelerations.removeFirst() }

        // 🚨 **Detectar caída**
        detectFreeFall(time: time, magnitude: magnitude)

        // 📡 **Detectar oscilaciones con FFT**
        isOscillating = detectOscillationsFFT()
    }

    // 🚨 **Detectar caída**
    func detectFreeFall(time: Double, magnitude: Double) {
        if magnitude < 0.1 {  // Umbral de caída
            isFreeFall = true
            showFreeFallWarning = true

            // Iniciar captura de datos de la caída
            if !isCapturingFall {
                isCapturingFall = true
                fallDetectionBuffer = []
                print("⚠️ Caída detectada. Capturando datos...")
            }

            fallDetectionBuffer.append(AccelerationData(time: time, value: magnitude))

            if fallDetectionBuffer.count >= 150 {

                isCapturingFall = false
            }

            DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
                self.showFreeFallWarning = false
            }
        } else {
            isFreeFall = false

            if isCapturingFall {
                fallDetectionBuffer.append(AccelerationData(time: time, value: magnitude))
                if fallDetectionBuffer.count >= 50 {
                    isCapturingFall = false
                }
            }
        }
    }

    // 📡 **Detectar oscilaciones con FFT**
    func detectOscillationsFFT() -> Bool {
        let sampleCount = 64
        guard accelerations.count >= sampleCount else { return false }

        let recentData = accelerations.suffix(sampleCount).map { $0.value }
        var realParts = recentData + [Double](repeating: 0.0, count: sampleCount - recentData.count)
        var imaginaryParts = [Double](repeating: 0.0, count: sampleCount)

        var splitComplex = DSPDoubleSplitComplex(realp: &realParts, imagp: &imaginaryParts)
        let fftSetup = vDSP_create_fftsetupD(vDSP_Length(log2(Double(sampleCount))), FFTRadix(kFFTRadix2))

        vDSP_fft_zipD(fftSetup!, &splitComplex, 1, vDSP_Length(log2(Double(sampleCount))), FFTDirection(FFT_FORWARD))

        var magnitudes = [Double](repeating: 0.0, count: sampleCount / 2)
        vDSP_zvmagsD(&splitComplex, 1, &magnitudes, 1, vDSP_Length(sampleCount / 2))

        let maxMagnitude = magnitudes[1...].max() ?? 0.0
        vDSP_destroy_fftsetupD(fftSetup)

        return maxMagnitude > 5.0
    }
    
    func HelloWorld() -> String{
        return ("Hello World")
    }
}

// 📊 **Estructura para almacenar datos de aceleración**
struct AccelerationData: Identifiable {
    let id = UUID()
    let time: Double
    let value: Double
}
