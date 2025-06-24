/***************************************************************************//**
 * Initialize application.
 * G2 electronics UG - Tensosense
 ******************************************************************************/

#include "em_common.h"
#include "app_assert.h"
#include "app_log.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "sl_sleeptimer.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "sl_simple_led_instances.h"
#include "em_iadc.h"
#include "sl_bt_api.h" //For event handling function sl_bt_on_event
#include "sl_imu.h" //For the IMU sensor
#include "sl_sensor_imu.h"
#include <stdint.h>
#include <stdio.h>


#define IMU_UPDATE_INTERVAL_MS 10
#define ADV_INTERVAL_MS        100

static uint8_t advertising_set_handle = 0xff;

/**
 * FUNCION PARA EL POSICIONAMIENTO DEL DISPOSITIVO! 13.03.2025 con python transmitPower.py!!
 */
#define GATTDB_RSSI_LEVEL gattdb_tx_power_level  // Característica GATT para enviar el RSSI

void send_rssi(void) {
    sl_status_t sc;
    int8_t median_rssi;

    // Obtener el RSSI mediano de la conexión activa
    sc = sl_bt_connection_get_median_rssi(advertising_set_handle, &median_rssi);
    if (sc == SL_STATUS_OK) {
        app_log_info("RSSI Mediano: %d dBm\n", median_rssi);
    } else {
        app_log_error("Error al obtener RSSI Mediano: 0x%04X\n", (unsigned int)sc);
        return;
    }

    // Convertir a un valor de 8 bits antes de enviarlo por BLE
    uint8_t rssi_data = (uint8_t)median_rssi;

    // Escribir RSSI en la característica GATT
    sc = sl_bt_gatt_server_write_attribute_value(GATTDB_RSSI_LEVEL, 0, sizeof(rssi_data), &rssi_data);
    if (sc == SL_STATUS_OK) {
        app_log_info("RSSI escrito en GATT: %d dBm\n", median_rssi);
    } else {
        app_log_error("Error al escribir RSSI en GATT: 0x%04X\n", (unsigned int)sc);
        return;
    }

    // Notificar RSSI a los dispositivos conectados
    sc = sl_bt_gatt_server_notify_all(GATTDB_RSSI_LEVEL, sizeof(rssi_data), &rssi_data);
    if (sc == SL_STATUS_OK) {
        app_log_info("Notificación BLE enviada con RSSI: %d dBm\n", median_rssi);
    } else {
        app_log_error("Error al notificar RSSI: 0x%04X\n", (unsigned int)sc);
    }
}

/**
 * FILTRO FIR SENCILLO
 */
#define FIR_TAP_NUM 5  // Número de coeficientes del filtro (orden del filtro)

// Coeficientes del Filtro FIR (Ejemplo: filtro pasa-bajas)
const float firCoeffs[FIR_TAP_NUM] = {0.2, 0.2, 0.2, 0.2, 0.2}; // Media móvil simple

// Buffer circular para almacenar muestras pasadas
float firBuffer[FIR_TAP_NUM] = {0};
uint8_t bufferIndex = 0;

// === [Función del Filtro FIR] ===
float applyFIR(float newSample) {
    // Insertar nueva muestra en el buffer circular
    firBuffer[bufferIndex] = newSample;
    bufferIndex = (bufferIndex + 1) % FIR_TAP_NUM; // Mover índice circularmente

    // Aplicar convolución FIR
    float output = 0.0;
    uint8_t i, j = bufferIndex;
    for (i = 0; i < FIR_TAP_NUM; i++) {
        j = (j != 0) ? (j - 1) : (FIR_TAP_NUM - 1); // Acceder a muestras en orden inverso
        output += firCoeffs[i] * firBuffer[j]; // Multiplicar y acumular
    }
    return output;
}

/**
 * FILTRO FIR MEJORADO CON VENTANA DE HAMMING
 */
#define FIR_TAP_NUM_NEW 7  // Mayor número de coeficientes para una mejor atenuación del ruido

// Coeficientes del Filtro FIR (Ventana de Hamming - mejor respuesta de frecuencia)
const float firCoeffs_new[FIR_TAP_NUM_NEW] = {0.05, 0.12, 0.18, 0.30, 0.18, 0.12, 0.05};

// Buffer circular para almacenar muestras pasadas
static float firBuffer_NEW[FIR_TAP_NUM_NEW] = {0};
static uint8_t bufferIndex_new = 0;

