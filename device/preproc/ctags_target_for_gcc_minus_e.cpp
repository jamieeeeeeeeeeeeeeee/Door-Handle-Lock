# 1 "/home/runner/work/Door-Handle-Lock/Door-Handle-Lock/device/device.ino"
// #define EEPROM_RESET
# 3 "/home/runner/work/Door-Handle-Lock/Door-Handle-Lock/device/device.ino" 2

// Core 0 Setup - Core 0 handles everything but Wi-Fi and EEPROM! (Fingerprint, display, servo..)
void setup(void) {
  // Clearing memory
  memset(last_pen, 0, sizeof(last_pen));
  memset(packet, 0, sizeof(packet));

  // Pico LED
  pinMode((32u) /* for pico built-in LED*/, OUTPUT);

  // Display setup
  graphics.clear();
  BLACK = graphics.create_pen(0, 0, 0);
  WHITE = graphics.create_pen(255, 255, 255);
  PRIMARY = graphics.create_pen(245, 66, 66);
  RED = graphics.create_pen(255, 0, 0);
  GREEN = graphics.create_pen(0, 255, 0);
  display_blank();
  display_navbar();

  // Attach the servo motor
  if (!servo.attach(28)) {
    devices_setup[2] = '2';
  } else {
    devices_setup[2] = '1';
  }

  // Set the baudrate for the sensor serial port
  finger.begin(57600);
  finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);

  // Check if sensor is connected successfully
  if (finger.verifyPassword()) {
    // Found the sensor
    finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);
    devices_setup[1] = '1';
  } else {
    // Failed to find sensor
    devices_setup[1] = '2';
    // Pico LED blink is our error indicator in this case (since we can't use the sensor)
    /*while (true) {
      gpio_put(PICO_LED, 1);
      sleep_ms(100);
      gpio_put(PICO_LED, 0);
      sleep_ms(100);
    }*/
  }
  display_setting_up();

  // Tell sensor to find its stored details
  finger.getParameters();
  finger.getTemplateCount();

  // Access these stored details ( getParameters and getTemplateCount are blocking :) )
  if (finger.templateCount == 0) {
    // No fingerprints stored
    while (finger.getImage() != 0x02 /*!< No finger on the sensor*/) {
    finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);
    }
    while(! fingerprint_enroll() );
  } else {
    // Fingerprints stored
    finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
  }
}

// Core 1 Setup - Core 1 handles everything Wi-Fi and EEPROM (AP mode, TCP server, connecting to home network..)
void setup1(void) {
  // Setup 1 is ran by the second core (or core1, setup is ran by core0)
  // This is where we will handle WiFi stuff which is blocking
  // The main core will handle the sensor stuff which is also blocking..
  // .. but it will be blocking nothing (if we have them on the same core, then)
  // they will be blocking each other which is not good!

  // EEPROM setup
  addr = 0;
  EEPROM.begin(256); // 256 bytes of EEPROM (1 for being aware of first time setup, 32 SSID + 64 password) some more for later
  devices_setup[3] = '2';

  byte first = EEPROM.read(addr);

  // Quick reset of EEPROM during testing:







  // WiFi setup based on EEPROM that we read
  if (first == 0) {
    //WIFI_SETUP = SETUP_FAILED;
    wifi_first_time_setup();
  } else {
    wifi_second_time_setup();
    //WIFI_SETUP = SETUP_SUCCESS;
  }
}

// Core 0 Main loop 
void loop(void) {
  int p = 0;
  if (get_bootsel_button()) {
    if (finger.templateCount == 0) {
      // No fingerprints stored, so enroll a new one
      while(!fingerprint_enroll());
    } else {
      // We need confirmation from an existing fingerprint before being able to enroll a new one
      if (fingerprint_get_id() > 0) {
        finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
        while(!fingerprint_enroll() );
      } else {
      }
    }
  } else {
    p = fingerprint_get_id();
    if (p == -1) {
      finger.LEDcontrol(0x04 /*!< Always off*/, 0, (0x01 /*!< Red LED*/));
      finger.LEDcontrol(0x04 /*!< Always off*/, 0, (0x02 /*!< Blue LED*/));
      finger.LEDcontrol(0x04 /*!< Always off*/, 0, (0x03 /*!< Purple LEDpassword*/));
    } else if (p > 0) {
      servo_unlock();
    }
  }
}

