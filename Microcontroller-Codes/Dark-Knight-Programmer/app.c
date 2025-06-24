/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "app_log.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "sl_sleeptimer.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "test_alpha.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

static bool report_button_flag = false;
static uint32_t last_button_press = 0;
static uint8_t button_press_count = 0;

// Status indicators
typedef enum {
  PROG_STATUS_IDLE = 0,
  PROG_STATUS_INITIALIZING,
  PROG_STATUS_PROGRAMMING,
  PROG_STATUS_VERIFYING,
  PROG_STATUS_SUCCESS,
  PROG_STATUS_ERROR
} programming_status_t;

static programming_status_t current_status = PROG_STATUS_IDLE;
static uint32_t last_status_blink = 0;
static bool led_blink_state = false;

// SWD Programming definitions - Using mikroBUS pins
#define SWD_SWCLK_PORT    gpioPortC
#define SWD_SWCLK_PIN     1          // mikroBUS SCK (PC01)
#define SWD_SWDIO_PORT    gpioPortC
#define SWD_SWDIO_PIN     2          // mikroBUS MISO (PC02)
#define SWD_RESET_PORT    gpioPortC
#define SWD_RESET_PIN     8          // mikroBUS RST (PC08)

// Forward declarations
static void swd_test_reset_pin(void);
static void update_status_led(void);
static void set_programming_status(programming_status_t status);

// SWD Protocol constants
#define SWD_IDCODE_CORTEX_M3    0x2BA01477
#define SWD_IDCODE_CORTEX_M0P   0x0BC11477
#define SWD_JTAG_TO_SWD_SEQ     0xE79E
#define SWD_LINE_RESET_CYCLES   50
#define SWD_IDLE_CYCLES         8

// SWD Register addresses
#define SWD_DP_IDCODE     0x00
#define SWD_DP_CTRL_STAT  0x04
#define SWD_DP_SELECT     0x08
#define SWD_DP_RDBUFF     0x0C
#define SWD_DP_ABORT      0x00  // Write only
#define SWD_AP_CSW        0x00
#define SWD_AP_TAR        0x04
#define SWD_AP_DRW        0x0C
#define SWD_AP_IDR        0xFC

// SWD Response codes
#define SWD_RESPONSE_OK    0x1
#define SWD_RESPONSE_WAIT  0x2
#define SWD_RESPONSE_FAULT 0x4

// Debug and Control Registers
#define DHCSR_ADDRESS     0xE000EDF0  // Debug Halting Control and Status Register
#define AIRCR_ADDRESS     0xE000ED0C  // Application Interrupt and Reset Control Register
#define DEMCR_ADDRESS     0xE000EDFC  // Debug Exception and Monitor Control Register

// MSC (Memory System Controller) definitions
#define MSC_BASE_SERIES0  0x400C0000  // Series 0 devices
#define MSC_BASE_SERIES1  0x400E0000  // Series 1 devices
#define MSC_WRITECTRL     0x008  // Offset from MSC base
#define MSC_WRITECMD      0x00C  // Offset from MSC base
#define MSC_ADDRB         0x010  // Offset from MSC base
#define MSC_WDATA         0x018  // Offset from MSC base
#define MSC_STATUS        0x01C  // Offset from MSC base

// MSC_WRITECTRL bits (use SDK definitions)
#define MSC_WRITECTRL_WDOUBLE   (1 << 2)   // Double word write (custom)

// MSC_WRITECMD bits (use SDK definitions) 
#define MSC_WRITECMD_LADDRIM    (1 << 0)   // Load Address (custom)
#define MSC_WRITECMD_WRITEONCE  (1 << 3)   // Write Once (custom)
#define MSC_WRITECMD_WRITETRIG  (1 << 4)   // Write Trigger (custom)
#define MSC_WRITECMD_ERASEMAIN1 (1 << 9)   // Erase Main Block 1 (custom)

// AHB-AP CSW settings
#define AHB_AP_CSW_SIZE_32BIT    (2 << 0)  // 32-bit transfers
#define AHB_AP_CSW_ADDRINC_SINGLE (1 << 4)  // Auto-increment
#define AHB_AP_CSW_HPROT         (1 << 25) // Bus protection
#define CSW_DEFAULT_VALUE        0x23000002

// Device identification
#define AHB_AP_IDR_CORTEX_M3     0x24770011
#define AHB_AP_IDR_CORTEX_M0P    0x04770031
#define AHB_AP_IDR_DEBUG_LOCKED  0x16E60001

// Flash parameters
#define FLASH_PAGE_SIZE_512      512
#define FLASH_PAGE_SIZE_1K       1024
#define FLASH_PAGE_SIZE_2K       2048
#define FLASH_PAGE_SIZE_4K       4096
#define FLASH_WRITE_DELAY_US     20
#define FLASH_ERASE_DELAY_MS     25
#define TARGET_FLASH_BASE        0x00000000

// Programming state
static bool swd_programming_active = false;
static uint32_t swd_programming_progress = 0;
static uint32_t swd_total_bytes = 0;
static uint32_t msc_base_address = MSC_BASE_SERIES0;
static uint32_t flash_page_size = FLASH_PAGE_SIZE_2K;
static bool device_is_series1 = false;

// Updates the Report Button characteristic.
static sl_status_t update_report_button_characteristic(void);
// Sends notification of the Report Button characteristic.
static sl_status_t send_report_button_notification(void);

