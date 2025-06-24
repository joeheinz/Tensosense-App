//
//  CWrapper.swift
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
//

import Foundation

class CWrapper: ObservableObject {
    @Published var accelX: Float = 0.0
    @Published var accelY: Float = 0.0
    @Published var accelZ: Float = 0.0
    @Published var normAcc: Float = 0.0
    @Published var dataPoints: [(Float, Float, Float, Float)] = [] // (X, Y, Z, Norm)

    func updateAcceleration() {
        var x: Float = 0.0
        var y: Float = 0.0
        var z: Float = 0.0
        
        getAcceleration(&x, &y, &z) // Call C function
        let norm = normAcceleration(x, y, z)
        
        DispatchQueue.main.async {
            self.accelX = x
            self.accelY = y
            self.accelZ = z
            self.normAcc = norm
            
            // Store latest 50 points for graph
            self.dataPoints.append((x, y, z, norm))
            if self.dataPoints.count > 50 {
                self.dataPoints.removeFirst()
            }
        }
    }
}