// === [Función del Filtro FIR Mejorado] ===
float applyFIR_Hamming(float newSample) {
    // Insertar nueva muestra en el buffer circular
    firBuffer_NEW[bufferIndex_new] = newSample;
    bufferIndex_new = (bufferIndex_new + 1) % FIR_TAP_NUM_NEW; // Mover índice circularmente

    // Aplicar convolución FIR
    float output = 0.0;
    uint8_t i, j = bufferIndex_new;

    for (i = 0; i < FIR_TAP_NUM_NEW; i++) {
        j = (j != 0) ? (j - 1) : (FIR_TAP_NUM_NEW - 1); // Recorrer muestras en orden inverso
        output += firCoeffs_new[i] * firBuffer_NEW[j]; // Multiplicar por coeficientes y acumular
    }

    return output;
}

/**
 * FOR THE IMU DATA PROCESSING
 */
// Estructura unificada para datos IMU
typedef struct {
    int16_t accel[3];
    int16_t orient[3];
} imu_data_t;

// ======== [FUNCIÓN BLE UNIFICADA] ========
static void update_and_notify(uint16_t characteristic, const int16_t *data) {
    uint8_t packet[6];
    for (int i = 0; i < 3; i++) {
        packet[i*2] = data[i];
        packet[(i*2)+1] = (data[i] >> 8) & 0xFF;
    }

    sl_status_t sc = sl_bt_gatt_server_write_attribute_value(characteristic, 0, sizeof(packet), packet);
    if (sc == SL_STATUS_OK) {
        sc = sl_bt_gatt_server_notify_all(characteristic, sizeof(packet), packet);
    }
    app_log_status(sc);
}

void handle_imu_data(void) {
    imu_data_t data;
    sl_imu_update();

    sl_imu_get_acceleration(data.accel);
    sl_imu_get_orientation(data.orient);

    // Logging unificado
    app_log_info("Accel: X:%-5d Y:%-5d Z:%-5d | Orient: X:%-5d Y:%-5d Z:%-5d\n",
                data.accel[0], data.accel[1], data.accel[2],
                data.orient[0], data.orient[1], data.orient[2]);

    // Actualizar y notificar
    update_and_notify(gattdb_imu_acceleration, data.accel);
    update_and_notify(gattdb_imu_orientation, data.orient);
}
/**
 * FOR THE IADC
 */
// How many samples to capture
#define NUM_SAMPLES         1024
// Set CLK_ADC to 10 MHz
#define CLK_SRC_ADC_FREQ    20000000  // CLK_SRC_ADC
//#define CLK_ADC_FREQ        10000000  // CLK_ADC - 10 MHz max in normal mode
#define CLK_ADC_FREQ         1000000  // For High resolution
//#define IADC_INPUT_0_PORT_PIN     iadcPosInputPortBPin0 // THIS WORKS!
#define IADC_INPUT_1_PORT_PIN     iadcNegInputGnd    // Ground reference
//#define IADC_INPUT_2_PORT_PIN     iadcNegInputPortBPin3
#define IADC_INPUT_0_PORT_PIN     iadcPosInputPadAna2    // Analog pins
//#define IADC_INPUT_1_PORT_PIN     iadcNegInputPadAna3
#define IADC_INPUT_0_BUS          BBUSALLOC
#define IADC_INPUT_0_BUSALLOC     GPIO_BBUSALLOC_BEVEN0_ADC0
#define IADC_INPUT_1_BUS          BBUSALLOC
#define IADC_INPUT_1_BUSALLOC     GPIO_BBUSALLOC_BODD0_ADC0

/**
 * IADC WORKING FUNCTION!!! WITH THE AMPLIFIER! CON EL AMPLIFICADOR. Funciona
 */