// SWD Programming functions
static void swd_init_gpio(void);
static void swd_set_swclk(bool state);
static void swd_set_swdio(bool state);
static bool swd_get_swdio(void);
static void swd_set_swdio_input(void);
static void swd_set_swdio_output(void);
static void swd_delay_us(uint32_t us);
static void swd_clock_cycle(void);
static void swd_line_reset(void);
static void swd_jtag_to_swd_sequence(void);
static uint8_t swd_read_bit(void);
static void swd_write_bit(uint8_t bit);
static uint32_t swd_read_word(void);
static void swd_write_word(uint32_t data);
static uint8_t swd_transaction(bool read, uint8_t addr, uint32_t *data);
static bool swd_init_sequence(void);
static bool swd_read_idcode(uint32_t *idcode);
static bool swd_program_target(void);
static void swd_send_progress_notification(const char* message);
static bool swd_power_up_debug(void);
static bool swd_halt_cpu(void);
static bool swd_configure_ahb_ap(void);
static uint32_t swd_read_memory(uint32_t address);
static bool swd_write_memory(uint32_t address, uint32_t data);
static bool swd_erase_flash(uint32_t start_addr, uint32_t size);
static bool swd_write_flash(uint32_t start_addr, const uint8_t *data, uint32_t size);
static bool swd_verify_flash(uint32_t start_addr, const uint8_t *data, uint32_t size);
static bool swd_mass_erase(void);
static bool swd_page_erase(uint32_t page_addr);
static void swd_determine_device_type(uint32_t idr_value);

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  // Make sure there will be no button events before the boot event.
  sl_button_disable(SL_SIMPLE_BUTTON_INSTANCE(0));

  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  // Update status LED continuously
  update_status_led();
  
  // Check if there was a report button interaction.
  if (report_button_flag) {
    sl_status_t sc;

    report_button_flag = false; // Reset flag

    if (swd_programming_active) {
      // Execute SWD programming
      app_log_info("Starting SWD target programming...");
      swd_send_progress_notification("Initializing SWD interface...");
      
      bool success = swd_program_target();
      
      if (success) {
        app_log_info("SWD programming completed successfully!");
        swd_send_progress_notification("Programming completed successfully!");
      } else {
        app_log_error("SWD programming failed!");
        swd_send_progress_notification("Programming failed!");
      }
      
      swd_programming_active = false;
    } else {
      // Normal button notification
      sc = update_report_button_characteristic();
      app_log_status_error(sc);

      if (sc == SL_STATUS_OK) {
        sc = send_report_button_notification();
        app_log_status_error(sc);
      }
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);

      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);

      // Button events can be received from now on.
      sl_button_enable(SL_SIMPLE_BUTTON_INSTANCE(0));

      // Initialize SWD GPIO pins
      swd_init_gpio();
      set_programming_status(PROG_STATUS_IDLE);
      app_log_info("SWD GPIO initialized");
      
      // Initialize programming parameters
      swd_total_bytes = sizeof(test_alpha_bin);
      app_log_info("Target firmware size: %lu bytes", swd_total_bytes);

      // Check the report button state, then update the characteristic and
      // send notification.
      sc = update_report_button_characteristic();
      app_log_status_error(sc);

      if (sc == SL_STATUS_OK) {
        sc = send_report_button_notification();
        app_log_status_error(sc);
      }
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("Connection opened.\n");
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("Connection closed.\n");

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that the value of an attribute in the local GATT
    // database was changed by a remote GATT client.
    case sl_bt_evt_gatt_server_attribute_value_id:
      // The value of the gattdb_led_control characteristic was changed.
      if (gattdb_led_control == evt->data.evt_gatt_server_attribute_value.attribute) {
        uint8_t data_recv;
        size_t data_recv_len;

        // Read characteristic value.
        sc = sl_bt_gatt_server_read_attribute_value(gattdb_led_control,
                                                    0,
                                                    sizeof(data_recv),
                                                    &data_recv_len,
                                                    &data_recv);
        (void)data_recv_len;
        app_log_status_error(sc);

        if (sc != SL_STATUS_OK) {
          break;
        }

        // Toggle LED.
        if (data_recv == 0x00) {
          sl_led_turn_off(SL_SIMPLE_LED_INSTANCE(0));
          app_log_info("LED off.\n");
        } else if (data_recv == 0x01) {
          sl_led_turn_on(SL_SIMPLE_LED_INSTANCE(0));
          app_log_info("LED on.\n");
        } else {
          app_log_error("Invalid attribute value: 0x%02x\n", (int)data_recv);
        }
      }
      break;

    // -------------------------------
    // This event occurs when the remote device enabled or disabled the
    // notification.
    case sl_bt_evt_gatt_server_characteristic_status_id:
      if (gattdb_report_button == evt->data.evt_gatt_server_characteristic_status.characteristic) {
        // A local Client Characteristic Configuration descriptor was changed in
        // the gattdb_report_button characteristic.
        if (evt->data.evt_gatt_server_characteristic_status.client_config_flags
            & sl_bt_gatt_notification) {
          // The client just enabled the notification. Send notification of the
          // current button state stored in the local GATT table.
          app_log_info("Notification enabled.");

          sc = send_report_button_notification();
          app_log_status_error(sc);
        } else {
          app_log_info("Notification disabled.\n");
        }
      }
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/***************************************************************************//**
 * Simple Button
 * Button state changed callback
 * @param[in] handle Button event handle
 ******************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if (SL_SIMPLE_BUTTON_INSTANCE(0) == handle) {
    // Only send message when button is pressed
    if (sl_button_get_state(SL_SIMPLE_BUTTON_INSTANCE(0)) == SL_SIMPLE_BUTTON_PRESSED) {
      uint32_t current_time = sl_sleeptimer_get_tick_count();
      
      // Check for double-click (within 500ms)
      if ((current_time - last_button_press) < sl_sleeptimer_ms_to_tick(500)) {
        button_press_count++;
      } else {
        button_press_count = 1;
      }
      
      last_button_press = current_time;
      
      if (button_press_count >= 2) {
        // Double-click detected - test reset pin
        button_press_count = 0;
        app_log_info("Double-click detected - testing reset pin");
        swd_test_reset_pin();
      } else if (!swd_programming_active) {
        // Single click - start SWD programming (after delay to check for double-click)
        app_log_info("Button pressed - will start programming if no double-click");
        report_button_flag = true;
      } else {
        app_log_info("Button pressed - programming already in progress");
      }
    }
  }
}

/***************************************************************************//**
 * Updates the Report Button characteristic.
 *
 * Checks the current button state and then writes it into the local GATT table.
 ******************************************************************************/
