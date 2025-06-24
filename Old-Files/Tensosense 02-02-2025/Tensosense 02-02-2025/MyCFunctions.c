//
//  MyCFunctions.c
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
//

#include "MyCFunctions.h"

#include <stdio.h>

#include "MyCFunctions.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <Accelerate/Accelerate.h>

#define BUFFER_SIZE 10 // Size for FIR filter

// Simulated acceleration values (in real use, get them via Bluetooth)
float accel_x = 0.0, accel_y = 0.0, accel_z = 0.0;

// FIR filter buffer
float accel_buffer[BUFFER_SIZE] = {0};
int buffer_index = 0;

void getAcceleration(float *x, float *y, float *z) {
    *x = ((float)rand() / (float)(RAND_MAX)) * 2 - 1; // Random -1 to 1
    *y = ((float)rand() / (float)(RAND_MAX)) * 2 - 1;
    *z = ((float)rand() / (float)(RAND_MAX)) * 2 - 1;
}

// Compute the Euclidean norm of acceleration
float normAcceleration(float x, float y, float z) {
    return sqrtf(x * x + y * y + z * z);
}

// Apply a simple FIR (moving average) filter
float filterFIR(float normAcceleration) {
    // Store new value in circular buffer
    accel_buffer[buffer_index] = normAcceleration;
    buffer_index = (buffer_index + 1) % BUFFER_SIZE;

    // Compute average
    float sum = 0.0;
    for (int i = 0; i < BUFFER_SIZE; i++) {
        sum += accel_buffer[i];
    }

    return sum / BUFFER_SIZE;
}

// Detect falls based on norm acceleration
float detectFall(float processedAcceleration) {
    if (processedAcceleration < 0.1) { // Threshold for a fall (near 0 acceleration)
        printf("⚠️ Fall detected! ⚠️\n");
        return 1;
    }
    return 0;
}

// Detect oscillations using FFT
float detectOscillation(float processedAcceleration) {
    int N = 16; // Number of FFT points (must be power of 2)
    float inputSignal[N];
    float outputSignal[N];
    DSPSplitComplex signal;
    
    // Fill input buffer with acceleration data
    for (int i = 0; i < N; i++) {
        inputSignal[i] = processedAcceleration + (float)(rand() % 10) / 100.0; // Add small noise
    }
    
    // FFT Setup
    FFTSetup setup = vDSP_create_fftsetup(log2f(N), kFFTRadix2);
    signal.realp = inputSignal;
    signal.imagp = outputSignal;
    
    vDSP_fft_zip(setup, &signal, 1, log2f(N), kFFTDirection_Forward);
    vDSP_destroy_fftsetup(setup);

    // Analyze frequencies
    float sum = 0.0;
    for (int i = 0; i < N; i++) {
        sum += fabs(outputSignal[i]); // Sum of magnitudes
    }
    
    if (sum > 10.0) { // Arbitrary threshold for oscillations
        printf("🔄 Oscillation detected! 🔄\n");
        return 1;
    }
    
    return 0;
}
