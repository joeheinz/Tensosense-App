# BASELINE - Dark Knight Programmer

Este es el punto de control del código que funciona correctamente.

## ✅ Estado del Baseline

**Fecha:** 2025-01-22  
**Funcionalidad:** Botón envía "Boton Apretado" por Bluetooth  

### Qué funciona:

1. **Detección de botón**: Pin PB02 detecta presionar/soltar
2. **Bluetooth**: Envía "Boton Apretado" cuando se presiona el botón
3. **Logs**: Debug completo por puerto serial
4. **Estructura estable**: Basado en ejemplo Blinky que funciona

### Archivos de baseline:

- `app_baseline.c` - Código principal funcional
- `bluetooth_receiver.py` - Receptor Python para Bluetooth
- `dual_receiver.py` - Monitor dual (Bluetooth + Serial)
- `run_receiver.sh` - Script para ejecutar el monitor
- `requirements.txt` - Dependencias Python

### Cómo usar el baseline:

```bash
# Para restaurar el código funcional:
cp app_baseline.c app.c

# Para usar el monitor:
./run_receiver.sh
```

### Configuración del botón:

- **Pin**: PB02 (Puerto B, Pin 2)
- **Modo**: Interrupt
- **Función**: Solo detecta cuando se presiona (no cuando se suelta)

### Mensajes esperados:

**Por serial:**
```
[SERIAL] Button PRESSED - sending binary: 0x01
[SERIAL] Attribute written: 0x01
[SERIAL] Sending notification: Boton Apretado (len=14)
[SERIAL] SUCCESS: String notification sent!
```

**Por Bluetooth:**
```
[BLE] Received: 'Boton Apretado'
[BLE] Raw data: 426f746f6e20417072657461646f (length: 14)
[BLE] *** BUTTON PRESSED DETECTED! ***
```

### Características técnicas:

- **GATT**: Usa datos binarios en la base de datos
- **Notifications**: Envía string directamente (bypass GATT para el mensaje)
- **Error handling**: Logs detallados de éxito/fallo
- **Compatibilidad**: Mantiene estructura del ejemplo Blinky original

---

**IMPORTANTE**: Este baseline es el punto de retorno seguro. Cualquier modificación experimental debe poder regresar a este estado funcional.