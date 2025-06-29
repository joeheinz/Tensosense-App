const express = require('express');
const WebSocket = require('ws');
const http = require('http');
const path = require('path');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const cors = require('cors');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server, path: '/ws' });

// Configuración
const PORT = process.env.PORT || 8000;
const JWT_SECRET = process.env.JWT_SECRET || 'tensosense-secret-key-2025';

// Usuarios de prueba (en producción usar base de datos)
const users = [
    {
        id: 1,
        username: 'admin',
        password: '$2a$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2.uheWG/igi', // password
        role: 'admin'
    },
    {
        id: 2,
        username: 'tensosense',
        password: '$2a$10$92IXUNpkjO0rOQ5byMi.Ye4oKoEa3Ro9llC/.og/at2.uheWG/igi', // password
        role: 'user'
    }
];

// Almacenamiento temporal de datos
let connectedDevices = new Map();
let sensorData = {
    acceleration: [],
    tension: [],
    lastUpdate: null
};

// Middleware
app.use(express.json());
app.use(cors());
app.use(express.static(path.join(__dirname, 'public')));

// Rutas de autenticación
app.post('/api/login', async (req, res) => {
    const { username, password } = req.body;
    
    const user = users.find(u => u.username === username);
    if (!user) {
        return res.status(401).json({ error: 'Usuario no encontrado' });
    }
    
    const validPassword = await bcrypt.compare(password, user.password);
    if (!validPassword) {
        return res.status(401).json({ error: 'Contraseña incorrecta' });
    }
    
    const token = jwt.sign(
        { id: user.id, username: user.username, role: user.role },
        JWT_SECRET,
        { expiresIn: '24h' }
    );
    
    res.json({
        token,
        user: {
            id: user.id,
            username: user.username,
            role: user.role
        }
    });
});

// Middleware de autenticación para WebSocket
function authenticateToken(token) {
    try {
        return jwt.verify(token, JWT_SECRET);
    } catch (err) {
        return null;
    }
}

// Configuración WebSocket
wss.on('connection', (ws, req) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    const token = url.searchParams.get('token');
    
    // Autenticar conexión WebSocket
    const user = authenticateToken(token);
    if (!user) {
        ws.close(1008, 'Token inválido');
        return;
    }
    
    ws.user = user;
    const deviceId = `device_${Date.now()}`;
    ws.deviceId = deviceId;
    
    connectedDevices.set(deviceId, {
        ws,
        user,
        lastSeen: Date.now(),
        dataCount: 0
    });
    
    console.log(`📱 Dispositivo conectado: ${deviceId} (Usuario: ${user.username})`);
    
    // Enviar datos históricos al conectarse
    ws.send(JSON.stringify({
        type: 'initial_data',
        data: {
            acceleration: sensorData.acceleration.slice(-100),
            tension: sensorData.tension.slice(-100),
            connectedDevices: connectedDevices.size
        }
    }));
    
    // Broadcast a todos los clientes sobre nuevo dispositivo
    broadcastToClients({
        type: 'device_connected',
        deviceId,
        connectedDevices: connectedDevices.size,
        username: user.username
    });
    
    ws.on('message', (data) => {
        try {
            const message = JSON.parse(data.toString());
            handleSensorData(ws, message);
        } catch (error) {
            // Si no es JSON, tratar como datos de sensor directos (compatibilidad con iOS)
            try {
                const sensorMessage = JSON.parse(data.toString());
                if (sensorMessage.time && sensorMessage.value !== undefined) {
                    handleSensorData(ws, sensorMessage);
                }
            } catch (parseError) {
                console.error('Error parsing message:', parseError);
            }
        }
    });
    
    ws.on('close', () => {
        connectedDevices.delete(deviceId);
        console.log(`📱 Dispositivo desconectado: ${deviceId}`);
        
        broadcastToClients({
            type: 'device_disconnected',
            deviceId,
            connectedDevices: connectedDevices.size
        });
    });
    
    ws.on('error', (error) => {
        console.error('WebSocket error:', error);
        connectedDevices.delete(deviceId);
    });
});

