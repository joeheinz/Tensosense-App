===============================================================================
                        PROYECTO TENSOSENSE - MICROCONTROLADOR
                        G2 Electronics UG - Sistema de Monitoreo IoT
===============================================================================

DESCRIPCIÓN GENERAL:
Este proyecto implementa un sistema de monitoreo avanzado basado en el microcontrolador 
EFR32MG24 de Silicon Labs. El dispositivo "Tensosense" combina sensores IMU (acelerómetro 
y giroscopio) con conversión analógica-digital de alta precisión, transmitiendo datos en 
tiempo real via Bluetooth Low Energy (BLE).

===============================================================================
                              ARQUITECTURA DEL SISTEMA
===============================================================================

HARDWARE PRINCIPAL:
- Microcontrolador: EFR32MG24 (Silicon Labs)
- Radio Bluetooth: Integrado en el SoC
- Sensor IMU: ICM20689 (3 ejes aceleración + 3 ejes orientación)
- Conversor ADC: IADC integrado de 20 bits
- LEDs de estado: Puertos A.4, B.0, D.2
- Alimentación: 3.3V

SOFTWARE BASE:
- SDK: Gecko SDK v4.4.2
- Compilador: GNU ARM v12.2.1
- Arquitectura: Sin sistema operativo (Super Loop)
- Herramientas: Simplicity Studio v5

===============================================================================
                           FUNCIONALIDADES PRINCIPALES
===============================================================================

1. SENSOR IMU (ACELERÓMETRO/GIROSCOPIO):
   - Chip: ICM20689
   - Rango de medición: Configurable
   - Resolución: 16 bits por eje
   - Frecuencia: 10ms (100 Hz)
   - Datos: Aceleración XYZ + Orientación XYZ
   - Calibración: Automática del giroscopio al inicio

2. CONVERSOR ANALÓGICO-DIGITAL (IADC):
   - Resolución: 20 bits (1 millón de valores)
   - Modo: Bipolar diferencial
   - Amplificación: 4x
   - Referencia: VDD×0.8 (≈2.64V)
   - Frecuencia de muestreo: 1 MHz
   - Buffer circular: 100 muestras para promediado
   - Entrada: Pin analógico ANA2

3. PROCESAMIENTO DIGITAL DE SEÑALES:
   - Filtro FIR con ventana Hamming
   - Orden del filtro: 7 coeficientes
   - Coeficientes: [0.05, 0.12, 0.18, 0.30, 0.18, 0.12, 0.05]
   - Propósito: Reducción de ruido en mediciones analógicas
   - Implementación: Buffer circular para eficiencia

4. SISTEMA DE DETECCIÓN:
   - Umbral de detección: 470 unidades de voltaje
   - Acción: Control automático de LEDs
   - LED Verde (A.4): Encendido cuando > umbral
   - LED Rojo (B.0): Encendido cuando < umbral

===============================================================================
                        COMUNICACIÓN BLUETOOTH LOW ENERGY
===============================================================================

CONFIGURACIÓN BLE:
- Protocolo: Bluetooth Low Energy (BLE)
- Perfil: GATT (Generic Attribute Profile)
- Nombre del dispositivo: "Tensosense"
- Intervalo de advertising: 100ms
- Modo: Connectable and discoverable

SERVICIOS GATT IMPLEMENTADOS:

1. SERVICIO BLINKY EXAMPLE:
   UUID: de8a5aac-a99b-c315-0c80-60d4cbb51224
   
   Características:
   - LED Control (UUID: 5b026510-4088-c297-46d8-be6c736a087a)
     Función: Control remoto de LEDs
     Tamaño: 1 byte
     Propiedades: Read/Write
   
   - Report Button (UUID: 61a885a4-41c3-60d0-9a53-6d652a70d29c)
     Función: Estado de botones
     Tamaño: 1 byte
     Propiedades: Read/Notify
   
   - Analog (UUID: 2A58)
     Función: Datos del conversor ADC
     Tamaño: 2 bytes (voltaje en mV)
     Propiedades: Read/Write/Notify/Indicate
     Frecuencia: 10ms cuando conectado

