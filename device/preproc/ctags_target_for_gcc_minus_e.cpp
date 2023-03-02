# 1 "/home/runner/work/doorhandlelock/doorhandlelock/device/device.ino"
# 2 "/home/runner/work/doorhandlelock/doorhandlelock/device/device.ino" 2
using namespace pimoroni;

// Bootsel, fingerprint sensor and servo globals
__Bootsel BOOTSEL;
SerialPIO mySerial(0, 1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
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
String ssid;

// Display globals
ST7789 display(320, 240, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT));
PicoGraphics_PenRGB565 graphics(display.width, display.height, nullptr);
uint8_t last_pen[3];
Pen BLACK;
Pen WHITE;
Pen PRIMARY;

// Unlock servo function
void unlock(void) {
  // Note that this is a blocking call, the bootsell button will not be checked until the servo has completed its movement!
  for (int pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    servo.write(pos); // tell servo to go to position in variable 'pos'
    delay(1); // waits 1ms for the servo to reach the position
  }
  delay(1000);
  for (int pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
    servo.write(pos); // tell servo to go to position in variable 'pos'
    delay(1); // waits 1ms for the servo to reach the position
  }
}

// Setup 
void setup(void)
{
  // Misc setup
  pinMode((32u), OUTPUT);
  memset(last_pen, 0, sizeof(last_pen));
  memset(packet, 0, sizeof(packet));

  // Display setup
  graphics.clear();
  BLACK = graphics.create_pen(0, 0, 0);
  WHITE = graphics.create_pen(255, 255, 255);
  PRIMARY = graphics.create_pen(176, 196, 222);


  display.set_backlight(255);
  display.update(&graphics);
  graphics.set_pen(BLACK);
  graphics.rectangle(Rect(0, 0, 320, 240));
  graphics.set_pen(PRIMARY);
  graphics.text("Door Lock", Point(10, 10), true, 2);
  display.update(&graphics);

  // Wi-Fi setup
  AP_MODE = true;
  WiFi.mode(WIFI_STA);

  // Get mac address of device
  mac = WiFi.macAddress();
  ssid = "Door Lock - " + mac.substring(0, 6);

  WiFi.softAP(ssid); // leave password empty for open AP
  server.begin();

  // Attach the servo motor
  servo.attach(28);

  // Set the baudrate for the sensor serial port
  finger.begin(57600);
  finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);

  if (finger.verifyPassword()) {
    // Found the sensor
    finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);
  } else {
    // Failed to find sensor
    // Pico LED blink is our error indicator in this case (since we couldn't find the sensor)
    while (true) {
      gpio_put((32u), 1);
      sleep_ms(100);
      gpio_put((32u), 0);
      sleep_ms(100);
    }
  }

  finger.getParameters();

  finger.getTemplateCount();
  if (finger.templateCount == 0) {
    // No fingerprints stored
    while (true)
      while (finger.getImage() != 0x02 /*!< No finger on the sensor*/) {
        finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);
      }
      while(! fingerprint_enroll() );
  } else {
    // Fingerprints stored
    finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
  }
}

