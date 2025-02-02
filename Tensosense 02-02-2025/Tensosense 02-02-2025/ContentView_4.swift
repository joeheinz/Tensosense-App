//
//  ContentView_4.swift
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
//

import SwiftUI

struct ContentView_4: View {
    var body: some View {
        VStack {
            Text("Pantalla 4")
                .font(.largeTitle)
                .bold()
            
            NavigationLink(destination: ContentView_1()) {
                Text("Volver a Inicio")
                    .padding()
                    .background(Color.red)
                    .foregroundColor(.white)
                    .cornerRadius(10)
            }
        }
        .padding()
    }
}

#Preview {
    ContentView_4()
}