/**
void initIADC(void)
{
  // Declare initialization structures
  IADC_Init_t init = IADC_INIT_DEFAULT;
  IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
  IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;
  // Scan table structure
  IADC_ScanTable_t scanTable = IADC_SCANTABLE_DEFAULT;

  CMU_ClockEnable(cmuClock_IADC0, true);
  CMU_ClockEnable(cmuClock_GPIO, true);

  IADC_reset(IADC0);

  // Use FSRC0 as the IADC clock to allow operation in EM2
  CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_FSRCO);

  // Shutdown between conversions to reduce current
  init.warmup = iadcWarmupNormal;

  // Set HFSCLK prescale
  init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

  // Configure reference voltage and gain
  //initAllConfigs.configs[0].reference = iadcCfgReferenceInt1V2; // Internal reference (1.2V)
  initAllConfigs.configs[0].reference = iadcCfgReferenceVddx; //3,3v
  initAllConfigs.configs[0].vRef = 3300;                        // Reference voltage in mV
  initAllConfigs.configs[0].analogGain = iadcCfgAnalogGain2x;   // Gain of 2x

  initAllConfigs.configs[0].osrHighSpeed = iadcCfgOsrHighSpeed2x; //normally iadcCfgOsrHighSpeed64x
  initAllConfigs.configs[0].digAvg = iadcDigitalAverage16;
  // Set alignment to right justified with 20 bits for data field
  initScan.alignment = iadcAlignRight20;


  // Configure CLK_ADC prescale
  initAllConfigs.configs[0].adcClkPrescale = IADC_calcAdcClkPrescale(IADC0,
                                                                     CLK_ADC_FREQ,
                                                                     0,
                                                                     iadcCfgModeNormal,
                                                                     init.srcClkPrescale);

  // Configure scan mode
  initScan.triggerAction = iadcTriggerActionContinuous; // Continuous scan mode
  initScan.dataValidLevel = iadcFifoCfgDvl4;            // FIFO generates an interrupt after 4 entries
  initScan.fifoDmaWakeup = true;                        // Enable DMA wakeup on FIFO

  // Configure scan table for input channels
  scanTable.entries[0].posInput = IADC_INPUT_0_PORT_PIN; // POSITIVE
  scanTable.entries[0].negInput = IADC_INPUT_1_PORT_PIN; // NEGATIVE
  scanTable.entries[0].includeInScan = true;            // Include this channel in scan


  initAllConfigs.configs[0].twosComplement = iadcCfgTwosCompUnipolar; // Mejor manejo de voltajes pequeños

  // Initialize IADC
  IADC_init(IADC0, &init, &initAllConfigs);
  IADC_initScan(IADC0, &initScan, &scanTable);

  // Configure GPIO analog bus for ADC inputs

  GPIO->IADC_INPUT_0_BUS |= IADC_INPUT_0_BUSALLOC;
  GPIO->IADC_INPUT_1_BUS |= IADC_INPUT_1_BUSALLOC;


  // Start scan conversion
  IADC_command(IADC0, iadcCmdStartScan);

  uint32_t status = IADC0->STATUS;
  app_log_info("IADC On!\n");
  app_log_info("IADC STATUS: 0x%08X\n",(unsigned int)status);
}
*/

//NEW TEST FUNCTION: //this works for the bipolar case!!!!
void initIADC (void)
{
  // Declare init structs
  IADC_Init_t init = IADC_INIT_DEFAULT;
  IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
  IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;
  IADC_ScanTable_t scanTable = IADC_SCANTABLE_DEFAULT;

  // Enable IADC0 and GPIO clock branches

  /* Note: For EFR32xG21 radio devices, library function calls to
   * CMU_ClockEnable() have no effect as oscillators are automatically turned
   * on/off based on demand from the peripherals; CMU_ClockEnable() is a dummy
   * function for EFR32xG21 for library consistency/compatibility.
   */
  CMU_ClockEnable(cmuClock_IADC0, true);
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Reset IADC to reset configuration in case it has been modified by
  // other code
  IADC_reset(IADC0);

  // Select clock for IADC
  CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_FSRCO);  // FSRCO - 20MHz

  // Modify init structs and initialize
  init.warmup = iadcWarmupKeepWarm;

  // Set the HFSCLK prescale value here
  init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

  // Configuration 0 is used by both scan and single conversions by default
  // Use internal bandgap (supply voltage in mV) as reference
  initAllConfigs.configs[0].reference = iadcCfgReferenceVddX0P8Buf; //iadcCfgReferenceVddx; //works vdd
  initAllConfigs.configs[0].vRef = 13;
  initAllConfigs.configs[0].analogGain = iadcCfgAnalogGain4x;
  initAllConfigs.configs[0].twosComplement = iadcCfgTwosCompBipolar;

  // Divides CLK_SRC_ADC to set the CLK_ADC frequency
  initAllConfigs.configs[0].adcClkPrescale = IADC_calcAdcClkPrescale(IADC0,
                                             CLK_ADC_FREQ,
                                             0,
                                             iadcCfgModeHighAccuracy,
                                             init.srcClkPrescale);

  // Assign pins to positive and negative inputs in differential mode

  // Configure scan mode
  initScan.triggerAction = iadcTriggerActionContinuous; // Continuous scan mode
  initScan.dataValidLevel = iadcFifoCfgDvl4;            // FIFO generates an interrupt after 4 entries
  initScan.fifoDmaWakeup = true;                        // Enable DMA wakeup on FIFO

  // Configure scan table for input channels
  scanTable.entries[0].posInput = iadcPosInputPadAna2; // POSITIVE
  scanTable.entries[0].negInput = iadcPosInputPadAna2 | 1; // NEGATIVE
  scanTable.entries[0].includeInScan = true;            // Include this channel in scan

  // Configure scan table for input channels
  //scanTable.entries[1].posInput = iadcPosInputPadAna2 | 1; // POSITIVE ACTUALLY AIN3
  //scanTable.entries[1].negInput = iadcNegInputGnd; // NEGATIVE
  //scanTable.entries[1].includeInScan = true;            // Include this channel in scan

  // Initialize the IADC
  IADC_init(IADC0, &init, &initAllConfigs);

  // Initialize the Single conversion inputs
  IADC_initScan(IADC0, &initScan, &scanTable);




  // Allocate the analog bus for ADC0 inputs
  GPIO->IADC_INPUT_0_BUS |= IADC_INPUT_0_BUSALLOC;
  GPIO->IADC_INPUT_1_BUS |= IADC_INPUT_1_BUSALLOC;


  // Configure GPIO analog bus for ADC inputs

  // Start scan conversion
  IADC_command(IADC0, iadcCmdStartScan);

  uint32_t status = IADC0->STATUS;
  app_log_info("IADC On!\n");
  app_log_info("IADC STATUS: 0x%08X\n",(unsigned int)status);
}
//END OF TEST FUNCTION NO FUNCIONA!


