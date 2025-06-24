# Dark Knight Programmer - SWD Flash Programmer

Este proyecto implementa un programador SWD (Serial Wire Debug) usando Bluetooth Low Energy para programar microcontroladores EFM32/EFR32 de Silicon Labs. El dispositivo actúa como un programador standalone que puede flashear firmware a través de la interfaz SWD.

## Características Principales

- **Programación SWD**: Implementa el protocolo SWD completo para debugging y programación
- **Interfaz Bluetooth**: Control remoto a través de BLE usando un smartphone
- **Soporte para mikroBUS**: Utiliza pines del conector mikroBUS para las señales SWD
- **Verificación de Flash**: Verifica la integridad de la programación después del flasheo
- **Protocolo Completo**: Implementa especificaciones AN1011 y AN0062 de Silicon Labs

## Configuración de Pines (mikroBUS)

El programador utiliza los siguientes pines del conector mikroBUS:

| Señal SWD | Pin mikroBUS | GPIO | Función |
|-----------|--------------|------|---------|
| SWCLK | SCK (PC01) | PC1 | Clock de SWD |
| SWDIO | MISO (PC02) | PC2 | Data I/O de SWD |
| RESET | RST (PC08) | PC8 | Reset del target |

### Conexiones al Target

Conecte estos pines del mikroBUS a su microcontrolador target:
- **PC1 (SWCLK)** → Pin SWCLK del target
- **PC2 (SWDIO)** → Pin SWDIO del target  
- **PC8 (RESET)** → Pin RESET del target
- **GND** → Ground común
- **3.3V** → Alimentación del target (si es necesario)

## Compilación del Proyecto

### Requisitos
- Simplicity Studio 5
- Gecko SDK 4.4.2
- GNU ARM Toolchain v12.2.1

### Pasos para Compilar

1. **Abrir Simplicity Studio**:
   ```
   Abrir Simplicity Studio 5
   File → Import → Existing Projects into Workspace
   Seleccionar la carpeta del proyecto
   ```

2. **Configurar el Toolchain**:
   ```
   Project → Properties → C/C++ Build → Tool Chain Editor
   Verificar que GNU ARM v12.2.1 esté seleccionado
   ```

3. **Compilar**:
   ```
   Click derecho en el proyecto → Build Project
   O usar Ctrl+B
   ```

4. **Archivos Generados**:
   - `Dark-Knight-Programmer.hex` - Archivo para flashear
   - `Dark-Knight-Programmer.bin` - Archivo binario
   - `Dark-Knight-Programmer.s37` - Archivo S-record

## Uso del Programador

### 1. Preparación del Hardware

1. **Flashear el Firmware**:
   ```
   Usar Simplicity Commander o J-Link:
   commander flash Dark-Knight-Programmer.hex --device EFR32MG24B210F1536IM48
   ```

2. **Conexiones Físicas**:
   - Conectar el target al conector mikroBUS según la tabla de pines
   - Asegurar conexión a ground común
   - Verificar alimentación del target (3.3V)

### 2. Control via Bluetooth

