# Tensosense-App

## Autor: Heinz Möller S.

### Sistema Profesional de Monitoreo de Sensores Duales con Aplicación Móvil

Tensosense-App es una solución completa de monitoreo de sensores en tiempo real que incluye tanto la aplicación móvil iOS como el firmware para microcontroladores. El sistema está diseñado para la captura, análisis y visualización de datos de sensores de aceleración y tensión mediante comunicación Bluetooth Low Energy (BLE).

---

## Características Principales

### Aplicación iOS (SwiftUI)
- **Interfaz dual deslizable** con navegación horizontal entre sensores
- **Monitoreo en tiempo real** de aceleración y tensión
- **Detección inteligente de eventos** (caída libre, oscilaciones, variaciones críticas)
- **Visualización gráfica avanzada** con Charts framework
- **Integración GPS** para geolocalización de eventos
- **Conectividad WebSocket** para transmisión de datos
- **Exportación CSV** de datos históricos
- **Splash screen profesional** con branding personalizado

### Firmware Microcontrolador (C/EFR32)
- **Sensores duales**: IMU (aceleración) + IADC (tensión analógica)
- **Filtros FIR** avanzados con ventana de Hamming
- **Detección de umbrales** configurables
- **Comunicación BLE** con notificaciones automáticas
- **Gestión de energía** optimizada
- **Indicadores LED** de estado y conectividad

---

## Arquitectura del Sistema

### Estructura del Proyecto

```
Tensosense-App/
├── IOS-App/                    # Aplicaciones iOS
│   ├── Tensosense Beta/        # Versión principal
│   └── Tensosense Dark-Knight/ # Versión alternativa
├── Android-App/                # Aplicación Android (en desarrollo)
├── Microcontroller-Codes/      # Firmware para microcontroladores
│   ├── test_alpha/            # Código principal del sensor
│   ├── Dark-Knight-Programmer/ # Herramientas de programación
│   └── bootloader-apploader-1/ # Bootloader del sistema
└── Old-Files/                  # Archivos históricos
```

### Componentes Clave iOS

#### 1. Gestión de Interfaz
- **SplashScreenView.swift**: Pantalla de carga con animaciones profesionales
- **RootView.swift**: Controlador principal de la aplicación
- **ContentView.swift**: Interfaz principal con páginas deslizables
- **MapView.swift**: Vista del mapa con integración GPS

#### 2. Gestión de Comunicaciones
- **BLEManager.swift**: Manejo dual de Bluetooth para sensores múltiples
- **WebSocketManager.swift**: Conectividad en tiempo real
- **CSVManager.swift**: Exportación y almacenamiento de datos

#### 3. Configuración y Utilidades
- **AppConfiguration.swift**: Configuraciones centralizadas del sistema
- **ModernComponents.swift**: Componentes UI reutilizables
- **AccelerationData.swift**: Modelos de datos de sensores

---

## Especificaciones Técnicas

### Sensores Soportados

#### Sensor de Aceleración (IMU)
- **UUID BLE**: `c4c1f6e2-d4a2-4cbb-8e5c-96c0c4f1b3a2`
- **Datos**: 3 ejes (X, Y, Z) en formato Int16
- **Detecciones**: Caída libre, oscilaciones FFT
- **Umbrales configurables**: Caída libre (0.1-2.0g), Oscilación (3.0-10.0)

#### Sensor de Tensión (IADC)
- **UUID BLE**: `00002a58-0000-1000-8000-00805f9b34fb`
- **Datos**: Valor ADC directo en formato UInt16
- **Rango típico**: ~2100 unidades ADC (3.3V referencia)
- **Detecciones**: Variaciones críticas, desviación estándar continua
- **Filtrado**: FIR con ventana de Hamming (7 coeficientes)

### Comunicación BLE

#### Características del Protocolo
- **Intervalo de advertising**: 100ms
- **Intervalo de actualización IMU**: 10ms
- **Notificaciones automáticas**: Habilitadas para ambos sensores
- **Gestión de conexión**: Reconexión automática
- **RSSI reporting**: Para posicionamiento del dispositivo

