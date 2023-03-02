#line 1 "/home/runner/work/doorhandlelock/doorhandlelock/device/device.hpp"
/***************************************************
  This is an example sketch for our optical Fingerprint sensor with LED ring

  These displays use TTL Serial to communicate, 2 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include <vector>
#include <unordered_map>
#include <functional>

#include <string.h>
#include <math.h>
#include <Adafruit_Fingerprint.h>
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "Servo.h"
#include "WiFi.h"
#include "WiFiClient.h"
#include "WiFiServer.h"
#include "WiFiUdp.h"

#include "src/picow/libraries/pico_display_2/pico_display_2.hpp"
#include "src/picow/drivers/st7789/st7789.hpp"
#include "src/picow/libraries/pico_graphics/pico_graphics.hpp"
#include "src/picow/drivers/button/button.hpp"

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// Wi-Fi enum codes
enum {
  WIFI_NOT_CONNECTED,
  WIFI_CONNECTING,
  WIFI_FAILED,
  WIFI_CHIP_ERROR,
  WIFI_CONNECTED
};

// LED control macros
#define LED_OFF(colour) finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, (colour))
#define LED_ON(colour) finger.LEDcontrol(FINGERPRINT_LED_ON, 0, (colour))
#define LED_FLASH(colour, repeat) finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, (colour), (repeat) ? (repeat) : (5)) // default flashing value!
#define LED_BREATHE(colour) finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, (colour))

#define LED_RED FINGERPRINT_LED_RED
#define LED_BLUE FINGERPRINT_LED_BLUE
#define LED_PURPLE FINGERPRINT_LED_PURPLE

#define LED_SETUP finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_PURPLE)
#define LED_FATAL finger.LEDcontrol(FINGERPRINT_LED_GRADUAL_ON, 200, FINGERPRINT_LED_RED)
#define LED_SUCCESS finger.LEDcontrol(FINGERPRINT_LED_BREATHING, 100, FINGERPRINT_LED_BLUE)
#define LED_ERROR finger.LEDcontrol(FINGERPRINT_LED_FLASHING, 25, FINGERPRINT_LED_RED, 10)

// Servo control macros
#define SERVO_LOCK servo.write(0);
#define SERVO_UNLOCK servo.write(180);

// Miscellaneous macros
#define CONFIDENCE_THRESHOLD 50
#define PICO_LED LED_BUILTIN

// Fingerprint helper function prototypes
uint8_t fingerprint_enroll(void);
uint8_t fingerprint_get_id(void);

// Display helper function prototypes
void draw_blank_screen(void);
void draw_navbar(void);

// Define temporary bootsel button
static bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;

    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    noInterrupts();
    rp2040.idleOtherCore();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    for (volatile int i = 0; i < 1000; ++i);

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
    bool button_state = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    rp2040.resumeOtherCore();
    interrupts();

    return button_state;
}

__Bootsel::operator bool() {
    return get_bootsel_button();
}