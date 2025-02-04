//
//  ContentView.swift
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
import SwiftUI
import CoreBluetooth

struct ContentView_1: View {
    @StateObject private var bluetoothManager = BluetoothManager()
    
    private let gradientBackground = LinearGradient(
        stops: [
            Gradient.Stop(color: .black, location: 0.0),
            Gradient.Stop(color: .init(white: 0.15, opacity: 1), location: 1.0)
        ],
        startPoint: .topLeading,
        endPoint: .bottomTrailing
    )
    
    private var connectionStatus: String {
        bluetoothManager.connectedPeripheral != nil ?
        "✅ Connected to \(bluetoothManager.connectedPeripheral?.name ?? "Unknown")" :
        "❌ No device connected"
    }
    
    private var connectionColor: Color {
        bluetoothManager.connectedPeripheral != nil ? .mint : .red
    }

    var body: some View {
        NavigationStack {
            ZStack {
                gradientBackground
                    .edgesIgnoringSafeArea(.all)
                
                ContentLayout(bluetoothManager: bluetoothManager,
                              connectionStatus: connectionStatus,
                              connectionColor: connectionColor)
            }
            .navigationTitle("")
            .onAppear(perform: bluetoothManager.startScanning)
        }
    }
}

// ✅ Diseño General de la Vista
private struct ContentLayout: View {
    @ObservedObject var bluetoothManager: BluetoothManager
    let connectionStatus: String
    let connectionColor: Color
    
    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            HeaderSection()
            ConnectionStatus(status: connectionStatus, color: connectionColor)
            DeviceList(devices: bluetoothManager.devices)
            NavigationButtons()
        }
        .padding(.horizontal)
        .padding(.top, 24)
    }
}

// ✅ Título Principal
private struct HeaderSection: View {
    var body: some View {
        Text("Nearby Bluetooth Devices")
            .font(.system(.title2, design: .rounded, weight: .semibold))
            .foregroundColor(.white)
            .frame(maxWidth: .infinity, alignment: .leading)
    }
}

// ✅ Indicador de Conexión
private struct ConnectionStatus: View {
    let status: String
    let color: Color
    
    var body: some View {
        Text(status)
            .font(.subheadline)
            .foregroundColor(color)
            .frame(maxWidth: .infinity, alignment: .leading)
            .padding(.bottom, 8)
    }
}

// ✅ Lista de Dispositivos Bluetooth (AHORA SOLO MUESTRA "Tensosense")
private struct DeviceList: View {
    let devices: [(CBPeripheral, String)]
    
    var body: some View {
        Group {
            if devices.isEmpty {
                SearchIndicator()
            } else {
                ScrollView {
                    LazyVStack(spacing: 12) {
                        ForEach(devices.filter { $0.1.hasPrefix("Tensosense") }, id: \.0.identifier) { device in
                            DeviceCard(deviceName: device.1)
                        }
                    }
                }
                .frame(maxHeight: 520)
            }
        }
    }
}

// ✅ Indicador de Búsqueda de Dispositivos
private struct SearchIndicator: View {
    var body: some View {
        VStack {
            ProgressView()
                .progressViewStyle(CircularProgressViewStyle(tint: .gray))
            Text("Searching for devices...")
                .foregroundColor(.gray)
                .font(.subheadline)
                .padding(.top, 8)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 24)
    }
}

// ✅ Tarjeta de Dispositivo Bluetooth (Sin botón de conexión)
private struct DeviceCard: View {
    let deviceName: String
    
    var body: some View {
        HStack {
            Text(deviceName)
                .font(.callout)
                .foregroundColor(.white)
            
            Spacer()
        }
        .padding()
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(Color.black.opacity(0.3))
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(Color.blue.opacity(0.2), lineWidth: 1)
                )
        )
    }
}

// ✅ Botones de Navegación
private struct NavigationButtons: View {
    var body: some View {
        VStack(spacing: 12) {
            NavigationLink("Tension", destination: ContentView_2())
            NavigationLink("Acceleration", destination: ContentView_3())
            NavigationLink("Map", destination: ContentView_4())
        }
        .buttonStyle(NavButtonStyle())
    }
}

// ✅ Estilo de Botones de Navegación
private struct NavButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.subheadline.weight(.medium))
            .foregroundColor(.white)
            .frame(maxWidth: .infinity)
            .padding()
            .background(
                RoundedRectangle(cornerRadius: 10)
                    .fill(Color.white.opacity(0.12))
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .stroke(Color.white.opacity(0.2), lineWidth: 1)
                    )
            )
            .opacity(configuration.isPressed ? 0.7 : 1.0)
    }
}

// ✅ Animación para Botones
struct ScaleButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .scaleEffect(configuration.isPressed ? 0.95 : 1.0)
            .animation(.interactiveSpring(), value: configuration.isPressed)
    }
}

#Preview {
    ContentView_1()
}