1. **Descargar App**: Instalar EFR Connect para [Android](https://play.google.com/store/apps/details?id=com.siliconlabs.bledemo) o [iOS](https://apps.apple.com/us/app/efr-connect/id1030932759)

2. **Conectar al Dispositivo**:
   - Abrir EFR Connect
   - Buscar "Dark-Knight-Programmer" en la lista de dispositivos
   - Conectar al dispositivo

3. **Operaciones Disponibles**:
   - **LED Control**: Enciende/apaga LED para indicar estado
   - **Button Status**: Recibe notificaciones del botón físico
   - **Programming Status**: Monitorea el progreso de programación

### 3. Programación Manual (Opcional)

El proyecto incluye Python scripts para control directo:

```bash
# Instalar dependencias
pip install -r requirements.txt

# Ejecutar receptor Bluetooth
python bluetooth_receiver.py

# Ejecutar receptor dual (Bluetooth + Serial)
python dual_receiver.py
```

## Protocolo SWD Implementado

### Funciones Principales

1. **Inicialización SWD**:
   - Reset de línea SWD
   - Cambio de JTAG a SWD
   - Lectura de IDCODE
   - Configuración de AP (Access Port)

2. **Operaciones de Memoria**:
   - Lectura/escritura de registros
   - Acceso a memoria via AHB-AP
   - Programación de flash page por page

3. **Verificación**:
   - Checksum verification
   - Word-by-word verification
   - Error reporting

### Targets Soportados

El programador soporta microcontroladores con los siguientes IDCODE:
- **Cortex-M33**: `0x2BA01477` (EFR32MG24, etc.)
- **Cortex-M0+**: `0x0BC11477` (EFM32, etc.)

## Indicadores de Estado

### LED Indicador
- **Apagado**: Sistema inactivo
- **Encendido Fijo**: Conectado vía Bluetooth
- **Parpadeando**: Programación en curso
- **Encendido con parpadeos rápidos**: Error de programación

### Botón de Usuario
- **Presión corta**: Inicia programación manual
- **Presión larga**: Reset del sistema
- **Doble click**: Modo de verificación

## Troubleshooting

### Problemas Comunes

1. **No se detecta el target**:
   - Verificar conexiones SWD (SWCLK, SWDIO, RESET)
   - Confirmar alimentación del target
   - Revisar que el target no esté en modo de bajo consumo

2. **Error de programación**:
   - Verificar integridad del archivo de firmware
   - Confirmar que el target esté desbloqueado
   - Revisar niveles de voltaje (3.3V)

3. **Bluetooth no conecta**:
   - Verificar que el dispositivo esté encendido
   - Reiniciar la aplicación móvil
   - Reset del programador con el botón

### Debugging

Para habilitar logs de debug:

```c
// En app.c, cambiar el nivel de debug
#define DEBUG_LEVEL 2  // 0=Off, 1=Error, 2=Info, 3=Debug
```

### Códigos de Error

| Código | Descripción |
|--------|-------------|
| 0x01 | Target no responde |
| 0x02 | IDCODE incorrecto |
| 0x03 | Error de flash unlock |
| 0x04 | Error de verificación |
| 0x05 | Timeout de operación |

## Archivos del Proyecto

### Archivos Principales
- `app.c` - Lógica principal y protocolo SWD
- `app_baseline.c` - Implementación de referencia (baseline)
- `main.c` - Punto de entrada del programa
- `app.h` - Definiciones y prototipos

### Configuración
- `config/` - Archivos de configuración del sistema
- `autogen/` - Archivos generados automáticamente
- `test_alpha.h/.bin` - Firmware de prueba para testing

### Scripts Python
- `bluetooth_receiver.py` - Receptor Bluetooth standalone
- `dual_receiver.py` - Receptor dual (BT + Serial)
- `create_bl_files.*` - Scripts para crear archivos bootloader

## Desarrollo y Modificaciones

### Cambiar Target de Programación

Para modificar el firmware que se programa:

1. Reemplazar `test_alpha.bin` con su firmware
2. Actualizar `test_alpha.h` con los nuevos datos:
   ```bash
   xxd -i firmware.bin > test_alpha.h
   ```

### Agregar Nuevos Targets

Para soportar nuevos microcontroladores:

1. Agregar IDCODE en `app.c`:
   ```c
   #define SWD_IDCODE_NEW_TARGET  0x12345678
   ```

2. Actualizar la función de detección:
   ```c
   // En swd_detect_target()
   case SWD_IDCODE_NEW_TARGET:
       app_log_info("Detected: New Target");
       break;
   ```

## Referencias Técnicas

- [AN1011: EFM32 Standalone Programmer](AN1011-efm32-standalone-programmer.txt)
- [AN0062: EFM32 Programming Guide](an0062.txt)
- [ARM Debug Interface Specification](https://developer.arm.com/documentation/ihi0031/latest/)
- [SWD Protocol Specification](https://developer.arm.com/documentation/ihi0074/latest/)

## Licencia

Este proyecto está basado en el SDK de Silicon Labs y mantiene la licencia Zlib original. Ver archivos fuente individuales para detalles de licencia específicos.

## Soporte

Para reportar bugs o solicitar soporte:
- [Silicon Labs Community](https://www.silabs.com/community)
- [Gecko SDK Documentation](https://docs.silabs.com/gecko-platform/latest/)
- [Bluetooth Documentation](https://docs.silabs.com/bluetooth/latest/)