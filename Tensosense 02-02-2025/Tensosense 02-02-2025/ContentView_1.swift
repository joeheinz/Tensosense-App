//
//  ContentView.swift
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
import SwiftUI

struct ContentView_1: View {
    @State private var goToNextView = false // ✅ Estado para controlar la navegación
    @StateObject private var bluetoothManager = BluetoothManager() // Instancia del BluetoothManager


    var body: some View {
        NavigationStack {
            VStack {
                Text("This is ContentView_1")
                    .font(.title)
                    .padding()

                Button("Ir a ContentView_2") {
                    goToNextView = true // ✅ Cambia el estado al presionar
                }
                .padding()
                .background(Color.blue)
                .foregroundColor(.white)
                .cornerRadius(10)
                
                Button("Imprimir Hello Wold"){ //Imprime HelloWorld de la funcion
                    print(bluetoothManager.HelloWorld())
                }
                .padding()
                .background(Color.cyan)
                .foregroundColor(Color.white)
                .cornerRadius(10)
                Button("Cambia de color el boton"){
                    bluetoothManager.changeButtonColor()
                }
                .padding()
                .background(bluetoothManager.buttonColor)
                .foregroundColor(.white)
                .cornerRadius(10)
            }
            .navigationDestination(isPresented: $goToNextView) {
                ContentView_2() // ✅ Navega cuando el estado cambia a `true`
            }
        }
    }
}

#Preview {
    ContentView_1()
}
