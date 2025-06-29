# Tensosense Beta

Sistema de monitoreo avanzado para camiones que mide tension y aceleracion en tiempo real mediante conexion Bluetooth con el chip EFR32 "Tensosense".

## Descripcion General

Tensosense Beta es una aplicacion iOS profesional diseñada para el monitoreo de sensores en camiones industriales. La aplicacion se conecta al dispositivo Tensosense (chip EFR32) via Bluetooth para capturar datos de tension y aceleracion, procesarlos en tiempo real y transmitirlos a un servidor WebSocket para analisis centralizado.

## Funcionalidades Principales

### Conexion Bluetooth Multi-Usuario
- Soporte para conexion de multiples dispositivos iOS simultaneos al chip Tensosense EFR32
- Escaneo automatico de dispositivos Bluetooth disponibles
- Conexion estable con reconexion automatica en caso de perdida de señal
- Gestion inteligente de dispositivos descubiertos (filtrado por intensidad de señal RSSI > -70)

### Monitoreo de Sensores Dual
- **Sensor de Aceleracion**: Captura datos del acelerometro integrado en el chip EFR32
- **Sensor de Tension IADC**: Monitoreo de tension mediante el convertidor analogico-digital integrado
- Procesamiento en tiempo real con algoritmos de deteccion avanzados
- Visualizacion grafica de datos con actualizacion en vivo

### Deteccion de Eventos Criticos
- **Deteccion de Caida Libre**: Umbral configurable (< 0.1g) para detectar caidas
- **Deteccion de Vibraciones**: Analisis FFT para identificar oscillaciones peligrosas
- **Monitoreo de Tension**: Alertas automaticas por variaciones criticas en el ADC
- **Sistema de Alertas**: Notificaciones visuales inmediatas para eventos criticos

