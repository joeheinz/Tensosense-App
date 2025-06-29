# Integración con App iOS Tensosense Beta

## 📱 Configuración de la App iOS

### 1. Actualizar URL del Servidor WebSocket

En tu app iOS, actualiza la URL del servidor WebSocket en `AppConfiguration.swift`:

```swift
struct WebSocket {
    static let defaultServerURL = "ws://TU_IP_AQUI:8000/ws"  // Cambiar por tu IP
    static let connectionTimeout: TimeInterval = 5
    static let reconnectDelay: TimeInterval = 3
}
```

**Importante:** Cambia `TU_IP_AQUI` por la IP de tu computadora donde corre el servidor.

### 2. Modificación del WebSocketManager (Opcional)

Tu `WebSocketManager.swift` actual ya es compatible, pero puedes agregar autenticación:

```swift
func connectWithAuth(username: String, password: String) {
    // Hacer login primero
    authenticateUser(username: username, password: password) { [weak self] token in
        guard let token = token else { return }
        
        // Conectar con token
        var urlComponents = URLComponents(string: self?.serverURL ?? "")
        urlComponents?.queryItems = [URLQueryItem(name: "token", value: token)]
        
        guard let url = urlComponents?.url else { return }
        self?.webSocket = URLSession.shared.webSocketTask(with: url)
        self?.webSocket?.resume()
    }
}

private func authenticateUser(username: String, password: String, completion: @escaping (String?) -> Void) {
    let loginURL = "http://TU_IP_AQUI:8000/api/login" // Usar HTTP para login
    guard let url = URL(string: loginURL) else { 
        completion(nil)
        return 
    }
    
    let loginData = ["username": username, "password": password]
    guard let jsonData = try? JSONSerialization.data(withJSONObject: loginData) else {
        completion(nil)
        return
    }
    
    var request = URLRequest(url: url)
    request.httpMethod = "POST"
    request.setValue("application/json", forHTTPHeaderField: "Content-Type")
    request.httpBody = jsonData
    
    URLSession.shared.dataTask(with: request) { data, response, error in
        guard let data = data,
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let token = json["token"] as? String else {
            completion(nil)
            return
        }
        completion(token)
    }.resume()
}
```

## 🚀 Uso de la Interfaz Web

### 1. Iniciar el Servidor

```bash
cd /Users/joseheinz/Documents/GitHub/Tensosense-App/WebSocket
npm install
npm start
```

El servidor se ejecutará en: `http://localhost:8000`

### 2. Acceder al Dashboard

1. Abre tu navegador y ve a `http://localhost:8000`
2. Usa las credenciales de prueba:
   - **Usuario:** `admin` **Contraseña:** `password`
   - **Usuario:** `tensosense` **Contraseña:** `password`

### 3. Características del Dashboard

- ✅ **Login seguro** con JWT
- ✅ **Gráficas en tiempo real** de aceleración y tensión
- ✅ **Mapa interactivo** para ubicación de dispositivos
- ✅ **Lista de dispositivos** conectados
- ✅ **Estadísticas en tiempo real**
- ✅ **Diseño moderno y responsive**

## 🔧 Configuración de Red

### Obtener tu IP local

**En macOS/Linux:**
```bash
ifconfig | grep "inet " | grep -v 127.0.0.1
```

**En Windows:**
```cmd
ipconfig
```

### Configurar Firewall

Asegúrate de que el puerto 8000 esté abierto en tu firewall para permitir conexiones desde dispositivos iOS.

## 📊 Estructura de Datos

### Datos de Aceleración (desde iOS)
```json
{
  "time": 1704326400.123,
  "value": 9.81
}
```

### Datos de Tensión ADC (desde iOS)
```json
{
  "time": 1704326400.456,
  "value": 2100
}
```

### Respuesta del Servidor
```json
{
  "type": "acceleration_data",
  "data": {
    "time": 1704326400.123,
    "value": 9.81,
    "deviceId": "device_1704326400000",
    "username": "tensosense",
    "timestamp": "2024-01-04T10:00:00.123Z"
  },
  "stats": {
    "totalDevices": 1,
    "totalDataPoints": 42,
    "lastUpdate": 1704326400123
  }
}
```

## 🔐 Seguridad

- Las contraseñas están hasheadas con bcrypt
- Autenticación JWT con expiración de 24 horas
- Validación de tokens en conexiones WebSocket
- CORS habilitado para desarrollo

## 🚨 Detección de Eventos

El servidor automáticamente:
- Diferencia entre datos de aceleración (<50) y tensión (>50)
- Mantiene historial de los últimos 1000 puntos por tipo
- Limpia conexiones inactivas cada 30 segundos
- Actualiza estadísticas en tiempo real

## 📱 Próximos Pasos para iOS

1. **Actualizar la IP** en `AppConfiguration.swift`
2. **Opcional:** Implementar autenticación en la app
3. **Opcional:** Agregar configuración de usuario en la app
4. **Testear** la conexión enviando datos de sensores

La integración está diseñada para ser **plug-and-play** con tu app iOS actual. Solo necesitas cambiar la IP del servidor y ya debería funcionar!

## 🐛 Troubleshooting

### Problemas Comunes

1. **No se conecta desde iOS:**
   - Verificar que la IP sea correcta
   - Comprobar que el puerto 8000 esté abierto
   - Revisar logs del servidor

2. **Datos no aparecen en el dashboard:**
   - Verificar que los datos tengan formato `{time, value}`
   - Comprobar que la conexión WebSocket esté activa

3. **Error de autenticación:**
   - Verificar credenciales en el dashboard
   - Revisar que el token JWT sea válido

### Logs del Servidor

El servidor muestra logs detallados:
- `📱 Dispositivo conectado/desconectado`
- `🔗 WebSocket conectado`
- `❌ Errores de conexión`