#### Paquetes de Datos
```c
// Aceleración (6 bytes)
struct AccelPacket {
    int16_t x;  // Eje X
    int16_t y;  // Eje Y  
    int16_t z;  // Eje Z
};

// Tensión (2 bytes)
struct TensionPacket {
    uint16_t adc_value;  // Valor ADC directo
};
```

---

## Configuración y Uso

### Configuración de la Aplicación iOS

#### Requisitos del Sistema
- iOS 15.0 o superior
- Xcode 14.0 o superior
- SwiftUI compatible
- Permisos: Bluetooth, Localización, Notificaciones

#### Configuración de Umbrales

En `AppConfiguration.swift`:
```swift
struct Sensor {
    static let accelerationScale: Double = 1000.0
    static let freeFallThreshold: Double = 0.1  // g
    static let oscillationThreshold: Double = 5.0
    static let maxAccelerationCount: Int = 100
    static let sampleCountForFFT: Int = 64
}
```

#### UUIDs de Servicios BLE
```

### Configuración del Firmware Microcontrolador

#### Compilación y Programación
```bash
# Navegar al directorio del proyecto
cd Microcontroller-Codes/test_alpha/

# Compilar usando GNU ARM Toolchain
make clean && make

# Programar el dispositivo (usando Simplicity Studio)
./create_bl_files.sh
```

#### Configuración de Sensores

##### IADC (Tensión)
```c
// Configuración del convertidor analógico
#define CLK_ADC_FREQ         1000000  // 1MHz para alta resolución
#define IADC_INPUT_0_PORT_PIN iadcPosInputPadAna2  // Pin analógico 2
#define VREF_MV              3300.0   // Referencia 3.3V
```

##### IMU (Aceleración)
```c
// Configuración del sensor inercial
#define IMU_UPDATE_INTERVAL_MS 10     // Actualización cada 10ms
#define FIR_TAP_NUM_NEW       7       // Filtro FIR de 7 coeficientes
```

---

## Funcionalidades Avanzadas



### Geolocalización de Eventos

#### Guardado Automático de Ubicación
```swift
private func saveGeographicEvent(type: EventLocation.EventType, value: Double) {
    locationManager.requestLocation()
    
    let event = EventLocation(
        id: UUID(),
        latitude: locationManager.location?.coordinate.latitude ?? 0.0,
        longitude: locationManager.location?.coordinate.longitude ?? 0.0,
        timestamp: Date(),
        eventType: type,
        eventValue: value,
        accuracy: locationManager.location?.horizontalAccuracy ?? 0.0
    )
    
    let success = csvManager.saveEventLocation(event)
}
```

---

## Personalización y Extensión

### Agregar Nuevos Sensores

#### 1. Configuración (AppConfiguration.swift)
```swift
static let magnetometerServiceUUID = CBUUID(string: "TU-NUEVO-UUID-AQUI")
```

#### 2. Gestión BLE (BLEManager.swift)
```swift
@Published var magnetometerData: [AccelerationData] = []
@Published var showMagneticWarning = false

