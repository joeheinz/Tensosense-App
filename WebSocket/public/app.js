class TensosenseDashboard {
    constructor() {
        this.ws = null;
        this.token = localStorage.getItem('tensosense_token');
        this.user = null;
        this.isConnected = false;
        this.charts = {};
        this.map = null;
        this.markers = [];
        this.chartData = {
            acceleration: [],
            tension: []
        };
        this.chartsPaused = {
            acceleration: false,
            tension: false
        };
        
        this.init();
    }

    init() {
        this.setupEventListeners();
        
        // Verificar si ya está autenticado
        if (this.token) {
            this.validateToken();
        } else {
            this.showLogin();
        }
    }

    setupEventListeners() {
        // Login form
        const loginForm = document.getElementById('loginForm');
        if (loginForm) {
            loginForm.addEventListener('submit', (e) => this.handleLogin(e));
        }

        // Logout button
        const logoutBtn = document.getElementById('logoutBtn');
        if (logoutBtn) {
            logoutBtn.addEventListener('click', () => this.logout());
        }

        // Chart controls
        document.addEventListener('click', (e) => {
            if (e.target.classList.contains('chart-btn')) {
                this.handleChartControl(e.target);
            }
        });

        // Map controls
        const centerMapBtn = document.getElementById('centerMap');
        if (centerMapBtn) {
            centerMapBtn.addEventListener('click', () => this.centerMap());
        }

        const toggleMarkersBtn = document.getElementById('toggleMarkers');
        if (toggleMarkersBtn) {
            toggleMarkersBtn.addEventListener('click', () => this.toggleMarkers());
        }

        // Refresh devices
        const refreshDevicesBtn = document.getElementById('refreshDevices');
        if (refreshDevicesBtn) {
            refreshDevicesBtn.addEventListener('click', () => this.refreshDevices());
        }
    }

    async validateToken() {
        try {
            const response = await fetch('/api/stats', {
                headers: {
                    'Authorization': `Bearer ${this.token}`
                }
            });

            if (response.ok) {
                // Token válido, mostrar dashboard
                this.showDashboard();
            } else {
                // Token inválido, mostrar login
                localStorage.removeItem('tensosense_token');
                this.showLogin();
            }
        } catch (error) {
            console.error('Error validating token:', error);
            this.showLogin();
        }
    }

    async handleLogin(e) {
        e.preventDefault();
        
        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;
        const submitBtn = e.target.querySelector('button[type="submit"]');
        const buttonText = submitBtn.querySelector('.button-text');
        const spinner = submitBtn.querySelector('.loading-spinner');
        const errorDiv = document.getElementById('loginError');

        // Mostrar loading
        buttonText.style.display = 'none';
        spinner.style.display = 'block';
        submitBtn.disabled = true;
        errorDiv.style.display = 'none';

        try {
            const response = await fetch('/api/login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ username, password })
            });

            const data = await response.json();

            if (response.ok) {
                this.token = data.token;
                this.user = data.user;
                localStorage.setItem('tensosense_token', this.token);
                this.showDashboard();
            } else {
                errorDiv.textContent = data.error || 'Error de autenticación';
                errorDiv.style.display = 'block';
            }
        } catch (error) {
            console.error('Login error:', error);
            errorDiv.textContent = 'Error de conexión al servidor';
            errorDiv.style.display = 'block';
        } finally {
            // Ocultar loading
            buttonText.style.display = 'block';
            spinner.style.display = 'none';
            submitBtn.disabled = false;
        }
    }

    logout() {
        localStorage.removeItem('tensosense_token');
        this.token = null;
        this.user = null;
        this.disconnect();
        this.showLogin();
    }

    showLogin() {
        document.getElementById('loginScreen').style.display = 'flex';
        document.getElementById('dashboard').style.display = 'none';
    }

    showDashboard() {
        document.getElementById('loginScreen').style.display = 'none';
        document.getElementById('dashboard').style.display = 'block';
        
        // Configurar usuario en header
        if (this.user) {
            document.getElementById('username-display').textContent = this.user.username;
        }
        
        // Inicializar dashboard
        this.initializeCharts();
        this.initializeMap();
        this.connectWebSocket();
        this.loadInitialData();
        
        // Auto-refresh stats cada 30 segundos
        setInterval(() => this.refreshStats(), 30000);
    }

    connectWebSocket() {
        const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${location.host}/ws?token=${this.token}`;
        
        this.ws = new WebSocket(wsUrl);
        
        this.ws.onopen = () => {
            console.log('🔗 WebSocket conectado');
            this.isConnected = true;
            this.updateConnectionStatus(true);
        };
        
        this.ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            this.handleWebSocketMessage(data);
        };
        
        this.ws.onclose = () => {
            console.log('❌ WebSocket desconectado');
            this.isConnected = false;
            this.updateConnectionStatus(false);
            
            // Intentar reconectar en 3 segundos
            setTimeout(() => {
                if (this.token) {
                    this.connectWebSocket();
                }
            }, 3000);
        };
        
        this.ws.onerror = (error) => {
            console.error('Error WebSocket:', error);
        };
    }

    disconnect() {
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
    }

    handleWebSocketMessage(data) {
        switch (data.type) {
            case 'initial_data':
                this.loadInitialChartData(data.data);
                break;
            case 'acceleration_data':
                this.addDataPoint('acceleration', data.data);
                break;
            case 'tension_data':
                this.addDataPoint('tension', data.data);
                break;
            case 'device_connected':
            case 'device_disconnected':
                this.refreshStats();
                this.refreshDevices();
                break;
        }
        
        // Actualizar estadísticas si están incluidas
        if (data.stats) {
            this.updateStats(data.stats);
        }
    }

    initializeCharts() {
        // Chart de Aceleración
        const accelCtx = document.getElementById('accelerationChart').getContext('2d');
        this.charts.acceleration = new Chart(accelCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Aceleración (m/s²)',
                    data: [],
                    borderColor: '#3282b8',
                    backgroundColor: 'rgba(50, 130, 184, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4,
                    pointRadius: 0,
                    pointHoverRadius: 4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        type: 'linear',
                        position: 'bottom',
                        title: {
                            display: true,
                            text: 'Tiempo (s)',
                            color: '#b0b0b0'
                        },
                        grid: {
                            color: 'rgba(176, 176, 176, 0.1)'
                        },
                        ticks: {
                            color: '#b0b0b0'
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: 'Aceleración (m/s²)',
                            color: '#b0b0b0'
                        },
                        grid: {
                            color: 'rgba(176, 176, 176, 0.1)'
                        },
                        ticks: {
                            color: '#b0b0b0'
                        }
                    }
                },
                plugins: {
                    legend: {
                        labels: {
                            color: '#b0b0b0'
                        }
                    }
                },
                animation: {
                    duration: 0
                }
            }
        });

        // Chart de Tensión
        const tensionCtx = document.getElementById('tensionChart').getContext('2d');
        this.charts.tension = new Chart(tensionCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Tensión ADC',
                    data: [],
                    borderColor: '#f39c12',
                    backgroundColor: 'rgba(243, 156, 18, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4,
                    pointRadius: 0,
                    pointHoverRadius: 4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        type: 'linear',
                        position: 'bottom',
                        title: {
                            display: true,
                            text: 'Tiempo (s)',
                            color: '#b0b0b0'
                        },
                        grid: {
                            color: 'rgba(176, 176, 176, 0.1)'
                        },
                        ticks: {
                            color: '#b0b0b0'
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: 'Valor ADC',
                            color: '#b0b0b0'
                        },
                        grid: {
                            color: 'rgba(176, 176, 176, 0.1)'
                        },
                        ticks: {
                            color: '#b0b0b0'
                        }
                    }
                },
                plugins: {
                    legend: {
                        labels: {
                            color: '#b0b0b0'
                        }
                    }
                },
                animation: {
                    duration: 0
                }
            }
        });
    }

    initializeMap() {
        this.map = L.map('map').setView([51.505, -0.09], 13);
        
        L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
            attribution: '© OpenStreetMap contributors'
        }).addTo(this.map);
        
        // Estilo oscuro para el mapa
        const style = document.createElement('style');
        style.textContent = `
            .leaflet-container {
                background: #1a1a2e;
                color: #ffffff;
            }
            .leaflet-tile {
                filter: hue-rotate(180deg) invert(1) contrast(1.2) brightness(0.7);
            }
            .leaflet-control-container .leaflet-control {
                background: #2c2c54;
                border: 1px solid #3c3c74;
            }
            .leaflet-control-container .leaflet-control a {
                background: #2c2c54;
                color: #ffffff;
            }
        `;
        document.head.appendChild(style);
    }

    loadInitialData() {
        this.refreshStats();
        this.refreshDevices();
    }

    loadInitialChartData(data) {
        if (data.acceleration) {
            this.chartData.acceleration = data.acceleration.slice(-50);
            this.updateChart('acceleration');
        }
        
        if (data.tension) {
            this.chartData.tension = data.tension.slice(-50);
            this.updateChart('tension');
        }
    }

    addDataPoint(type, dataPoint) {
        if (!this.chartsPaused[type]) {
            this.chartData[type].push(dataPoint);
            
            // Mantener solo los últimos 100 puntos
            if (this.chartData[type].length > 100) {
                this.chartData[type] = this.chartData[type].slice(-100);
            }
            
            this.updateChart(type);
        }
    }

    updateChart(type) {
        const chart = this.charts[type];
        if (!chart) return;
        
        const data = this.chartData[type];
        const now = Date.now() / 1000;
        
        chart.data.labels = data.map(point => now - (now - point.time));
        chart.data.datasets[0].data = data.map(point => ({
            x: point.time,
            y: point.value
        }));
        
        chart.update('none');
    }

    handleChartControl(button) {
        const chartType = button.dataset.chart;
        const action = button.dataset.action;
        
        // Remover clase active de todos los botones del mismo chart
        const chartButtons = document.querySelectorAll(`[data-chart="${chartType}"]`);
        chartButtons.forEach(btn => btn.classList.remove('active'));
        
        switch (action) {
            case 'play':
                this.chartsPaused[chartType] = false;
                button.classList.add('active');
                break;
            case 'pause':
                this.chartsPaused[chartType] = true;
                button.classList.add('active');
                break;
            case 'clear':
                this.chartData[chartType] = [];
                this.updateChart(chartType);
                // Reactivar el botón play
                document.querySelector(`[data-chart="${chartType}"][data-action="play"]`).classList.add('active');
                break;
        }
    }

    centerMap() {
        if (this.map && this.markers.length > 0) {
            const group = new L.featureGroup(this.markers);
            this.map.fitBounds(group.getBounds().pad(0.1));
        } else {
            // Centrar en ubicación por defecto
            this.map.setView([51.505, -0.09], 13);
        }
    }

    toggleMarkers() {
        // Implementar lógica para mostrar/ocultar marcadores
        this.markers.forEach(marker => {
            if (this.map.hasLayer(marker)) {
                this.map.removeLayer(marker);
            } else {
                this.map.addLayer(marker);
            }
        });
    }

    async refreshStats() {
        try {
            const response = await fetch('/api/stats');
            if (response.ok) {
                const stats = await response.json();
                this.updateStats(stats);
            }
        } catch (error) {
            console.error('Error refreshing stats:', error);
        }
    }

    updateStats(stats) {
        document.getElementById('connectedDevices').textContent = stats.connectedDevices || 0;
        document.getElementById('accelerationPoints').textContent = stats.totalAccelerationPoints || 0;
        document.getElementById('tensionPoints').textContent = stats.totalTensionPoints || 0;
        
        const lastUpdate = stats.lastUpdate ? 
            new Date(stats.lastUpdate).toLocaleTimeString() : '--';
        document.getElementById('lastUpdate').textContent = lastUpdate;
    }

    async refreshDevices() {
        try {
            const response = await fetch('/api/stats');
            if (response.ok) {
                const data = await response.json();
                this.updateDeviceList(data.devices || []);
            }
        } catch (error) {
            console.error('Error refreshing devices:', error);
        }
    }

    updateDeviceList(devices) {
        const deviceList = document.getElementById('deviceList');
        
        if (devices.length === 0) {
            deviceList.innerHTML = '<div class="no-devices">No hay dispositivos conectados</div>';
            return;
        }
        
        deviceList.innerHTML = devices.map(device => `
            <div class="device-card">
                <div class="device-info">
                    <div class="device-name">${device.username} - ${device.deviceId}</div>
                    <div class="device-status ${device.connected ? 'online' : 'offline'}">
                        ${device.connected ? 'En línea' : 'Desconectado'}
                    </div>
                </div>
                <div class="device-stats">
                    Datos enviados: ${device.dataCount}<br>
                    Última conexión: ${new Date(device.lastSeen).toLocaleString()}
                </div>
            </div>
        `).join('');
    }

    updateConnectionStatus(connected) {
        const statusDot = document.getElementById('connectionDot');
        const statusText = document.getElementById('connectionStatus');
        
        if (connected) {
            statusDot.className = 'status-dot connected';
            statusText.textContent = 'Conectado';
        } else {
            statusDot.className = 'status-dot disconnected';
            statusText.textContent = 'Desconectado';
        }
    }
}

// Inicializar la aplicación cuando se carga la página
document.addEventListener('DOMContentLoaded', () => {
    new TensosenseDashboard();
});