//1395 es el umbral
/**
 *
 * NEW IADC FUNCTION WITH DETECTOR, 1395 es el umbral
 */
static void readIADC_and_notify(void)
{
  // Definir un buffer estático para almacenar 100 muestras
  static uint32_t iadc_buffer[100] = {0};  // Almacena las muestras
  static uint8_t buffer_index = 0;         // Índice del buffer
  static uint32_t sum_iadc = 0;            // Suma acumulada

  // Verificar si hay datos en el FIFO del ADC
  if (!(IADC0->STATUS & IADC_STATUS_SCANFIFODV)) {
    app_log_warning("No hay datos en el FIFO del ADC.\n");
    return;
  }
  // Leer el valor del ADC desde el FIFO
  uint32_t iadcResult = IADC_pullScanFifoResult(IADC0).data;
  app_log_info("FIFO Raw: 0x%08x\n", (unsigned int)iadcResult);

  // === Guardar la muestra en el buffer y calcular el promedio ===
  sum_iadc -= iadc_buffer[buffer_index];  // Restar muestra antigua
  iadc_buffer[buffer_index] = iadcResult; // Agregar nueva muestra
  sum_iadc += iadcResult;                 // Sumar nueva muestra
  buffer_index = (buffer_index + 1) % 100; // Índice circular

  uint32_t iadc_avg = sum_iadc / 100; // Promedio de 100 muestras


  // Configuración del ADC con 20 bits de resolución
  const uint32_t ADC_RESOLUTION = 1000 ;//(1 << 20) - 1;  // 2^20 - 1 = 1048575
  const float VREF_MV = 3300.0;  // 3.3V referencia en mV

  // Convertir el valor del ADC a mV
  float voltage_mV = (iadcResult * VREF_MV) / ADC_RESOLUTION;
  float avg_voltage_mV = (iadc_avg * VREF_MV) / ADC_RESOLUTION; // Promedio en mV
  uint16_t voltage_int_avg = (uint16_t)avg_voltage_mV;  // Convertimos a entero para Bluetooth

  if(voltage_int_avg > (470)){
      GPIO_PinModeSet(gpioPortB, 0, gpioModePushPull, 0);
      GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 1);
  }
  else{
      GPIO_PinModeSet(gpioPortB, 0, gpioModePushPull, 1);
      GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 0);
  }

  // === Aplicar el filtro FIR al voltaje leído ===
  float filtered_voltage = applyFIR_Hamming(voltage_mV);
  uint16_t voltage_int = (uint16_t)filtered_voltage;  // Convertimos a entero para Bluetooth

  app_log_info("IADC Value: %u (%0.2f mV) - Promedio: %u (%0.2f mV)\n",
               (unsigned int)voltage_int, voltage_mV,
               (unsigned int)voltage_int_avg, avg_voltage_mV);

  // Convertir a bytes para enviar por Bluetooth
  uint8_t adcValue[2];
  adcValue[0] = voltage_int & 0xFF;         // Byte bajo
  adcValue[1] = (voltage_int >> 8) & 0xFF;  // Byte alto

  // Guardar el valor en la característica GATT
  sl_status_t sc = sl_bt_gatt_server_write_attribute_value(gattdb_analog_0, 0, sizeof(adcValue), adcValue);
  if (sc == SL_STATUS_OK) {
    // Enviar notificación Bluetooth
    sc = sl_bt_gatt_server_notify_all(gattdb_analog_0, sizeof(adcValue), adcValue);
    if (sc == SL_STATUS_OK) {
      app_log_info("Notificación enviada: %u mV\n", voltage_int);
    } else {
      app_log_error("Error al notificar Bluetooth: %x\n", (unsigned int)sc);
    }
  } else {
    app_log_error("Error al escribir en GATT: %x\n", (unsigned int)sc);
  }
}
//OLD FUNCTION WITHOUT DETECTOR!
/**
static void readIADC_and_notify(void)
{
  // Verificar si hay datos en el FIFO del ADC
  if (!(IADC0->STATUS & IADC_STATUS_SCANFIFODV)) {
    app_log_warning("No hay datos en el FIFO del ADC.\n");
    return;
  }
  // Leer el valor del ADC desde el FIFO
  uint32_t iadcResult = IADC_pullScanFifoResult(IADC0).data;
  app_log_info("FIFO Raw: 0x%08x\n", (unsigned int)iadcResult);

  // Configuración del ADC con 20 bits de resolución
  const uint32_t ADC_RESOLUTION = (1 << 20) - 1;  // 2^20 - 1 = 1048575
  const float VREF_MV = 3300.0;  // 3.3V referencia en mV

  // Convertir el valor del ADC a mV
  float voltage_mV = (iadcResult * VREF_MV) / ADC_RESOLUTION;

  //new
  // === Aplicar el filtro FIR al voltaje leído ===
  float filtered_voltage = applyFIR(voltage_mV);
  uint16_t voltage_int = (uint16_t)filtered_voltage;  // Convertimos a entero para Bluetooth

  //uint16_t voltage_int = (uint16_t)voltage_mV;  // Convertimos a entero para Bluetooth

  app_log_info("IADC Value: %u (%0.2f mV)\n", (unsigned int)iadcResult, voltage_mV);

  // Convertir a bytes para enviar por Bluetooth
  uint8_t adcValue[2];
  adcValue[0] = voltage_int & 0xFF;         // Byte bajo
  adcValue[1] = (voltage_int >> 8) & 0xFF;  // Byte alto

  // Guardar el valor en la característica GATT
  sl_status_t sc = sl_bt_gatt_server_write_attribute_value(gattdb_analog_0, 0, sizeof(adcValue), adcValue);
  if (sc == SL_STATUS_OK) {
    // Enviar notificación Bluetooth
    sc = sl_bt_gatt_server_notify_all(gattdb_analog_0, sizeof(adcValue), adcValue);
    if (sc == SL_STATUS_OK) {
      app_log_info("Notificación enviada: %u mV\n", voltage_int);
    } else {
      app_log_error("Error al notificar Bluetooth: %x\n", (unsigned int)sc);
    }
  } else {
    app_log_error("Error al escribir en GATT: %x\n", (unsigned int)sc);
  }
}
*/