static sl_status_t update_report_button_characteristic(void)
{
  sl_status_t sc;
  uint8_t data_send;

  switch (sl_button_get_state(SL_SIMPLE_BUTTON_INSTANCE(0))) {
    case SL_SIMPLE_BUTTON_PRESSED:
      data_send = (uint8_t)SL_SIMPLE_BUTTON_PRESSED;
      app_log_info("Button PRESSED - sending binary: 0x%02x", (int)data_send);
      break;

    case SL_SIMPLE_BUTTON_RELEASED:
      data_send = (uint8_t)SL_SIMPLE_BUTTON_RELEASED;
      app_log_info("Button RELEASED - sending binary: 0x%02x", (int)data_send);
      break;

    default:
      // Invalid button state
      return SL_STATUS_FAIL; // Invalid button state
  }

  // Write attribute in the local GATT database.
  sc = sl_bt_gatt_server_write_attribute_value(gattdb_report_button,
                                               0,
                                               sizeof(data_send),
                                               &data_send);
  if (sc == SL_STATUS_OK) {
    app_log_info("Attribute written: 0x%02x", (int)data_send);
  } else {
    app_log_error("FAILED to write attribute: 0x%04x", (int)sc);
  }

  return sc;
}

/***************************************************************************//**
 * Sends notification of the Report Button characteristic.
 *
 * Reads the current button state from the local GATT database and sends it as a
 * notification.
 ******************************************************************************/
static sl_status_t send_report_button_notification(void)
{
  sl_status_t sc;
  
  // Check if button is currently pressed
  if (sl_button_get_state(SL_SIMPLE_BUTTON_INSTANCE(0)) == SL_SIMPLE_BUTTON_PRESSED) {
    // Send "Boton Apretado" message directly
    const char* message = "Boton Apretado";
    size_t message_len = strlen(message);
    
    app_log_info("Sending notification: %s (len=%d)", message, (int)message_len);
    
    // Send characteristic notification directly with string
    sc = sl_bt_gatt_server_notify_all(gattdb_report_button,
                                      message_len,
                                      (uint8_t*)message);
    if (sc == SL_STATUS_OK) {
      app_log_info("SUCCESS: String notification sent!");
    } else {
      app_log_error("FAILED to send string notification: 0x%04x", (int)sc);
    }
  } else {
    // Use original approach for released button
    uint8_t data_send;
    size_t data_len;

    // Read report button characteristic stored in local GATT database.
    sc = sl_bt_gatt_server_read_attribute_value(gattdb_report_button,
                                                0,
                                                sizeof(data_send),
                                                &data_len,
                                                &data_send);
    if (sc != SL_STATUS_OK) {
      app_log_error("FAILED to read from GATT: 0x%04x", (int)sc);
      return sc;
    }

    // Send characteristic notification.
    sc = sl_bt_gatt_server_notify_all(gattdb_report_button,
                                      sizeof(data_send),
                                      &data_send);
    if (sc == SL_STATUS_OK) {
      app_log_info("Binary notification sent: 0x%02x", (int)data_send);
    } else {
      app_log_error("FAILED to send binary notification: 0x%04x", (int)sc);
    }
  }
  
  return sc;
}

/***************************************************************************//**
 * Initialize SWD GPIO pins
 ******************************************************************************/
static void swd_init_gpio(void)
{
  // Enable GPIO clock
  CMU_ClockEnable(cmuClock_GPIO, true);
  
  // Configure SWCLK as output (always driven by host)
  GPIO_PinModeSet(SWD_SWCLK_PORT, SWD_SWCLK_PIN, gpioModePushPull, 0);
  
  // Configure SWDIO as output initially
  GPIO_PinModeSet(SWD_SWDIO_PORT, SWD_SWDIO_PIN, gpioModePushPull, 1);
  
  // Configure RESET as output
  GPIO_PinModeSet(SWD_RESET_PORT, SWD_RESET_PIN, gpioModePushPull, 1);
  
  app_log_info("SWD GPIO configured: SWCLK=PA%d, SWDIO=PA%d, RESET=PA%d", 
               SWD_SWCLK_PIN, SWD_SWDIO_PIN, SWD_RESET_PIN);
}

/***************************************************************************//**
 * Set SWCLK pin state
 ******************************************************************************/
static void swd_set_swclk(bool state)
{
  if (state) {
    GPIO_PinOutSet(SWD_SWCLK_PORT, SWD_SWCLK_PIN);
  } else {
    GPIO_PinOutClear(SWD_SWCLK_PORT, SWD_SWCLK_PIN);
  }
}

/***************************************************************************//**
 * Set SWDIO pin state (when configured as output)
 ******************************************************************************/
static void swd_set_swdio(bool state)
{
  if (state) {
    GPIO_PinOutSet(SWD_SWDIO_PORT, SWD_SWDIO_PIN);
  } else {
    GPIO_PinOutClear(SWD_SWDIO_PORT, SWD_SWDIO_PIN);
  }
}

/***************************************************************************//**
 * Read SWDIO pin state (when configured as input)
 ******************************************************************************/
static bool swd_get_swdio(void)
{
  return GPIO_PinInGet(SWD_SWDIO_PORT, SWD_SWDIO_PIN);
}

/***************************************************************************//**
 * Configure SWDIO as input
 ******************************************************************************/
static void swd_set_swdio_input(void)
{
  GPIO_PinModeSet(SWD_SWDIO_PORT, SWD_SWDIO_PIN, gpioModeInputPull, 1);
}

/***************************************************************************//**
 * Configure SWDIO as output
 ******************************************************************************/
static void swd_set_swdio_output(void)
{
  GPIO_PinModeSet(SWD_SWDIO_PORT, SWD_SWDIO_PIN, gpioModePushPull, 1);
}

/***************************************************************************//**
 * Assert hardware reset (pull low)
 ******************************************************************************/
static void swd_assert_reset(void)
{
  GPIO_PinOutClear(SWD_RESET_PORT, SWD_RESET_PIN);
  app_log_info("SWD: Target reset asserted");
}

