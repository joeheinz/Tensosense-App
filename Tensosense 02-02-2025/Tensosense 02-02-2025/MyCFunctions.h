//
//  MyCFunctions.h
//  Tensosense 02-02-2025
//
//  Created by José Heinz Möller Santos on 02.02.25.
//

#ifndef MyCFunctions_h
#define MyCFunctions_h

int addNumbers(int a, int b);

void getAcceleration(float *x, float *y, float *z);

float normAcceleration(float x, float y , float z);

float filterFIR(float normAcceleration);

float detectFall(float processedAcceleration);

float detectOscillation(float processedAcceleration);



#endif /* MyCFunctions_h */
