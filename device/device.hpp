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

// C / C++ libraries
#include <vector>
#include <mutex>
#include <math.h>
#include <string.h>
#include <functional>
#include <unordered_map>

// Pico libraries
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <hardware/structs/ioqspi.h>
#include <hardware/structs/sio.h>

// Arduino - Pico libraries
#include <EEPROM.h>
#include <Servo.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

// External libraries (Adafruit)
#include <Adafruit_Fingerprint.h>

// External libraries (Pimoroni)
#include "src/picow/libraries/pico_display_2/pico_display_2.hpp"
#include "src/picow/drivers/st7789/st7789.hpp"
#include "src/picow/libraries/pico_graphics/pico_graphics.hpp"
#include "src/picow/drivers/button/button.hpp"

// Display config
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
#define CONFIDENCE_THRESHOLD 50 // for fingerprint matching
#define PICO_LED LED_BUILTIN // for pico built-in LED

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

// *********************************//
//              Globals             //
// *********************************//
using namespace pimoroni;

// Bootsel globals 
__Bootsel::operator bool() {
    return get_bootsel_button();
}
__Bootsel BOOTSEL;

// Fingerprint globals
SerialPIO mySerial(0, 1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Display globals
ST7789 display(320, 240, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT));
PicoGraphics_PenRGB565 graphics(display.width, display.height, nullptr);
uint8_t last_pen[3];
Pen BLACK;
Pen WHITE;
Pen PRIMARY;

// Servo globals
Servo servo;

// Wi-Fi globals
WiFiServer server(9999);
WiFiClient client;
bool AP_MODE = true;
uint8_t packet[255];
int wifi_status = WIFI_NOT_CONNECTED;
int timeout = 30;
int timeout_safety = 8;
String mac;
String ssid; // tidy this up later

// EEPROM globals
int addr = 0;

// << Helper function prototypes >> //
// Fingerprint helper function prototypes
uint8_t fingerprint_enroll(void);
uint8_t fingerprint_get_id(void);

// Display helper function prototypes
void display_blank(void);
void display_navbar(void);
void display_setting_up(void);
// std::mutex display_mutex; // Mutex for locking shared display functions.

// Servo helper function prototypes
void servo_unlock(void);
// std::mutex servo_mutex; // Mutex for locking shared servo functions.

// Wi-Fi helper function prototypes
char wifi_connect(char *name, char *pass);
char wifi_first_time_setup(void);
char wifi_second_time_setup(void);
// No mutex required as only Core 1 ever touches the Wi-Fi functions.

#define SETUP_SUCCESS 1
#define SETUP_FAILED 2
std::vector<char> devices_setup = {1, 2, 1, 0, 0}; // make sure this vector is never resized or reset
#define DISPLAY_SETUP devices_setup[0]
#define SENSOR_SETUP devices_setup[1]
#define SERVO_SETUP devices_setup[2]
#define EEPROM_SETUP devices_setup[3]
#define WIFI_SETUP devices_setup[4]
// blank space so github realises i made a change.