/***************************************************************************//**
 * Release hardware reset (pull high) 
 ******************************************************************************/
static void swd_release_reset(void)
{
  GPIO_PinOutSet(SWD_RESET_PORT, SWD_RESET_PIN);
  app_log_info("SWD: Target reset released");
}

/***************************************************************************//**
 * Perform complete target reset sequence
 ******************************************************************************/
static bool swd_target_reset_sequence(void)
{
  app_log_info("SWD: Performing target reset sequence");
  
  // Assert reset
  swd_assert_reset();
  swd_delay_us(1000);  // Hold reset for 1ms
  
  // Release reset
  swd_release_reset();
  swd_delay_us(100);   // Wait for target startup
  
  return true;
}

/***************************************************************************//**
 * Test reset pin functionality (visible on oscilloscope)
 ******************************************************************************/
static void swd_test_reset_pin(void)
{
  app_log_info("SWD: Testing reset pin - watch on oscilloscope!");
  swd_send_progress_notification("Testing reset pin...");
  
  // Make reset pin activity visible on scope
  for (int i = 0; i < 5; i++) {
    app_log_info("SWD: Reset pulse %d/5", i+1);
    
    // Assert reset (should see LOW on scope)
    swd_assert_reset();
    swd_delay_us(10000);  // 10ms low
    
    // Release reset (should see HIGH on scope) 
    swd_release_reset();
    swd_delay_us(10000);  // 10ms high
  }
  
  app_log_info("SWD: Reset pin test completed");
  swd_send_progress_notification("Reset test completed!");
}

/***************************************************************************//**
 * Set programming status and update indicators
 ******************************************************************************/
static void set_programming_status(programming_status_t status)
{
  current_status = status;
  
  const char* status_names[] = {
    "IDLE", "INITIALIZING", "PROGRAMMING", "VERIFYING", "SUCCESS", "ERROR"
  };
  
  app_log_info("SWD: Status changed to %s", status_names[status]);
  
  // Send status via Bluetooth
  char status_msg[64];
  switch (status) {
    case PROG_STATUS_IDLE:
      snprintf(status_msg, sizeof(status_msg), "Ready - Press button to program");
      break;
    case PROG_STATUS_INITIALIZING:
      snprintf(status_msg, sizeof(status_msg), "Initializing SWD interface...");
      break;
    case PROG_STATUS_PROGRAMMING:
      snprintf(status_msg, sizeof(status_msg), "Programming flash memory...");
      break;
    case PROG_STATUS_VERIFYING:
      snprintf(status_msg, sizeof(status_msg), "Verifying flash contents...");
      break;
    case PROG_STATUS_SUCCESS:
      snprintf(status_msg, sizeof(status_msg), "Programming completed successfully!");
      break;
    case PROG_STATUS_ERROR:
      snprintf(status_msg, sizeof(status_msg), "Programming failed - check connections");
      break;
  }
  
  swd_send_progress_notification(status_msg);
  update_status_led();
}

/***************************************************************************//**
 * Update status LED based on current programming status
 ******************************************************************************/
static void update_status_led(void)
{
  uint32_t current_time = sl_sleeptimer_get_tick_count();
  
  switch (current_status) {
    case PROG_STATUS_IDLE:
      // LED off when idle
      sl_led_turn_off(&sl_led_led0);
      break;
      
    case PROG_STATUS_INITIALIZING:
      // Slow blink during initialization (500ms)
      if ((current_time - last_status_blink) > sl_sleeptimer_ms_to_tick(500)) {
        led_blink_state = !led_blink_state;
        last_status_blink = current_time;
        if (led_blink_state) {
          sl_led_turn_on(&sl_led_led0);
        } else {
          sl_led_turn_off(&sl_led_led0);
        }
      }
      break;
      
    case PROG_STATUS_PROGRAMMING:
      // Fast blink during programming (200ms)
      if ((current_time - last_status_blink) > sl_sleeptimer_ms_to_tick(200)) {
        led_blink_state = !led_blink_state;
        last_status_blink = current_time;
        if (led_blink_state) {
          sl_led_turn_on(&sl_led_led0);
        } else {
          sl_led_turn_off(&sl_led_led0);
        }
      }
      break;
      
    case PROG_STATUS_VERIFYING:
      // Medium blink during verification (300ms)
      if ((current_time - last_status_blink) > sl_sleeptimer_ms_to_tick(300)) {
        led_blink_state = !led_blink_state;
        last_status_blink = current_time;
        if (led_blink_state) {
          sl_led_turn_on(&sl_led_led0);
        } else {
          sl_led_turn_off(&sl_led_led0);
        }
      }
      break;
      
    case PROG_STATUS_SUCCESS:
      // Solid ON for success
      sl_led_turn_on(&sl_led_led0);
      break;
      
    case PROG_STATUS_ERROR:
      // Very fast blink for error (100ms)
      if ((current_time - last_status_blink) > sl_sleeptimer_ms_to_tick(100)) {
        led_blink_state = !led_blink_state;
        last_status_blink = current_time;
        if (led_blink_state) {
          sl_led_turn_on(&sl_led_led0);
        } else {
          sl_led_turn_off(&sl_led_led0);
        }
      }
      break;
  }
}

/***************************************************************************//**
 * Microsecond delay for SWD timing
 ******************************************************************************/
static void swd_delay_us(uint32_t us)
{
  // Simple delay - could be optimized based on clock frequency
  for (uint32_t i = 0; i < us * 10; i++) {
    __NOP();
  }
}

/***************************************************************************//**
 * Generate one clock cycle
 ******************************************************************************/
static void swd_clock_cycle(void)
{
  swd_set_swclk(false);
  swd_delay_us(1);
  swd_set_swclk(true);
  swd_delay_us(1);
}

/***************************************************************************//**
 * Perform SWD line reset (50+ clock cycles with SWDIO high)
 ******************************************************************************/