### Transmision de Datos WebSocket
- Conexion en tiempo real al servidor WebSocket (ws://192.168.178.26:8000/ws)
- Envio automatico de datos de sensores al servidor central
- Formato JSON estandarizado para integracion con sistemas externos
- Estado de conexion visible en la interfaz de usuario

### Registro Geografico de Eventos
- Captura automatica de coordenadas GPS cuando ocurren eventos criticos
- Almacenamiento de eventos con ubicacion, timestamp y tipo de evento
- Generacion de archivos CSV con registro completo de viajes
- Mapas interactivos para visualizacion de eventos por ubicacion

### Gestion de Datos
- **Exportacion CSV**: Archivos estructurados para analisis posterior
- **Almacenamiento Local**: Datos guardados en el dispositivo iOS
- **Historial de Viajes**: Registro completo de todas las sesiones de monitoreo
- **Limpieza Automatica**: Gestion inteligente de espacio de almacenamiento

## Arquitectura Tecnica

### Configuracion Bluetooth
- **UUID Aceleracion**: c4c1f6e2-4be5-11e5-885d-feff819cdc9f
- **UUID Tension IADC**: 00002a58-0000-1000-8000-00805f9b34fb
- **Protocolo**: Bluetooth Low Energy (BLE)
- **Dispositivo Objetivo**: Tensosense (chip EFR32)

### Especificaciones de Sensores
- **Escala Aceleracion**: 1000.0 (factor de conversion)
- **Umbral Caida Libre**: 0.1g
- **Umbral Vibracion**: 5.0 (analisis FFT)
- **Frecuencia Muestreo**: 64 muestras para FFT
- **Buffer Maximo**: 100 lecturas simultaneas

### Configuracion WebSocket
- **URL Servidor**: ws://192.168.178.26:8000/ws
- **Formato Datos**: JSON {"time": timestamp, "value": valor}
- **Conexion Persistente**: Reconexion automatica cada 3 segundos
- **Timeout**: 5 segundos para establecer conexion

## Requisitos del Sistema

### Dispositivos Compatibles
- iPhone/iPad con iOS 14.0 o superior
- Bluetooth 4.0 o superior habilitado
- Permisos de ubicacion para registro geografico

### Hardware Requerido
- Chip Tensosense EFR32 configurado y operativo
- Red WiFi para conexion WebSocket
- Servidor WebSocket ejecutandose en la red local

## Instalacion y Configuracion

### Configuracion del Dispositivo
1. Asegurar que el chip Tensosense EFR32 este alimentado y transmitiendo
2. Verificar que los UUIDs de Bluetooth coincidan con la configuracion
3. Confirmar que el servidor WebSocket este ejecutandose en la direccion configurada

### Configuracion de la Aplicacion
1. Abrir la aplicacion Tensosense Beta
2. Conceder permisos de Bluetooth y ubicacion cuando se soliciten
3. La aplicacion iniciara automaticamente el escaneo de dispositivos
4. Seleccionar el dispositivo Tensosense de la lista de dispositivos encontrados

## Uso de la Aplicación

### Conexion Inicial
1. La aplicacion muestra "Scanning for devices..." al iniciar
2. Los dispositivos Tensosense apareceran en la lista automaticamente
3. Tocar el dispositivo deseado para establecer conexion
4. El estado cambiara a "Connected to [nombre_dispositivo]"

### Monitoreo en Tiempo Real
- **Panel Superior**: Graficos de aceleracion y tension en tiempo real
- **Panel Central**: Estado de conexion Bluetooth y WebSocket
- **Panel Inferior**: Lista de dispositivos disponibles y controles

### Eventos y Alertas
- **Alerta Roja**: Caida libre detectada
- **Alerta Naranja**: Vibracion critica detectada
- **Alerta Morada**: Tension critica en el sistema
- **Registro Automatico**: Todos los eventos se guardan con ubicacion GPS

### Exportacion de Datos
- Los archivos CSV se generan automaticamente en el directorio de documentos
- Formato: timestamp, coordenadas GPS, tipo de evento, valor del sensor
- Acceso via iTunes File Sharing o aplicaciones de gestion de archivos

## Soporte Multi-Usuario

La aplicacion soporta conexion simultanea de multiples dispositivos iOS al mismo chip Tensosense EFR32:

- **Conexiones Simultaneas**: Hasta 5 dispositivos iOS pueden conectarse al mismo tiempo
- **Sincronizacion de Datos**: Todos los dispositivos reciben los mismos datos del sensor
- **Gestion de Conflictos**: Sistema automatico de prioridades para comandos de configuracion
- **Distribucion de Carga**: El procesamiento se distribuye entre dispositivos conectados

## Estructura de Archivos de Datos

### Archivo de Eventos (trip_YYYY-MM-DD.csv)
```
ID,Latitude,Longitude,Timestamp,EventType,EventValue,Accuracy
uuid,lat,lng,2025-01-02 15:30:45,Fall,0.05,5.2
uuid,lat,lng,2025-01-02 15:31:20,Oscillation,8.5,3.1
```

### Archivo de Caidas (fall_detection_YYYY-MM-DD_HH-mm-ss.csv)
```
Tiempo,Aceleracion
1704204645.123,0.05
1704204645.124,0.03
```

## Especificaciones de Red

### Configuracion WebSocket
- **Puerto**: 8000
- **Protocolo**: WebSocket (ws://)
- **Formato Mensaje**: JSON
- **Frecuencia Envio**: Tiempo real (cada lectura de sensor)
- **Manejo de Errores**: Reconexion automatica con backoff exponencial

### Ejemplo de Datos Transmitidos
```json
{
  "time": 1704204645.123,
  "value": 1.05,
  "sensor_type": "acceleration",
  "device_id": "tensosense_001"
}
```

## Solución de Problemas

### Problemas de Conexion Bluetooth
- Verificar que el dispositivo Tensosense este encendido y en rango
- Reiniciar Bluetooth en el dispositivo iOS
- Limpiar la lista de dispositivos usando el boton "Clear Devices"

### Problemas de WebSocket
- Confirmar que el servidor este ejecutandose en la direccion IP correcta
- Verificar conectividad de red WiFi
- Revisar firewall y configuracion de puerto 8000

### Problemas de Datos GPS
- Asegurar que los permisos de ubicacion esten habilitados
- Verificar que el dispositivo tenga señal GPS clara
- Reiniciar la aplicacion si los datos de ubicacion no se actualizan

## Mantenimiento y Actualizaciones

### Limpieza de Datos
- La aplicacion limpia automaticamente dispositivos antiguos cada 30 segundos
- Los archivos CSV se mantienen indefinidamente hasta eliminacion manual
- Buffer de datos limitado a 100 lecturas para optimizar memoria

### Monitoreo de Rendimiento
- Estado de conexion visible en tiempo real
- Indicadores de calidad de señal Bluetooth
- Registro de errores en consola para diagnostico

## Seguridad y Privacidad

- Los datos se almacenan localmente en el dispositivo iOS
- Transmision WebSocket sin encriptacion (adecuado para redes locales)
- Datos GPS solo se capturan durante eventos criticos
- No se transmite informacion personal del usuario

## Soporte Tecnico

Para problemas relacionados con:
- **Hardware Tensosense**: Contactar al fabricante del chip EFR32
- **Configuracion de Red**: Verificar documentacion del servidor WebSocket
- **Funcionamiento iOS**: Revisar permisos y configuracion del dispositivo

---

**Version**: Beta 1.0  
**Compatibilidad**: iOS 14.0+  
**Ultima Actualizacion**: Enero 2025  
**Desarrollador**: José Heinz Möller Santos