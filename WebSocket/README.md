# 🚀 Tensosense WebSocket Server

Servidor WebSocket moderno con dashboard web para monitorear datos de sensores IoT de la app Tensosense Beta.

## ✨ Características

- 🔐 **Autenticación JWT** segura
- 📊 **Dashboard en tiempo real** con gráficas interactivas
- 🗺️ **Mapa interactivo** para ubicación de dispositivos
- 📱 **Responsive design** - funciona en desktop y móvil
- ⚡ **WebSocket** para comunicación en tiempo real
- 🎨 **Diseño moderno** con tema oscuro

## 🚀 Inicio Rápido

### 1. Instalación

```bash
cd /Users/joseheinz/Documents/GitHub/Tensosense-App/WebSocket
npm install
```

### 2. Ejecutar el Servidor

```bash
npm start
```

El servidor se ejecutará en: `http://localhost:8000`

### 3. Acceder al Dashboard

Abre tu navegador y ve a `http://localhost:8000`

**Credenciales de prueba:**
- Usuario: `admin` / Contraseña: `password`
- Usuario: `tensosense` / Contraseña: `password`

## 📱 Integración con iOS

Ver el archivo `INTEGRATION.md` para instrucciones detalladas de integración con la app Tensosense Beta.

### Configuración Rápida

1. **Obtén tu IP local:**
   ```bash
   ifconfig | grep "inet " | grep -v 127.0.0.1
   ```

2. **Actualiza la IP en tu app iOS** (`AppConfiguration.swift`):
   ```swift
   static let defaultServerURL = "ws://TU_IP_AQUI:8000/ws"
   ```

3. **¡Listo!** Tu app iOS ya puede enviar datos al dashboard.

## 🏗️ Arquitectura

```
├── server.js          # Servidor WebSocket + API REST
├── public/
│   ├── index.html     # Dashboard principal
│   ├── styles.css     # Estilos modernos
│   └── app.js         # Lógica del frontend
├── package.json       # Dependencias
└── INTEGRATION.md     # Guía de integración iOS
```

## 📊 Datos Soportados

### Aceleración
- **Rango:** 0-50 (detectado automáticamente)
- **Formato:** `{time: timestamp, value: acceleration}`
- **Visualización:** Gráfica azul en tiempo real

### Tensión ADC  
- **Rango:** >50 (detectado automáticamente)
- **Formato:** `{time: timestamp, value: adcValue}`
- **Visualización:** Gráfica naranja en tiempo real

## 🛠️ API Endpoints

- `POST /api/login` - Autenticación de usuarios
- `GET /api/data` - Obtener datos históricos
- `GET /api/stats` - Estadísticas del sistema
- `WebSocket /ws` - Conexión en tiempo real

## 🔧 Desarrollo

```bash
# Modo desarrollo con auto-restart
npm run dev

# Producción
npm start
```

## 📋 TODO

- [ ] Base de datos persistente (SQLite/PostgreSQL)
- [ ] Configuración de usuarios via web
- [ ] Exportar datos a CSV/Excel
- [ ] Alertas y notificaciones
- [ ] Modo oscuro/claro

## 🤝 Contribuir

1. Fork el proyecto
2. Crea tu branch (`git checkout -b feature/AmazingFeature`)
3. Commit tus cambios (`git commit -m 'Add AmazingFeature'`)
4. Push a la branch (`git push origin feature/AmazingFeature`)
5. Abre un Pull Request

## 📄 Licencia

MIT License - ver `LICENSE` para más detalles.

---

Desarrollado con ❤️ para el proyecto Tensosense