static void swd_line_reset(void)
{
  app_log_info("SWD: Performing line reset");
  swd_set_swdio_output();
  swd_set_swdio(true);
  
  for (int i = 0; i < SWD_LINE_RESET_CYCLES; i++) {
    swd_clock_cycle();
  }
  
  swd_set_swdio(false);
  for (int i = 0; i < SWD_IDLE_CYCLES; i++) {
    swd_clock_cycle();
  }
}

/***************************************************************************//**
 * Send JTAG-to-SWD switching sequence
 ******************************************************************************/
static void swd_jtag_to_swd_sequence(void)
{
  app_log_info("SWD: Sending JTAG-to-SWD sequence (0x%04X)", SWD_JTAG_TO_SWD_SEQ);
  swd_set_swdio_output();
  
  uint16_t seq = SWD_JTAG_TO_SWD_SEQ;
  for (int i = 0; i < 16; i++) {
    swd_set_swdio(seq & 1);
    swd_clock_cycle();
    seq >>= 1;
  }
}

/***************************************************************************//**
 * Read one bit from SWD interface
 ******************************************************************************/
static uint8_t swd_read_bit(void)
{
  swd_set_swdio_input();
  swd_set_swclk(false);
  swd_delay_us(1);
  swd_set_swclk(true);
  uint8_t bit = swd_get_swdio() ? 1 : 0;
  swd_delay_us(1);
  return bit;
}

/***************************************************************************//**
 * Write one bit to SWD interface
 ******************************************************************************/
static void swd_write_bit(uint8_t bit)
{
  swd_set_swdio_output();
  swd_set_swdio(bit & 1);
  swd_clock_cycle();
}

/***************************************************************************//**
 * Read 32-bit word from SWD interface
 ******************************************************************************/
static uint32_t swd_read_word(void)
{
  uint32_t data = 0;
  swd_set_swdio_input();
  
  for (int i = 0; i < 32; i++) {
    swd_set_swclk(false);
    swd_delay_us(1);
    swd_set_swclk(true);
    if (swd_get_swdio()) {
      data |= (1UL << i);
    }
    swd_delay_us(1);
  }
  
  return data;
}

/***************************************************************************//**
 * Write 32-bit word to SWD interface
 ******************************************************************************/
static void swd_write_word(uint32_t data)
{
  swd_set_swdio_output();
  
  for (int i = 0; i < 32; i++) {
    swd_set_swdio(data & 1);
    swd_clock_cycle();
    data >>= 1;
  }
}

/***************************************************************************//**
 * Perform SWD transaction
 * @param read: true for read, false for write
 * @param addr: register address (2 bits)
 * @param data: pointer to data (input for write, output for read)
 * @return: SWD response code
 ******************************************************************************/
static uint8_t swd_transaction(bool read, uint8_t addr, uint32_t *data)
{
  // Calculate parity
  uint8_t parity = (read ? 1 : 0) ^ ((addr >> 1) & 1) ^ (addr & 1);
  
  // Send start bit
  swd_write_bit(1);
  
  // Send read/write bit
  swd_write_bit(read ? 1 : 0);
  
  // Send address bits (A[3:2])
  swd_write_bit((addr >> 1) & 1);
  swd_write_bit(addr & 1);
  
  // Send parity bit
  swd_write_bit(parity);
  
  // Send stop bit
  swd_write_bit(0);
  
  // Park bit
  swd_write_bit(1);
  
  // Turnaround - one cycle
  swd_clock_cycle();
  
  // Read ACK (3 bits)
  uint8_t ack = 0;
  ack |= swd_read_bit();
  ack |= (swd_read_bit() << 1);
  ack |= (swd_read_bit() << 2);
  
  if (ack == SWD_RESPONSE_OK) {
    if (read) {
      // Read data
      *data = swd_read_word();
      
      // Read parity bit
      uint8_t parity_bit = swd_read_bit();
      
      // TODO: Verify parity
      (void)parity_bit;
    } else {
      // Turnaround
      swd_clock_cycle();
      
      // Write data
      swd_write_word(*data);
      
      // Write parity bit
      // TODO: Calculate and send parity
      swd_write_bit(0);
    }
  }
  
  // Turnaround
  swd_clock_cycle();
  
  // Idle cycles
  swd_set_swdio_output();
  swd_set_swdio(false);
  for (int i = 0; i < SWD_IDLE_CYCLES; i++) {
    swd_clock_cycle();
  }
  
  return ack;
}

/***************************************************************************//**
 * Initialize SWD communication sequence
 ******************************************************************************/
static bool swd_init_sequence(void)
{
  app_log_info("SWD: Starting initialization sequence");
  set_programming_status(PROG_STATUS_INITIALIZING);
  
  // Step 0: Hardware reset sequence (new!)
  swd_target_reset_sequence();
  
  // Step 1: Line reset
  swd_line_reset();
  
  // Step 2: JTAG-to-SWD sequence
  swd_jtag_to_swd_sequence();
  
  // Step 3: Another line reset
  swd_line_reset();
  
  // Step 4: Read IDCODE
  uint32_t idcode;
  if (!swd_read_idcode(&idcode)) {
    app_log_error("SWD: Failed to read IDCODE");
    return false;
  }
  
  app_log_info("SWD: Initialization sequence completed successfully");
  return true;
}

/***************************************************************************//**
 * Read IDCODE register
 ******************************************************************************/
static bool swd_read_idcode(uint32_t *idcode)
{
  app_log_info("SWD: Reading IDCODE register");
  swd_send_progress_notification("Reading target IDCODE...");
  
  uint8_t response = swd_transaction(true, SWD_DP_IDCODE, idcode);
  
  if (response == SWD_RESPONSE_OK) {
    app_log_info("SWD: IDCODE = 0x%08lX", *idcode);
    
    // Verify known IDCODE values
    if (*idcode == SWD_IDCODE_CORTEX_M3) {
      app_log_info("SWD: Detected Cortex-M3/M4 device");
      swd_send_progress_notification("Cortex-M3/M4 detected");
      return true;
    } else if (*idcode == SWD_IDCODE_CORTEX_M0P) {
      app_log_info("SWD: Detected Cortex-M0+ device");
      swd_send_progress_notification("Cortex-M0+ detected");
      return true;
    } else {
      app_log_warning("SWD: Unknown IDCODE 0x%08lX, continuing anyway", *idcode);
      swd_send_progress_notification("Unknown device detected");
      return true;
    }
  } else {
    app_log_error("SWD: IDCODE read failed, response = 0x%02X", response);
    return false;
  }
}