// Main loop
void loop(void)
{
  if (client) {
    if (AP_MODE)
    {
      unlock();
      draw_blank_screen();
      graphics.set_pen(WHITE);
      graphics.text("Connecting to", Point(10, 10), true, 2);
      display.update(&graphics);

      // AP mode will receive the home networks 
      // SSID and password in the form SSID?PASSWORD
      client.readBytes(packet, 255);
      int question = 0;
      int semicolon = 0;
      for (int i = 0; i < 255; i++) {
        if (packet[i] == '?') {
          packet[i] = 0;
          question = i;
        }
        else if (packet[i] == ';') {
          packet[i] = 0;
          semicolon = i;
          break;
        }
      }
      char *home_ssid = (char *)packet;
      char *home_password = (char *)packet + question + 1;

      graphics.text(home_ssid, Point(10, 50), true, 2);
      graphics.text(home_password, Point(10, 70), true, 2);
      display.update(&graphics);

      // connect to home network
      AP_MODE = false;
      server.close();
      WiFi.softAPdisconnect(true);
      WiFi.begin("SKYVUY5A", "oops hehe");

      timeout = timeout > 30 ? timeout : 30; // 30 * 500 = 15 seconds, reasonable time

      while (WiFi.status() != WL_CONNECTED and timeout > 0) {
        // temp blocking loop - move to second core
        // otherwise if this hangs forever, the
        // rest of the program will not run
        delay(500);
        finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);
        timeout--;
        if (WiFi.status() == WL_CONNECT_FAILED) {
          break;
        }
      }

      if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(); // cancel connection attempt, if any
        // in the case of multiple retries, that are caused by
        // the connection simply being slow, we will increase the timeout attempts
        if (timeout == 0) {
          timeout = 30 + timeout_safety;
          timeout_safety += 8;
        }
        // turn AP back on
        AP_MODE = true;
        WiFi.softAP(ssid); // leave password empty for open AP
        server.begin();
        finger.LEDcontrol(0x02 /*!< Flashing light*/, 25, 0x01 /*!< Red LED*/, 10);
        delay(500);
        draw_blank_screen();
        graphics.set_pen(WHITE);
        graphics.text("Failed to connect, please make sure your details are correct. Rejoin the Door Lock xx:xx network and try sending them again.", Point(10, 10), true, 2);
        display.update(&graphics);
        return;
      }
      // get my ip address
      IPAddress ip = WiFi.localIP();

      draw_blank_screen();
      graphics.set_pen(WHITE);
      graphics.text("Connected with IP address", Point(10, 10), true, 2);
      graphics.text(ip.toString().c_str(), Point(10, 70), true, 2);
      display.update(&graphics);

      server.begin();
      finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
      delay(500);
    }
    else {
      size_t read = client.readBytes(packet, 5);
      if (read == 0) {
        //unlock();
      }
      else if (read != 5) {
        // unknown command
      }
      else {
        if (memcmp(packet, "UNLCK", 5) == 0) {
          unlock();
        }
        else if (memcmp(packet, "IMAGE", 5) == 0) {
          while (client.connected()) {
            if (client.available()) {
              size_t read = client.readBytes(packet, 255);
              int index = 0;
              while (read >= 3) {
                if (last_pen[0] != packet[index] || last_pen[1] != packet[index + 1] || last_pen[2] != packet[index + 2]) {
                  last_pen[0] = packet[index];
                  last_pen[1] = packet[index + 1];
                  last_pen[2] = packet[index + 2];
                  graphics.set_pen(graphics.create_pen(packet[index], packet[index + 1], packet[index + 2]));
                }
                int x = packet[index + 3] + packet[index + 4];
                int y = packet[index + 5];
                if (x > 320 || y > 240) {
                  continue; // not sure what to do here but continue for now
                }
                graphics.pixel(Point(x, y));
                index += 6;
                read -= 6;
              }
            }
            display.update(&graphics);
          }
        }
      }
    }
  }
  else {
    client = server.available();
  }

  int p = 0;
  if ( get_bootsel_button() ) {
    if (finger.templateCount == 0) {
      // No fingerprints stored, so enroll a new one
      while(! fingerprint_enroll());
    }
    else {
      // We need confirmation from an existing fingerprint before being able to enroll a new one
      if (fingerprint_get_id() > 0) {
        finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
        while(! fingerprint_enroll() );
      }
      else {
        //LED_OFF;
      }
    }
  }
  else {
    p = fingerprint_get_id();
    if (p == -1) {
      finger.LEDcontrol(0x04 /*!< Always off*/, 0, (0x01 /*!< Red LED*/));
      finger.LEDcontrol(0x04 /*!< Always off*/, 0, (0x02 /*!< Blue LED*/));
      finger.LEDcontrol(0x04 /*!< Always off*/, 0, (0x03 /*!< Purple LEDpassword*/));
    }
    else if (p > 0) {
      unlock();
    }
  }
}

// Fingerprint helper function definitions
uint8_t fingerprint_enroll(void) {
  int p = -1;
  finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);
  while(p != 0x00 /*!< Command execution is complete*/) {
    p = finger.getImage();
    switch(p) {
    case 0x00 /*!< Command execution is complete*/:
      // Image taken
      break;
    case 0x02 /*!< No finger on the sensor*/:
      // Waiting for finger
      break;
    case 0x01 /*!< Error when receiving data package*/:
      // Communication error
      break;
    case 0x03 /*!< Failed to enroll the finger*/:
      // Imaging error
      break;
    default:
      // Unknown error 
      break;
    }
  }

  p = finger.image2Tz(1);
  switch(p) {
    case 0x00 /*!< Command execution is complete*/:
      // Image converted
      break;
    case 0x06 /*!< Failed to generate character file due to overly disorderly*/:
      // Image too messy
      return p;
    case 0x01 /*!< Error when receiving data package*/:
      // Communication error
      return p;
    case 0x07 /*!< Failed to generate character file due to the lack of character point*/:
      // Could not find fingerprint features
      return p;
    case 0x15 /*!< Failed to generate image because of lac of valid primary image*/:
      // Could not find fingerprint features
      return p;
    default:
      // Unknown error 
      return p;
  }

  // Get user to remove finger
  finger.LEDcontrol(0x04 /*!< Always off*/, 0, (0x03 /*!< Purple LEDpassword*/));
  delay(1000);

  // Get user to place finger again
  finger.LEDcontrol(0x03 /*!< Always on*/, 0, (0x02 /*!< Blue LED*/));
  p = 0;

  while(p != 0x02 /*!< No finger on the sensor*/) {
    p = finger.getImage();
  }

  p = -1;

  while(p != 0x00 /*!< Command execution is complete*/) {
    p = finger.getImage();
    switch(p) {
    case 0x00 /*!< Command execution is complete*/:
      // Image taken
      break;
    case 0x02 /*!< No finger on the sensor*/:
      // Waiting for finger
      break;
    case 0x01 /*!< Error when receiving data package*/:
      // Communication error
      break;
    case 0x03 /*!< Failed to enroll the finger*/:
      // Imaging error
      break;
    default:
      // Unknown error 
      break;
    }
  }

  p = finger.image2Tz(2);
  switch (p) {
    case 0x00 /*!< Command execution is complete*/:
      // Image converted
      break;
    case 0x06 /*!< Failed to generate character file due to overly disorderly*/:
      // Image too messy
      return p;
    case 0x01 /*!< Error when receiving data package*/:
      // Communication error
      return p;
    case 0x07 /*!< Failed to generate character file due to the lack of character point*/:
      // Could not find fingerprint features
      return p;
    case 0x15 /*!< Failed to generate image because of lac of valid primary image*/:
      // Could not find fingerprint features
      return p;
    default:
      // Unknown error 
      return p;
  }

  // Create fingerprint model
  p = finger.createModel();
  if (p == 0x00 /*!< Command execution is complete*/) {
    // Prints matched
  } else if (p == 0x01 /*!< Error when receiving data package*/) {
    // Communication error
    return p;
  } else if (p == 0x0A /*!< Failed to combine the character files*/) {
    // Fingerprints did not match
    return p;
  } else {
    // Unknown error
    return p;
  }

  // TODO: Do something better with ID
  int id = finger.getTemplateCount() + 1;
  if (id > 127) {
    finger.LEDcontrol(0x05 /*!< Gradually on*/, 200, 0x01 /*!< Red LED*/);
    // TODO: This is not good, enable deletion of fingerprints
  }
  p = finger.storeModel(id);
  if (p == 0x00 /*!< Command execution is complete*/) {
    // Stored
  } else if (p == 0x01 /*!< Error when receiving data package*/) {
    // Communication error
    return p;
  } else if (p == 0x0B /*!< Addressed PageID is beyond the finger library*/) {
    // Could not store in that location
    return p;
  } else if (p == 0x18 /*!< Error when writing flash*/) {
    // Error writing to flash !uh oh!
    return p;
  } else {
    // Unknown error 
    return p;
  }

  finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
  return true;
}