function handleSensorData(ws, data) {
    const device = connectedDevices.get(ws.deviceId);
    if (!device) return;
    
    device.lastSeen = Date.now();
    device.dataCount++;
    
    // Determinar tipo de dato basado en el valor
    // Valores típicos: aceleración (0-20), tensión ADC (1000-3000)
    const isAcceleration = data.value < 50;
    const dataPoint = {
        time: data.time || Date.now() / 1000,
        value: data.value,
        deviceId: ws.deviceId,
        username: ws.user.username,
        timestamp: new Date().toISOString()
    };
    
    if (isAcceleration) {
        sensorData.acceleration.push(dataPoint);
        // Mantener solo los últimos 1000 puntos
        if (sensorData.acceleration.length > 1000) {
            sensorData.acceleration = sensorData.acceleration.slice(-1000);
        }
    } else {
        sensorData.tension.push(dataPoint);
        if (sensorData.tension.length > 1000) {
            sensorData.tension = sensorData.tension.slice(-1000);
        }
    }
    
    sensorData.lastUpdate = Date.now();
    
    // Broadcast datos en tiempo real a todos los clientes web
    broadcastToClients({
        type: isAcceleration ? 'acceleration_data' : 'tension_data',
        data: dataPoint,
        stats: {
            totalDevices: connectedDevices.size,
            totalDataPoints: device.dataCount,
            lastUpdate: sensorData.lastUpdate
        }
    });
}

function broadcastToClients(message) {
    const messageStr = JSON.stringify(message);
    connectedDevices.forEach((device) => {
        if (device.ws.readyState === WebSocket.OPEN) {
            device.ws.send(messageStr);
        }
    });
}

// API para obtener datos históricos
app.get('/api/data', (req, res) => {
    const { type, limit = 100 } = req.query;
    
    let data;
    switch (type) {
        case 'acceleration':
            data = sensorData.acceleration.slice(-limit);
            break;
        case 'tension':
            data = sensorData.tension.slice(-limit);
            break;
        default:
            data = {
                acceleration: sensorData.acceleration.slice(-limit),
                tension: sensorData.tension.slice(-limit)
            };
    }
    
    res.json({
        data,
        connectedDevices: connectedDevices.size,
        lastUpdate: sensorData.lastUpdate
    });
});

// API para estadísticas
app.get('/api/stats', (req, res) => {
    const deviceStats = Array.from(connectedDevices.entries()).map(([id, device]) => ({
        deviceId: id,
        username: device.user.username,
        dataCount: device.dataCount,
        lastSeen: device.lastSeen,
        connected: device.ws.readyState === WebSocket.OPEN
    }));
    
    res.json({
        connectedDevices: connectedDevices.size,
        totalAccelerationPoints: sensorData.acceleration.length,
        totalTensionPoints: sensorData.tension.length,
        lastUpdate: sensorData.lastUpdate,
        devices: deviceStats
    });
});

// Servir la aplicación web
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Iniciar servidor
server.listen(PORT, () => {
    console.log(`🚀 Tensosense WebSocket Server running on http://localhost:${PORT}`);
    console.log(`📡 WebSocket endpoint: ws://localhost:${PORT}/ws`);
    console.log(`🌐 Web interface: http://localhost:${PORT}`);
});

// Limpiar conexiones inactivas cada 30 segundos
setInterval(() => {
    const now = Date.now();
    connectedDevices.forEach((device, deviceId) => {
        if (now - device.lastSeen > 60000) { // 1 minuto sin actividad
            console.log(`🧹 Limpiando dispositivo inactivo: ${deviceId}`);
            device.ws.terminate();
            connectedDevices.delete(deviceId);
        }
    });
}, 30000);