// Core 1 Main loop
void loop1(void) {
  if (client) {
    if (AP_MODE) {
      display_blank();

      // AP mode will receive the home networks 
      // SSID and password in the form SSID?PASSWORD
      client.readBytes(packet, 255);
      int question = 0;
      for (int i = 0; i < 255; i++) {
        if (packet[i] == '?') {
          packet[i] = 0;
          question = i;
        }
      }
      char *home_ssid = (char *)packet;
      char *home_password = (char *)packet + question + 1;

      // connect to home network
      AP_MODE = false;
      server.close();
      WiFi.softAPdisconnect(true);
      wifi_connect(home_ssid, home_password); //change this later to work with wifi_connect...   

      timeout = timeout > 30 ? timeout : 30; // 30 * 400 = 12 seconds, reasonable time

      while (WiFi.status() != WL_CONNECTED and timeout > 0) {
        // temp blocking loop - move to second core
        delay(400);
        finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);
        timeout--;
        if (WiFi.status() == WL_CONNECT_FAILED) {
          timeout = 0; // important not for breaking but for resetting time out value correctly
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
        delay(400);
        display_blank();
        graphics.set_pen(WHITE);
        graphics.text("Failed to connect, please try again!", Point(10, 10), true, 10);
        display.update(&graphics);
        return;
      }

      /*byte first = EEPROM.read(0);
      if (!first) {
        EEPROM.write(0, 1);
      }
      for (int i = 0; i < 32; i++) {
        EEPROM.write(i + 1, home_ssid[i]);
      }
      for (int i = 64; i < 96; i++) {
        EEPROM.write(i + 1, home_password[i]);
      }
      EEPROM.commit();*/
      // get my ip address

      IPAddress ip = WiFi.localIP();

      display_blank();
      graphics.set_pen(WHITE);
      graphics.text("Connected with IP address", Point(10, 10), true, 2);
      graphics.text(ip.toString().c_str(), Point(10, 70), true, 2);
      display.update(&graphics);

      server.begin();
      finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
    } else {
      size_t read = client.readBytes(packet, 5);
      if (read == 0) {
        //servo_unlock(); ??
      } else if (read != 5) {
        // unknown command??
      } else {
        if (memcmp(packet, "UNLCK", 5) == 0) {
          servo_unlock();
        } else if (memcmp(packet, "IMAGE", 5) == 0) {
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
  } else {
    client = server.available();
  }
}

// << Helper function definitions >> //
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

  if (confidence > 50 /* for fingerprint matching*/) {
    finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x02 /*!< Blue LED*/);
    return id;
  } else {
    finger.LEDcontrol(0x02 /*!< Flashing light*/, 25, 0x01 /*!< Red LED*/, 10);
    return -1;
  }
}

// Display helper function defintions
void display_blank(void) {
  //display_mutex.lock();

  graphics.clear();
  graphics.set_pen(BLACK);
  graphics.rectangle(Rect(0, 0, 320, 240));
  display.update(&graphics);

  //display_mutex.unlock();
}
void display_navbar(void) {
  //display_mutex.lock();

  graphics.set_pen(PRIMARY);
  graphics.rectangle(Rect(0, 0, 320, int(240 / 8)));
  graphics.set_pen(BLACK);
  graphics.text("Door lock", Point(5, 5), 50, 3);
  graphics.set_pen(WHITE);
  graphics.rectangle(Rect(120, 5, 3, 19));
  graphics.set_pen(BLACK);
  std::string message;
  switch(wifi_status) {
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

    //display_mutex.unlock();
}
void display_setting_up(void) {
  // make screen black, make navbar at top that says "SETTING UP"
  //display_mutex.lock();

  graphics.clear();
  graphics.set_pen(BLACK);
  graphics.rectangle(Rect(0, 0, 320, 240));
  graphics.set_pen(PRIMARY);
  graphics.rectangle(Rect(0, 0, 320, int(240 / 8)));
  graphics.set_pen(BLACK);
  graphics.text("Setting up", Point(5, 5), 200, 3);

  // now loop through devices_setup char vector
  // print the name and status of each device
  // if device is 0 then draw a green smile :)
  // if device is 1 then draw a red frown :(
  // if device is 2 then draw a white ... (waiting)

  for (int i = 0; i < devices_setup.size(); i++) {
    graphics.set_pen(WHITE);
    graphics.text("device1..", Point(5, 40 + (i * 30)), 200, 2);
    graphics.circle(Point(250, 50 + (i * 30)), 10);
    if (devices_setup[i] == '1') {
      graphics.set_pen(GREEN);
      graphics.circle(Point(250, 50 + (i * 30)), 8);
    } else if (devices_setup[i] == '2') {
      graphics.set_pen(RED);
      graphics.circle(Point(250, 50 + (i * 30)), 8);
    }
  }
  display.update(&graphics);
  //display_mutex.unlock();
}

// Servo helper function definitions
void servo_unlock(void) {
  //servo_mutex.lock();

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

  //servo_mutex.unlock();
}

// Wifi helper function definitions
char wifi_connect(char *name, char *pass) {
  // Connect to Wi-Fi
  WiFi.begin(name, pass);
  timeout = timeout > 30 ? timeout : 30; // 30 * 300 = 9 seconds, reasonable time

  while (WiFi.status() != WL_CONNECTED and timeout > 0) {
    // this is of course blocking!
    delay(300);
    finger.LEDcontrol(0x01 /*!< Breathing light*/, 100, 0x03 /*!< Purple LEDpassword*/);
    timeout--;
    if (WiFi.status() == WL_CONNECT_FAILED) {
      timeout = 0; // important not for breaking but for resetting time out value correctly
      break;
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(); // cancel connection attempt, if any
    // in the case of multiple retries, that are caused by
    // .. the connection simply being slow, we will increase the timeout attempts
    if (timeout == 0) {
      timeout = 30 + timeout_safety;
      timeout_safety += 8;
    }
    return 0;
  }
  return 1;
}
char wifi_first_time_setup(void) {
  AP_MODE = true;
  WiFi.mode(WIFI_STA);
  WiFi.softAP("Door Lock - " + mac.substring(0, 6)); // leave password empty for open AP
  // .. as mentioned in PowerPoint we may change to WPA2-PSK later for security..
  server.begin();
  return 1;
}
char wifi_second_time_setup(void) {
  AP_MODE = false;
  byte ssid_bytes[32];
  byte password_bytes[64];
  memset(ssid_bytes, 0, sizeof(ssid_bytes));
  memset(password_bytes, 0, sizeof(password_bytes));

  addr = 1; // important to skip first byte..

  // EEPROM does not get hurt by reading so we can do this as much as we want :)
  for (int i = 0; i < 32; i++) {
    ssid_bytes[i] = EEPROM.read(addr++);
  }

  for (int i = 0; i < 64; i++) {
    password_bytes[i] = EEPROM.read(addr++);
  }

  char* ssid = (char *)ssid_bytes;
  char* password = (char *)password_bytes;

  // this is in unlikley case that first byte of EEPROM has been set
  // .. maybe not reset properly at factory, or cosmic radiation hit it
  // .. and somehow didn't hit anything else
  if (ssid == "" || password == "") {
    wifi_first_time_setup();
  } else {
    // now we try connecting to the network, I think 2 attempts and then give up 
    // .. will be suitable. This will be in the case that the user's home WiFi is down
    // .. what we will do is put the thing into AP mode, but not reset the EEPROM,
    // since if the internet is just temporarily down, for example a power cut, then
    // a simple fix of turning on and off again will suffice :)
    for (int i = 0; i < 2; i++) {
      char connect = wifi_connect(ssid, password);
      if (connect == 1) {
        wifi_status = WIFI_CONNECTED;
        break;
      } else {
        wifi_status = WIFI_NOT_CONNECTED;
      }
    }
    if (wifi_status == WIFI_NOT_CONNECTED) {
      wifi_first_time_setup(); // !
    }
  }
  return 1;
}