/***************************************************************************//**
 * Main SWD programming function
 ******************************************************************************/
static bool swd_program_target(void)
{
  app_log_info("SWD: Starting complete target programming");
  swd_send_progress_notification("Starting target programming...");
  
  // Step 1: Initialize SWD interface
  swd_send_progress_notification("Initializing SWD interface...");
  if (!swd_init_sequence()) {
    app_log_error("SWD: Initialization failed");
    set_programming_status(PROG_STATUS_ERROR);
    return false;
  }
  
  // Step 2: Power up debug interface
  swd_send_progress_notification("Powering up debug interface...");
  if (!swd_power_up_debug()) {
    app_log_error("SWD: Debug power up failed");
    set_programming_status(PROG_STATUS_ERROR);
    return false;
  }
  
  // Step 3: Configure AHB-AP
  swd_send_progress_notification("Configuring AHB-AP...");
  if (!swd_configure_ahb_ap()) {
    app_log_error("SWD: AHB-AP configuration failed");
    set_programming_status(PROG_STATUS_ERROR);
    return false;
  }
  
  // Step 4: Halt CPU
  swd_send_progress_notification("Halting target CPU...");
  if (!swd_halt_cpu()) {
    app_log_error("SWD: CPU halt failed");
    set_programming_status(PROG_STATUS_ERROR);
    return false;
  }
  
  app_log_info("SWD: Target firmware size: %lu bytes", swd_total_bytes);
  
  // Step 5: Erase flash
  swd_send_progress_notification("Erasing target flash...");
  if (!swd_erase_flash(TARGET_FLASH_BASE, swd_total_bytes)) {
    app_log_error("SWD: Flash erase failed");
    set_programming_status(PROG_STATUS_ERROR);
    return false;
  }
  
  // Step 6: Program flash
  set_programming_status(PROG_STATUS_PROGRAMMING);
  if (!swd_write_flash(TARGET_FLASH_BASE, test_alpha_bin, swd_total_bytes)) {
    app_log_error("SWD: Flash programming failed");
    set_programming_status(PROG_STATUS_ERROR);
    return false;
  }
  
  // Step 7: Verify flash
  set_programming_status(PROG_STATUS_VERIFYING);
  if (!swd_verify_flash(TARGET_FLASH_BASE, test_alpha_bin, swd_total_bytes)) {
    app_log_error("SWD: Flash verification failed");
    set_programming_status(PROG_STATUS_ERROR);
    return false;
  }
  
  // Step 8: Reset target
  swd_send_progress_notification("Resetting target device...");
  swd_write_memory(AIRCR_ADDRESS, 0xFA050004);
  
  swd_programming_progress = 100;
  app_log_info("SWD: Complete programming sequence finished successfully!");
  set_programming_status(PROG_STATUS_SUCCESS);
  
  return true;
}

/***************************************************************************//**
 * Send programming progress notification via Bluetooth
 ******************************************************************************/
static void swd_send_progress_notification(const char* message)
{
  sl_status_t sc;
  size_t message_len = strlen(message);
  
  app_log_info("SWD Progress: %s", message);
  
  // Send via Bluetooth if connected
  sc = sl_bt_gatt_server_notify_all(gattdb_report_button,
                                    message_len,
                                    (uint8_t*)message);
  
  if (sc == SL_STATUS_OK) {
    app_log_info("SWD: Progress notification sent via Bluetooth");
  } else {
    app_log_warning("SWD: Failed to send progress notification: 0x%04lX", sc);
  }
}

/***************************************************************************//**
 * Power up debug interface
 ******************************************************************************/
static bool swd_power_up_debug(void)
{
  app_log_info("SWD: Powering up debug interface");
  
  // Set CDBGPWRUPREQ and CSYSPWRUPREQ
  uint32_t ctrl_stat = 0x50000000;
  uint8_t response = swd_transaction(false, SWD_DP_CTRL_STAT, &ctrl_stat);
  
  if (response != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to write CTRL/STAT register");
    return false;
  }
  
  // Wait for power up acknowledgment
  for (int retry = 0; retry < 100; retry++) {
    response = swd_transaction(true, SWD_DP_CTRL_STAT, &ctrl_stat);
    if (response == SWD_RESPONSE_OK) {
      if ((ctrl_stat & 0xA0000000) == 0xA0000000) {
        app_log_info("SWD: Debug interface powered up successfully");
        return true;
      }
    }
    swd_delay_us(1000); // 1ms delay
  }
  
  app_log_error("SWD: Debug power up timeout, CTRL/STAT=0x%08lX", ctrl_stat);
  return false;
}

/***************************************************************************//**
 * Configure AHB-AP for memory access
 ******************************************************************************/