private func processMagnetometerData(_ data: Data) {
    // Implementar procesamiento específico del sensor
}
```

#### 3. Interfaz de Usuario (ContentView.swift)
```swift
// Cambiar indicadores de página
ForEach(0..<3, id: \.self) { index in  // Era 0..<2

// Agregar tercera página
SensorPageView(
    pageTitle: "Magnetometer Monitor",
    pageSubtitle: "Real-time magnetic field data",
    iconName: "location.north",
    iconColor: .orange,
    sensorData: bleManager.magnetometerData,
    // ... otros parámetros
).tag(2)
```

### Personalización Visual

#### Colores y Temas
```swift
// Colores disponibles para sensores
iconColor: .green    // Aceleración
iconColor: .purple   // Tensión  
iconColor: .orange   // Magnetómetro
iconColor: .red      // Temperatura
iconColor: .blue     // Presión
```

#### Iconos SF Symbols
```swift
// Iconos recomendados para sensores
"chart.line.uptrend.xyaxis"  // Aceleración
"gyroscope"                  // Tensión/Rotación
"location.north"             // Magnetómetro
"thermometer"                // Temperatura
"gauge"                      // Presión
```

---

## Herramientas de Desarrollo

### Scripts de Compilación

#### Crear Archivos de Bootloader
```bash
# Linux/macOS
./create_bl_files.sh

# Windows
create_bl_files.bat

# Python (multiplataforma)
python create_bl_files.py
```

### Herramientas de Prueba

#### Receptor Python para Desarrollo
```bash
cd Microcontroller-Codes/Dark-Knight-Programmer/
python bluetooth_receiver.py
```

#### Cliente BLE de Prueba
```bash
# Para pruebas duales de sensores
python dual_receiver.py

# Ejecutar script de recepción
./run_receiver.sh
```

---

## Resolución de Problemas

### Problemas Comunes

#### 1. Conexión BLE No Funciona
- **Verificar permisos de Bluetooth** en configuración iOS
- **Confirmar UUIDs** en AppConfiguration.swift
- **Revisar estado del microcontrolador** (LED parpadeante = buscando conexión)

#### 2. Sin Datos en Gráficos
- **Verificar vinculación** de `sensorData` en ContentView
- **Comprobar procesamiento** en BLEManager.swift líneas 184-245
- **Revisar logs** de Xcode para errores de datos

#### 3. GPS No Disponible
- **Verificar permisos** en Tensosense-Beta-Info.plist
- **Comprobar configuración** de LocationManager
- **Activar servicios de ubicación** en iOS

#### 4. Filtros No Aplicados
- **Verificar inicialización** de buffers FIR
- **Comprobar coeficientes** del filtro
- **Revisar frecuencia** de muestreo

### Configuraciones de Debug

#### Logs de Depuración
```swift
// Habilitar logs detallados en BLEManager
print("Aceleración: X:\(x) Y:\(y) Z:\(z) Magnitud:\(magnitude)")
print("Tensión IADC: \(adcValue)")
print("GPS: Lat:\(latitude) Lon:\(longitude)")
```


---

## Roadmap y Mejoras Futuras

### Funcionalidades Planificadas
- **Aplicación Android** nativa
- **Dashboard web** para monitoreo remoto
- **Análisis de machine learning** para patrones de comportamiento
- **Integración cloud** para almacenamiento masivo de datos
- **API REST** para integración con sistemas externos

### Optimizaciones Técnicas
- **Compresión de datos** BLE para mayor eficiencia
- **Predicción de eventos** usando algoritmos avanzados
- **Gestión avanzada de energía** en microcontrolador
- **Sincronización temporal** precisa entre sensores

---

## Licencia y Contacto

### Información del Autor
**Heinz Möller S.**  
Ingeniero en Electrónica y Desarrollador de Software  
Especialista en Sistemas Embebidos y Aplicaciones Móviles

### Soporte Técnico
Para consultas técnicas, problemas de implementación o solicitudes de nuevas funcionalidades, contactar al autor del proyecto.

### Contribuciones
El proyecto está abierto a contribuciones y mejoras. Se recomienda seguir las convenciones de código establecidas y documentar adecuadamente cualquier modificación.

---

## Changelog

### Versión Actual: Beta Profesional Completa
- Implementación dual de sensores (Aceleración + Tensión)
- Interfaz deslizable con navegación horizontal
- Splash screen profesional con branding
- Filtros FIR avanzados con ventana de Hamming
- Detección inteligente de eventos geográficos
- Sistema de alertas y notificaciones en tiempo real
- Exportación CSV con geolocalización
- Documentación técnica completa

### Baseline Commit: 5e903c2
Sistema profesional completo con todas las funcionalidades principales implementadas y probadas.

---

*Última actualización: Enero 2025*  
*Versión del documento: 1.0*  
*Compatibilidad: iOS 15.0+, EFR32 Series 2*