/**
 * BLUETOOTH EVENT HANDLING! from example on
 */
// ======== [EVENTOS BLE] ========
// The advertising set handle allocated from Bluetooth stack.
//static uint8_t advertising_set_handle = 0xff;
//NEW 10:33
bool report_connection = false; //To know if conected or not.
void sl_bt_on_event(sl_bt_msg_t *evt) {
    sl_status_t sc;

    switch (SL_BT_MSG_ID(evt->header)) {
        // Evento de arranque del sistema BLE
        case sl_bt_evt_system_boot_id:
            app_log_info("Sistema arrancado. Iniciando advertising...\n");
            //sl_sleeptimer_delay_millisecond(3000);
            // Inicializar IADC al arrancar
            //initIADC();

            // Crear y configurar advertising
            sc = sl_bt_advertiser_create_set(&advertising_set_handle);
            app_assert_status(sc);

            sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                       sl_bt_advertiser_general_discoverable);
            app_assert_status(sc);

            sc = sl_bt_advertiser_set_timing(advertising_set_handle,
                                             (ADV_INTERVAL_MS * 16) / 10,
                                             (ADV_INTERVAL_MS * 16) / 10,
                                             0,
                                             0);
            app_assert_status(sc);

            // Iniciar advertising
            sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                               sl_bt_legacy_advertiser_connectable);
            app_log_info("Advertising iniciado");
            app_assert_status(sc);
            break;

        // Evento de desconexión
        case sl_bt_evt_connection_closed_id:
            app_log_info("Conexión cerrada. Reiniciando advertising...\n");
            report_connection = false; //NEW
            // Generar nuevos datos de advertising antes de reiniciar
            sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                       sl_bt_advertiser_general_discoverable);
            app_assert_status(sc);

            // Reiniciar advertising
            sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                               sl_bt_legacy_advertiser_connectable);
            app_assert_status(sc);
            break;
        // NEW 10:26 12.03.2025
        case sl_bt_evt_connection_opened_id:
            app_log_info("Conexión abierta... \n");
            report_connection = true;
            break;

        // Otros eventos BLE
        default:
            break;
    }
}