2. SERVICIO IMU:
   UUID: a4e649f4-4be5-11e5-885d-feff819cdc9f
   
   Características:
   - Acceleration (UUID: c4c1f6e2-4be5-11e5-885d-feff819cdc9f)
     Función: Datos de aceleración X,Y,Z
     Tamaño: 6 bytes (3×16 bits)
     Propiedades: Write/Notify/Indicate
   
   - Orientation (UUID: b7c4b694-bee3-45dd-ba9f-f3b5e994f49a)
     Función: Datos de orientación X,Y,Z
     Tamaño: 6 bytes (3×16 bits)
     Propiedades: Notify
   
   - Control Point (UUID: 71e30b8c-4131-4703-b0a0-b0bbba75856b)
     Función: Control del sensor IMU
     Tamaño: 3 bytes
     Propiedades: Write/Indicate

3. SERVICIO TX POWER:
   UUID: 1804
   
   Características:
   - Tx Power Level (UUID: 2A07)
     Función: RSSI para posicionamiento
     Tamaño: 1 byte (dBm)
     Propiedades: Read
     Uso: Medición de distancia/posicionamiento

===============================================================================
                              FLUJO DE OPERACIÓN
===============================================================================

SECUENCIA DE INICIO:
1. Inicialización del sistema (sl_system_init)
2. Configuración de periféricos:
   - Inicialización del sensor IMU
   - Calibración del giroscopio
   - Configuración del IADC
   - Setup de GPIOs para LEDs
3. Configuración Bluetooth:
   - Creación del advertising set
   - Configuración de parámetros BLE
   - Inicio del advertising
4. Entrada al bucle principal

BUCLE PRINCIPAL (app_process_action):

MODO DESCONECTADO:
- LED rojo (A.4) encendido permanente
- LED azul (B.0) encendido permanente  
- LED verde parpadea cada 1 segundo
- Advertising activo
- Modo de bajo consumo

MODO CONECTADO:
- Detener parpadeo de LEDs
- LED verde (A.4) apagado
- Ejecutar cada 10ms:
  ├── Leer datos del IADC
  ├── Aplicar filtro FIR
  ├── Control de LEDs según umbral
  ├── Transmitir datos ADC via BLE
  ├── Leer datos del IMU
  ├── Transmitir aceleración via BLE
  ├── Transmitir orientación via BLE
  └── Enviar RSSI para posicionamiento

===============================================================================
                          PROCESAMIENTO DE DATOS
===============================================================================

CADENA DE PROCESAMIENTO ADC:
1. Muestreo continuo a 1MHz
2. Lectura desde FIFO (4 muestras por interrupción)
3. Conversión a voltaje: V = (ADC_VALUE × 3300mV) / 1000
4. Almacenamiento en buffer circular (100 muestras)
5. Cálculo de promedio móvil
6. Aplicación de filtro FIR Hamming
7. Conversión a entero de 16 bits
8. Empaquetado en 2 bytes (Little Endian)
9. Transmisión BLE

CADENA DE PROCESAMIENTO IMU:
1. Actualización del sensor (sl_imu_update)
2. Lectura de aceleración (3 ejes × 16 bits)
3. Lectura de orientación (3 ejes × 16 bits)
4. Empaquetado: [X_low, X_high, Y_low, Y_high, Z_low, Z_high]
5. Transmisión BLE separada para cada tipo de dato

===============================================================================
                            GESTIÓN DE ENERGÍA
===============================================================================

ESTRATEGIAS DE AHORRO:
- Power Manager integrado (sl_power_manager)
- Modo sleep automático entre mediciones
- Clock gating de periféricos no utilizados
- Configuración IADC con shutdown entre conversiones

CONSUMO ESTIMADO:
- Modo advertising: ~2-5mA
- Modo conectado transmitiendo: ~8-15mA
- Modo sleep: <1mA

===============================================================================
                           CONFIGURACIÓN Y CALIBRACIÓN