static bool swd_configure_ahb_ap(void)
{
  app_log_info("SWD: Configuring AHB-AP");
  
  // Select AHB-AP, bank 0xF to read IDR
  uint32_t select_val = 0x000000F0;
  uint8_t response = swd_transaction(false, SWD_DP_SELECT, &select_val);
  if (response != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to select AHB-AP bank F");
    return false;
  }
  
  // Read IDR to verify AHB-AP
  uint32_t idr_value;
  response = swd_transaction(true, SWD_AP_IDR, &idr_value);
  if (response != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to read AHB-AP IDR");
    return false;
  }
  
  app_log_info("SWD: AHB-AP IDR = 0x%08lX", idr_value);
  
  // Check for known IDR values
  if (idr_value == AHB_AP_IDR_DEBUG_LOCKED) {
    app_log_error("SWD: Device is debug locked!");
    return false;
  } else if (idr_value == AHB_AP_IDR_CORTEX_M3) {
    app_log_info("SWD: Cortex-M3/M4 device detected");
    swd_determine_device_type(idr_value);
  } else if (idr_value == AHB_AP_IDR_CORTEX_M0P) {
    app_log_info("SWD: Cortex-M0+ device detected");
    swd_determine_device_type(idr_value);
  } else {
    app_log_warning("SWD: Unknown AHB-AP IDR: 0x%08lX, continuing...", idr_value);
  }
  
  // Select AHB-AP, bank 0 for normal operation
  select_val = 0x00000000;
  response = swd_transaction(false, SWD_DP_SELECT, &select_val);
  if (response != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to select AHB-AP bank 0");
    return false;
  }
  
  // Configure CSW for 32-bit transfers with auto-increment
  uint32_t csw_val = CSW_DEFAULT_VALUE;
  response = swd_transaction(false, SWD_AP_CSW, &csw_val);
  if (response != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to configure CSW");
    return false;
  }
  
  app_log_info("SWD: AHB-AP configured successfully");
  return true;
}

/***************************************************************************//**
 * Determine device type and set parameters
 ******************************************************************************/
static void swd_determine_device_type(uint32_t idr_value)
{
  // Default to Series 0 settings
  msc_base_address = MSC_BASE_SERIES0;
  flash_page_size = FLASH_PAGE_SIZE_2K;
  device_is_series1 = false;
  
  // TODO: Could determine specific device type from device info page
  // For now, assume Series 0 device with 2KB pages
  app_log_info("SWD: Using MSC base 0x%08lX, page size %lu bytes (IDR: 0x%08lX)", 
               msc_base_address, flash_page_size, idr_value);
}

/***************************************************************************//**
 * Halt target CPU
 ******************************************************************************/
static bool swd_halt_cpu(void)
{
  app_log_info("SWD: Halting target CPU");
  
  // Halt the CPU
  if (!swd_write_memory(DHCSR_ADDRESS, 0xA05F0003)) {
    app_log_error("SWD: Failed to halt CPU");
    return false;
  }
  
  // Enable halt-on-reset
  if (!swd_write_memory(DEMCR_ADDRESS, 0x00000001)) {
    app_log_error("SWD: Failed to set halt-on-reset");
    return false;
  }
  
  // Reset the CPU (will halt on first instruction)
  if (!swd_write_memory(AIRCR_ADDRESS, 0xFA050004)) {
    app_log_error("SWD: Failed to reset CPU");
    return false;
  }
  
  app_log_info("SWD: CPU halted and reset successfully");
  return true;
}

/***************************************************************************//**
 * Read memory via AHB-AP
 ******************************************************************************/
static uint32_t swd_read_memory(uint32_t address)
{
  // Set TAR
  if (swd_transaction(false, SWD_AP_TAR, &address) != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to set TAR for read");
    return 0xFFFFFFFF;
  }
  
  // Read via DRW
  uint32_t data;
  if (swd_transaction(true, SWD_AP_DRW, &data) != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to read DRW");
    return 0xFFFFFFFF;
  }
  
  return data;
}

/***************************************************************************//**
 * Write memory via AHB-AP
 ******************************************************************************/
static bool swd_write_memory(uint32_t address, uint32_t data)
{
  // Set TAR
  if (swd_transaction(false, SWD_AP_TAR, &address) != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to set TAR for write");
    return false;
  }
  
  // Write via DRW
  if (swd_transaction(false, SWD_AP_DRW, &data) != SWD_RESPONSE_OK) {
    app_log_error("SWD: Failed to write DRW");
    return false;
  }
  
  return true;
}

/***************************************************************************//**
 * Mass erase flash
 ******************************************************************************/
static bool swd_mass_erase(void)
{
  app_log_info("SWD: Performing mass erase");
  
  // Enable flash writing
  uint32_t msc_writectrl = msc_base_address + MSC_WRITECTRL;
  if (!swd_write_memory(msc_writectrl, MSC_WRITECTRL_WREN)) {
    app_log_error("SWD: Failed to enable flash writing");
    return false;
  }
  
  // Start mass erase
  uint32_t msc_writecmd = msc_base_address + MSC_WRITECMD;
  if (!swd_write_memory(msc_writecmd, MSC_WRITECMD_ERASEMAIN0)) {
    app_log_error("SWD: Failed to start mass erase");
    return false;
  }
  
  // Wait for completion
  app_log_info("SWD: Waiting for mass erase completion...");
  sl_sleeptimer_delay_millisecond(FLASH_ERASE_DELAY_MS);
  
  app_log_info("SWD: Mass erase completed");
  return true;
}

/***************************************************************************//**
 * Erase flash page
 ******************************************************************************/
static bool swd_page_erase(uint32_t page_addr)
{
  // Enable flash writing
  uint32_t msc_writectrl = msc_base_address + MSC_WRITECTRL;
  if (!swd_write_memory(msc_writectrl, MSC_WRITECTRL_WREN)) {
    return false;
  }
  
  // Load page address
  uint32_t msc_addrb = msc_base_address + MSC_ADDRB;
  if (!swd_write_memory(msc_addrb, page_addr)) {
    return false;
  }
  
  // Load address into internal register
  uint32_t msc_writecmd = msc_base_address + MSC_WRITECMD;
  if (!swd_write_memory(msc_writecmd, MSC_WRITECMD_LADDRIM)) {
    return false;
  }
  
  // Start page erase
  if (!swd_write_memory(msc_writecmd, MSC_WRITECMD_ERASEPAGE)) {
    return false;
  }
  
  // Wait for completion
  sl_sleeptimer_delay_millisecond(FLASH_ERASE_DELAY_MS);
  
  return true;
}

/***************************************************************************//**
 * Erase flash memory range
 ******************************************************************************/