uint8_t fingerprint_get_id(void) {
  uint8_t p = finger.getImage();
  switch (p) {
    case 0x00 /*!< Command execution is complete*/:
      // Image taken
      break;
    case 0x02 /*!< No finger on the sensor*/:
      // Waiting for finger
      return 0;
    case 0x01 /*!< Error when receiving data package*/:
      // Communication error
      return 0;
    case 0x03 /*!< Failed to enroll the finger*/:
      // Imaging error
      return 0;
    default:
      // Unknown error 
      return 0;
  }

  p = finger.image2Tz();
  switch (p) {
    case 0x00 /*!< Command execution is complete*/:
      // Image converted
      break;
    case 0x06 /*!< Failed to generate character file due to overly disorderly*/:
      // Image too messy
      return 0;
    case 0x01 /*!< Error when receiving data package*/:
      // Communication error
      return 0;
    case 0x07 /*!< Failed to generate character file due to the lack of character point*/:
      // Could not find fingerprint features
      return 0;
    case 0x15 /*!< Failed to generate image because of lac of valid primary image*/:
      // Could not find fingerprint features
      return 0;
    default:
      // Unknown error 
      return 0;
  }

  p = finger.fingerSearch();
  if (p == 0x00 /*!< Command execution is complete*/) {
    // Found a match!
  } else if (p == 0x01 /*!< Error when receiving data package*/) {
    // Communication error
    return 0;
  } else if (p == 0x09 /*!< Failed to find matching finger*/) {
    // Failed to find match
    return 0;
  } else {
    // Unknown error 
    return 0;
  }

  // found a match!
  int id = finger.fingerID;
  int confidence = finger.confidence;

  if (confidence > 50) {
    finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
    return id;
  } else {
    finger.LEDcontrol(0x02 /*!< Flashing light*/, 25, 0x01 /*!< Red LED*/, 10);
    return -1;
  }
}

// Display helper function defintions
void draw_blank_screen(void) { // I think this can be made better
  graphics.set_pen(BLACK);
  graphics.rectangle(Rect(0, 0, 320, 240));
  display.update(&graphics);
}

void draw_navbar(void) {
  graphics.set_pen(PRIMARY);
  graphics.rectangle(Rect(0, 0, 320, int(240 / 8)));
  graphics.set_pen(BLACK);
  graphics.text("Door lock", Point(5, 5), 50, 3);
  graphics.set_pen(WHITE);
  graphics.rectangle(Rect(120, 5, 3, 19));
  graphics.set_pen(BLACK);
  std::string message;
  switch(wifi_status)
  {
    case WIFI_CHIP_ERROR:
      message = "ERROR";
      break;
    case WIFI_FAILED:
      message = "FAILED";
      break;
    case WIFI_CONNECTED:
      message = "ipaddres";
      break;
    case WIFI_CONNECTING:
      message = "CONNECTING";
      break;
    case WIFI_NOT_CONNECTED:
      message = "NOT CONNECTED";
      break;
    default:
      message = "FATAL";
    }
    graphics.text(message, Point(130, 8), 200, 2);
    display.update(&graphics);
}