===============================================================================

PARÁMETROS CONFIGURABLES:
- IMU_UPDATE_INTERVAL_MS = 10 (frecuencia de muestreo)
- ADV_INTERVAL_MS = 100 (intervalo de advertising)
- UMBRAL_DETECCION = 470 (nivel de disparo)
- FIR_TAP_NUM_NEW = 7 (orden del filtro)

CALIBRACIÓN IMU:
- Automática al inicio (sl_imu_calibrate_gyro)
- Compensación de offset del giroscopio
- Configuración de rangos dinámicos

CALIBRACIÓN ADC:
- Referencia interna calibrada
- Compensación de offset automática
- Corrección de no linealidad integrada

===============================================================================
                              CÓDIGOS DE ERROR
===============================================================================

CÓDIGOS BLUETOOTH:
- SL_STATUS_OK (0x0000): Operación exitosa
- Error al escribir GATT: Problema de memoria o configuración
- Error al notificar: Cliente desconectado o buffer lleno

CÓDIGOS IADC:
- IADC_STATUS_SCANFIFODV: Datos disponibles en FIFO
- Warning "No hay datos en FIFO": Muestreo más rápido que lectura

===============================================================================
                           APLICACIONES Y CASOS DE USO
===============================================================================

MONITOREO INDUSTRIAL:
- Detección de vibraciones en maquinaria
- Medición de tensión/deformación en estructuras
- Control de procesos en tiempo real

APLICACIONES BIOMÉDICAS:
- Monitoreo de actividad física
- Análisis de postura y movimiento
- Sensores portables (wearables)

IoT Y SMART CITIES:
- Sensores estructurales en puentes/edificios
- Monitoreo ambiental
- Sistemas de posicionamiento indoor (RSSI)

===============================================================================
                              DESARROLLO Y DEBUG
===============================================================================

HERRAMIENTAS DE DESARROLLO:
- Simplicity Studio v5 (IDE principal)
- GNU ARM Toolchain v12.2.1
- Gecko SDK v4.4.2
- Energy Profiler (análisis de consumo)

DEBUGGING:
- SWO (Serial Wire Output) habilitado
- Logs via app_log_info/warning/error
- LED de estado para diagnóstico visual
- RTT (Real Time Transfer) disponible

COMPILACIÓN:
- Makefile automático generado
- Configuración Release/Debug
- Optimización para tamaño/velocidad

===============================================================================
                             ESPECIFICACIONES TÉCNICAS
===============================================================================

MICROCONTROLADOR EFR32MG24:
- Core: ARM Cortex-M33 @ 78MHz
- Flash: 1536 KB
- RAM: 256 KB
- Radio: 2.4GHz (Bluetooth/Zigbee/Thread)
- Periféricos: ADC 20-bit, I2C, SPI, UART, Timer

COMUNICACIÓN:
- Bluetooth 5.0+ Low Energy
- Potencia TX: -40 to +10 dBm
- Sensibilidad RX: -97 dBm
- Throughput: hasta 2 Mbps

SENSORES:
- IMU: 6-DOF (3 acelerómetro + 3 giroscopio)
- ADC: 20-bit, hasta 1 Msps
- Rango dinámico: > 100 dB

===============================================================================
                              VERSIÓN Y LICENCIA
===============================================================================

Versión del firmware: 0.0.0
Versión del hardware: 000
Fabricante: Silicon Labs
Desarrollador: G2 Electronics UG

Licencia: Zlib License
Copyright 2021 Silicon Labs Inc.

Este proyecto utiliza componentes de código abierto del Gecko SDK y está
desarrollado para aplicaciones industriales y de investigación.

===============================================================================
                                   CONTACTO
===============================================================================

Para soporte técnico, documentación adicional o consultas sobre el proyecto:

Desarrollador: G2 Electronics UG
Proyecto: Tensosense
Plataforma: Silicon Labs EFR32MG24
SDK: Gecko SDK v4.4.2

Fecha de documentación: 2025
Última actualización: Proyecto activo en desarrollo

===============================================================================