/**
 * This function sets the gpio to High or Low (the red one)
 */
// Función para configurar el GPIO y encender el LED
void gpio_init(void) {
  // Configurar el pin como salida push-pull
  GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 0);
  GPIO_PinModeSet(gpioPortD, 2, gpioModePushPull, 0);
  GPIO_PinModeSet(gpioPortB, 0, gpioModePushPull, 0);

  app_log_info("All leds are turned on!\n");
  sl_sleeptimer_delay_millisecond(1000);
  //sl_led_turn_on(SL_SIMPLE_LED_INSTANCE(0)); //THIS WORKS TOO!
  GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 1);
  GPIO_PinModeSet(gpioPortD, 2, gpioModePushPull, 1);
  GPIO_PinModeSet(gpioPortB, 0, gpioModePushPull, 1);

  app_log_info("All leds are turned off!\n");

}


/**
 * PARA PARPADEAR EL LED
 */
static sl_sleeptimer_timer_handle_t led_timer;
static bool led_state = false; // Estado del LED
// === [Callback del Temporizador] ===
void led_blink_callback(sl_sleeptimer_timer_handle_t *handle, void *data) {
    (void)handle; (void)data; // Evita advertencias de variables no usadas

    led_state = !led_state; // Alternar LED
    GPIO_PinOutToggle(gpioPortA, 4); // Cambiar estado del LED
}

// === [Función para Iniciar el Parpadeo] ===
void start_led_blink(void) {
    sl_sleeptimer_start_periodic_timer_ms(&led_timer, 1000, led_blink_callback, NULL, 0, 0);
}

// === [Función para Detener el Parpadeo] ===
void stop_led_blink(void) {
    sl_sleeptimer_stop_timer(&led_timer);
    GPIO_PinOutClear(gpioPortA, 4); // Apagar el LED al detener
}


/**
 * Función de inicialización
 */
void app_init(void) {
  // Encender el LED al inicio
  sl_sensor_imu_init();
  sl_imu_init();
  sl_imu_configure(IMU_UPDATE_INTERVAL_MS);
  sl_imu_calibrate_gyro();
  app_log_info("Sistema IMU inicializado\n");
  gpio_init();
  app_log_info("System up bro!\n");
  initIADC();
  //FOR IMU************
  //************
}

/***************************************************************************//**
 * Función de acción (Loop principal)
 ******************************************************************************/
void app_process_action(void) {
  static uint32_t last_update = 0;
  if(report_connection == true){ //new!
      stop_led_blink();
      GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 0);
      if ((sl_sleeptimer_get_tick_count() - last_update) >= sl_sleeptimer_ms_to_tick(IMU_UPDATE_INTERVAL_MS)) {
            last_update = sl_sleeptimer_get_tick_count();
            //app_log_info("corriendo readIADC_and_notify");
            //initIADC(); //again to reset iadc
            readIADC_and_notify();
            handle_imu_data();
            send_rssi(); //new 13.03.2025
      }
  }
  else if (report_connection == false){ //VER LO DEL ENERGY MANAGEMENT
      GPIO_PinModeSet(gpioPortA, 4, gpioModePushPull, 1);
      GPIO_PinModeSet(gpioPortB, 0, gpioModePushPull, 1);
      start_led_blink();
  }
}

