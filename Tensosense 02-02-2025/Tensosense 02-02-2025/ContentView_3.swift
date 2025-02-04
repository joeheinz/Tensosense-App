//
//  ContentView_3.swift
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
//
import SwiftUI
import Charts

struct ContentView_3: View {
    @StateObject private var bluetoothManager = BluetoothManager() // ✅ Se usa correctamente como @StateObject
    
    var body: some View {
        NavigationStack {
            ZStack {
                // ✅ Fondo moderno
                LinearGradient(
                    colors: [Color.black, Color.blue.opacity(0.3)],
                    startPoint: .topLeading,
                    endPoint: .bottomTrailing
                )
                .ignoresSafeArea()

                VStack(spacing: 20) {
                    Text("📊 Accelerometer Data")
                        .font(.title2.bold())
                        .foregroundColor(.cyan)

                    ConnectionStatus(bluetoothManager: bluetoothManager)
                    DeviceList(bluetoothManager: bluetoothManager) // ✅ Lista de dispositivos Tensosense
                    AccelerationChart(bluetoothManager: bluetoothManager)
                    WarningAlerts(bluetoothManager: bluetoothManager)

                    Spacer()
                }
                .padding()
                .onAppear {
                    bluetoothManager.startScanning() // ✅ Escanea dispositivos cuando la vista carga
                    
                    if bluetoothManager.isConnected{
                        bluetoothManager.connectedPeripheral?.discoverServices(nil)
                    }
                }
            }
            .navigationTitle("")
        }
    }
}

// ✅ Muestra si hay un dispositivo conectado
private struct ConnectionStatus: View {
    @ObservedObject var bluetoothManager: BluetoothManager

    var body: some View {
        Text(bluetoothManager.connectedPeripheral != nil ?
             "✅ Connected to \(bluetoothManager.connectedPeripheral?.name ?? "Unknown")" :
             "❌ No device connected")
            .font(.headline)
            .foregroundColor(bluetoothManager.connectedPeripheral != nil ? .mint : .red)
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.white.opacity(0.1))
            .cornerRadius(10)
    }
}

// ✅ Lista de dispositivos filtrada (Solo "Tensosense")
private struct DeviceList: View {
    @ObservedObject var bluetoothManager: BluetoothManager

    var body: some View {
        VStack {
            Text("🔍 Select Device")
                .font(.headline)
                .foregroundColor(.white)

            if bluetoothManager.devices.isEmpty {
                Text("No devices found...")
                    .foregroundColor(.gray)
                    .padding()
            } else {
                ScrollView {
                    LazyVStack(spacing: 10) {
                        ForEach(bluetoothManager.devices.filter { $0.1.hasPrefix("Tensosense") }, id: \.0.identifier) { device in
                            Button(action: {
                                bluetoothManager.connectToDevice(device.0) // ✅ Conecta al dispositivo seleccionado
                            }) {
                                DeviceCard(deviceName: device.1, isConnected: device.0 == bluetoothManager.connectedPeripheral)
                            }
                        }
                    }
                }
                .frame(height: 150)
            }
        }
    }
}

// ✅ Tarjeta de Dispositivo Bluetooth (Muestra si está conectado)
private struct DeviceCard: View {
    let deviceName: String
    let isConnected: Bool

    var body: some View {
        HStack {
            Text(deviceName)
                .font(.callout)
                .foregroundColor(.white)
            
            Spacer()
            
            if isConnected {
                Text("Connected ✅")
                    .foregroundColor(.green)
                    .font(.footnote)
            }
        }
        .padding()
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(isConnected ? Color.green.opacity(0.3) : Color.black.opacity(0.3))
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(isConnected ? Color.green : Color.blue.opacity(0.2), lineWidth: 1)
                )
        )
    }
}

// ✅ Gráfico de aceleración en tiempo real
private struct AccelerationChart: View {
    @ObservedObject var bluetoothManager: BluetoothManager

    var body: some View {
        VStack {
            Text("📈 Acceleration Chart")
                .font(.headline)
                .foregroundColor(.white)

            Chart(bluetoothManager.accelerationData) { point in
                LineMark(
                    x: .value("Time", point.time),
                    y: .value("Acceleration", point.norm)
                )
                .interpolationMethod(.catmullRom)
                .foregroundStyle(Gradient(colors: [.cyan, .purple]))
            }
            .chartXAxis {
                AxisMarks(format: .dateTime.minute().second())
            }
            .chartYAxis {
                AxisMarks(position: .leading)
            }
            .frame(height: 200)
            .padding()
            .background(RoundedRectangle(cornerRadius: 15).fill(Color.gray.opacity(0.3)))
        }
    }
}

// ✅ Alertas de caída libre y oscilación
private struct WarningAlerts: View {
    @ObservedObject var bluetoothManager: BluetoothManager

    var body: some View {
        VStack(spacing: 10) {
            if bluetoothManager.showFallWarning {
                WarningCard(text: "⚠️ Free Fall Detected!", color: .red)
            }
            
            if bluetoothManager.showOscillationWarning {
                WarningCard(text: "⚠️ Oscillation Detected!", color: .blue)
            }
        }
    }
}

private struct WarningCard: View {
    let text: String
    let color: Color

    var body: some View {
        Text(text)
            .foregroundColor(color)
            .font(.headline)
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.black.opacity(0.8))
            .cornerRadius(15)
    }
}

#Preview {
    ContentView_3()
}