static bool swd_erase_flash(uint32_t start_addr, uint32_t size)
{
  app_log_info("SWD: Erasing flash from 0x%08lX, size %lu bytes", start_addr, size);
  
  // If erasing from beginning and size is large, use mass erase
  if (start_addr == TARGET_FLASH_BASE && size > (64 * 1024)) {
    return swd_mass_erase();
  }
  
  // Page-by-page erase
  uint32_t pages_to_erase = (size + flash_page_size - 1) / flash_page_size;
  app_log_info("SWD: Erasing %lu pages of %lu bytes each", pages_to_erase, flash_page_size);
  
  for (uint32_t i = 0; i < pages_to_erase; i++) {
    uint32_t page_addr = start_addr + (i * flash_page_size);
    
    app_log_info("SWD: Erasing page %lu/%lu at 0x%08lX", i+1, pages_to_erase, page_addr);
    
    if (!swd_page_erase(page_addr)) {
      app_log_error("SWD: Failed to erase page at 0x%08lX", page_addr);
      return false;
    }
    
    // Update progress
    swd_programming_progress = (i * 30) / pages_to_erase;  // Erase = 30% of total
    char progress_msg[64];
    snprintf(progress_msg, sizeof(progress_msg), "Erased page %lu/%lu", i+1, pages_to_erase);
    swd_send_progress_notification(progress_msg);
  }
  
  app_log_info("SWD: Flash erase completed successfully");
  return true;
}

/***************************************************************************//**
 * Write flash memory
 ******************************************************************************/
static bool swd_write_flash(uint32_t start_addr, const uint8_t *data, uint32_t size)
{
  app_log_info("SWD: Writing %lu bytes to flash at 0x%08lX", size, start_addr);
  
  // Enable flash writing
  uint32_t msc_writectrl = msc_base_address + MSC_WRITECTRL;
  if (!swd_write_memory(msc_writectrl, MSC_WRITECTRL_WREN)) {
    app_log_error("SWD: Failed to enable flash writing");
    return false;
  }
  
  uint32_t words_to_write = (size + 3) / 4;  // Round up to word boundary
  uint32_t msc_addrb = msc_base_address + MSC_ADDRB;
  uint32_t msc_wdata = msc_base_address + MSC_WDATA;
  uint32_t msc_writecmd = msc_base_address + MSC_WRITECMD;
  
  app_log_info("SWD: Writing %lu words to flash", words_to_write);
  
  for (uint32_t i = 0; i < words_to_write; i++) {
    uint32_t word_addr = start_addr + (i * 4);
    
    // Prepare word data (handle partial words at end)
    uint32_t word_data = 0xFFFFFFFF;
    for (int j = 0; j < 4 && (i * 4 + j) < size; j++) {
      word_data = (word_data & ~(0xFF << (j * 8))) | (data[i * 4 + j] << (j * 8));
    }
    
    // Load address
    if (!swd_write_memory(msc_addrb, word_addr)) {
      app_log_error("SWD: Failed to set address for word %lu", i);
      return false;
    }
    
    if (!swd_write_memory(msc_writecmd, MSC_WRITECMD_LADDRIM)) {
      app_log_error("SWD: Failed to load address for word %lu", i);
      return false;
    }
    
    // Write data
    if (!swd_write_memory(msc_wdata, word_data)) {
      app_log_error("SWD: Failed to write data for word %lu", i);
      return false;
    }
    
    // Trigger write
    if (!swd_write_memory(msc_writecmd, MSC_WRITECMD_WRITETRIG)) {
      app_log_error("SWD: Failed to trigger write for word %lu", i);
      return false;
    }
    
    // Wait for write completion
    swd_delay_us(FLASH_WRITE_DELAY_US);
    
    // Progress update every 1000 words
    if (i % 1000 == 0) {
      swd_programming_progress = 30 + ((i * 50) / words_to_write);  // Write = 50% of total
      char progress_msg[64];
      snprintf(progress_msg, sizeof(progress_msg), "Wrote %lu/%lu words", i, words_to_write);
      swd_send_progress_notification(progress_msg);
      app_log_info("SWD: %s", progress_msg);
    }
  }
  
  app_log_info("SWD: Flash write completed successfully");
  return true;
}

/***************************************************************************//**
 * Verify flash memory
 ******************************************************************************/
static bool swd_verify_flash(uint32_t start_addr, const uint8_t *data, uint32_t size)
{
  app_log_info("SWD: Verifying %lu bytes at 0x%08lX", size, start_addr);
  
  uint32_t words_to_verify = (size + 3) / 4;
  uint32_t errors = 0;
  
  for (uint32_t i = 0; i < words_to_verify; i++) {
    uint32_t word_addr = start_addr + (i * 4);
    
    // Read word from flash via AHB-AP with auto-increment
    uint32_t flash_data = swd_read_memory(word_addr);
    
    // Prepare expected word data
    uint32_t expected_data = 0xFFFFFFFF;
    for (int j = 0; j < 4 && (i * 4 + j) < size; j++) {
      expected_data = (expected_data & ~(0xFF << (j * 8))) | (data[i * 4 + j] << (j * 8));
    }
    
    // Compare
    if (flash_data != expected_data) {
      app_log_error("SWD: Verify error at 0x%08lX: expected 0x%08lX, got 0x%08lX", 
                   word_addr, expected_data, flash_data);
      errors++;
      if (errors > 10) {
        app_log_error("SWD: Too many verification errors, aborting");
        return false;
      }
    }
    
    // Progress update every 1000 words
    if (i % 1000 == 0) {
      swd_programming_progress = 80 + ((i * 20) / words_to_verify);  // Verify = 20% of total
      char progress_msg[64];
      snprintf(progress_msg, sizeof(progress_msg), "Verified %lu/%lu words", i, words_to_verify);
      swd_send_progress_notification(progress_msg);
      app_log_info("SWD: %s", progress_msg);
    }
  }
  
  if (errors == 0) {
    app_log_info("SWD: Flash verification completed successfully");
    return true;
  } else {
    app_log_error("SWD: Flash verification failed with %lu errors", errors);
    return false;
  }
}