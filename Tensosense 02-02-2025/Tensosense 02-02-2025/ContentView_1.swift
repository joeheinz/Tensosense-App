//
//  ContentView.swift
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
import SwiftUI

struct ContentView_1: View {
    @StateObject private var bluetoothManager = BluetoothManager()
    @State private var message: String = "Presiona el botón"

    var body: some View {
        VStack {
            Text("This is ContentView_1")
                .font(.title)
                .padding()

            Text(message) // Muestra el mensaje actualizado

            Button("Press") {
                print(message)
                message = bluetoothManager.HelloWorld() // ✅ Llamar a la función correctamente
            }
            NavigationLink(destination: ContentView_2()) {
                                Text("Ir a ContentView_2")
                                    .padding()
                                    .background(Color.blue)
                                    .foregroundColor(.white)
                                    .cornerRadius(10)
                }
        }
    }
}

#Preview {
    ContentView_1()
}
