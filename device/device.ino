//#define EEPROM_RESET
#include "device.hpp"

// Core 0 Setup - Core 0 handles everything Wi-Fi and EEPROM (AP mode, TCP server, connecting to home network..)
void setup(void) {
  WiFi.mode(WIFI_STA);
  uint32_t result = rp2040.fifo.pop(); // We need to pause Core 0 here otherwise
  // .. the setup on Core 0 might try and call the display function, but
  // .. if the display is not yet setup over here, then we have
  // .. undefined behaviour!! 
  // (.pop() is blocking until there is something to pop!)

  if (result == 1) {
    // if Core 1 failed, it will be a waste of power to leave it running,
    // .. so we will idle the core
    //rp2040.idleOtherCore();
  }
 
  // Setup 0 is ran by the second core (or core0, setup1 is ran by core1)
  // This is where we will handle WiFi stuff which is blocking
  // The main core will handle the sensor stuff which is also blocking..
  // .. but it will be blocking nothing (if we have them on the same core, then)
  // they will be blocking each other which is not good!
  
  // EEPROM setup
  addr = 0;
  EEPROM.begin(256); // 256 bytes of EEPROM (1 for being aware of first time setup, 32 SSID + 64 password) some more for later
  EEPROM_SETUP = SETUP_SUCCESS;
  display_setting_up();

  // Quick reset of EEPROM during testing:
  #ifdef EEPROM_RESET
  for (int i = 0; i < 256; i++) {
    if (EEPROM.read(i) != 0) {
      EEPROM.write(i, 0);
    }
  }
  EEPROM.commit();
  #endif

  byte first = EEPROM.read(addr);

  // WiFi setup based on EEPROM that we read
  if (first == 0) {
    WIFI_SETUP = SETUP_FAILED;
    display_setting_up();
    wifi_first_time_setup();
  } else {
    WIFI_SETUP = SETUP_SUCCESS;
    display_setting_up();
    wifi_second_time_setup();
  }
}

// Core 0 Main loop
void loop(void) {
  if (client) {
    //display_pulsing_circle("  Client connected!", 2, 1);
    if (AP_MODE) {
      display_blank();

      // AP mode will receive the home networks 
      // SSID and password in the form SSID?PASSWORD
      client.readBytes(packet1, 255);
      int question = 0;
      for (int i = 0; i < 255; i++) {
        if (packet1[i] == '?') {
          packet1[i] = 0;
          question = i;
        }
      }
      char *home_ssid = (char *)packet1;
      char *home_password = (char *)packet1 + question + 1;

      // create char * "Trying to connect to {home_ssid}"
      char *connecting = (char *)malloc(32 + strlen(home_ssid));
      strcpy(connecting, "Trying to connect to ");
      strcat(connecting, home_ssid);
      display_pulsing_circle(connecting, 1, 2);

      // connect to home network
      AP_MODE = false;
      server.close();
      WiFi.softAPdisconnect(true);
      wifi_connect(home_ssid, home_password); //change this later to work with wifi_connect...   
      
      timeout = timeout > 30 ? timeout : 30; // 30 * 400 = 12 seconds, reasonable time

      while (WiFi.status() != WL_CONNECTED and timeout > 0) {
        // temp blocking loop - move to second core
        delay(400); 
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
        display_pulsing_circle("  Connection failed :( Please try again!", 1, 0);
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
            
      display_pulsing_circle("  Connected Successfully!", 1, 1);  
      server.begin();
    } else {
      size_t read = 0;
      for (int i = 0; i < 240; i++) {
        int16_t byt = client.read();
        if (byt == -1) {
          break;
        }
        packet[i] = byt;
        read = i;
      }
      if (read == 0) {
        //servo_unlock(); ??
      } else if (read < 5) {
        // unknown command??
      } else {
        if (memcmp(packet, "UNLCK", 5) == 0) {
          servo_unlock();
        } else {
          // we need to clear in case our image fails.. since
          // .. we dont want to leave a "Connection Success"
          // .. image on screen! This will be very annoying !
          while (client.available()) {
            if (memcmp(packet, "UNLCK", 5) == 0) {
              servo_unlock();
            }
            int index = 0;
            while (index < read) {
              if (last_pen[0] != packet[index] || last_pen[1] != packet[index + 1] || last_pen[2] != packet[index + 2]) {
                last_pen[0] = packet[index];
                last_pen[1] = packet[index + 1];
                last_pen[2] = packet[index + 2];
                graphics.set_pen(graphics.create_pen(packet[index], packet[index + 1], packet[index + 2]));
              }
              int x = packet[index + 3] + packet[index + 4];
              int y = packet[index + 5];
              if (x > 320 || y > 240) {
                index += 6;
                continue; // not sure what to do here but continue for now
              }
              graphics.pixel(Point(x, y));
              index += 6;
              //display.update(&graphics);
            }
            read = 0;
            for (int i = 0; i < 240; i++) {
              int16_t byt = client.read();
              if (byt == -1) {
                break;
              }
              packet[i] = byt;
              read = i;
            }
          }
          display.set_backlight(255.0);
          display.update(&graphics);
        }
      }
    }
  } else {
    client = server.accept();
  }
}


// Core 1 Setup - Core 1 handles everything but Wi-Fi and EEPROM! (Fingerprint, display, servo..)
void setup1(void) {
  // Clearing memory
  memset(last_pen, 0, sizeof(last_pen));
  memset(packet, 0, sizeof(packet));

  // Display setup
  graphics.clear();
  BLACK = graphics.create_pen(0, 0, 0);
  WHITE = graphics.create_pen(255, 255, 255);
  PRIMARY = graphics.create_pen(245, 66, 66);
  RED = graphics.create_pen(255, 0, 0);
  GREEN = graphics.create_pen(0, 255, 0);
  display_blank();
  DISPLAY_SETUP = SETUP_SUCCESS;
  display_setting_up();  

  // Pico LED
  pinMode(PICO_LED, OUTPUT);

  // Attach the servo motor
  if (!!servo.attach(28)) { // servo returns 0 on success, 1 on failure, but I left !! to make
    // .. clear that we are checking for failure
    SERVO_SETUP = SETUP_FAILED;
  } else {
    SERVO_SETUP = SETUP_SUCCESS;
  }
  display_setting_up();

  // Set the baudrate for the sensor serial port
  finger.begin(57600);
  LED_SETUP;

  // Check if sensor is connected successfully
  if (finger.verifyPassword()) {
    // Found the sensor
    LED_SETUP;
    SENSOR_SETUP = SETUP_SUCCESS;
    // Tell sensor to find its stored details
    finger.getParameters();
    finger.getTemplateCount();
    
    // Access these stored details ( getParameters and getTemplateCount are blocking :) )
    if (finger.templateCount == 0) {
      // No fingerprints stored
      while (finger.getImage() != FINGERPRINT_NOFINGER) {
      LED_SETUP;
      }
      while(! fingerprint_enroll() );
    } else {
      // Fingerprints stored
      LED_SUCCESS;
    }
    display_setting_up();
    rp2040.fifo.push(uint32_t(0)); // Letting the other core know it can begin :)
  } else {
    // Failed to find sensor - we still want to allow Wi-Fi..
    SENSOR_SETUP = SETUP_FAILED;
    rp2040.fifo.push(uint32_t(1)); // Letting the other core know it can begin, 
    // .. and that this core can be idled!
  }
}

// Core 1 Main loop 
void loop1(void) {
  int p = 0;
  if (get_bootsel_button()) {
    if (finger.templateCount == 0) {
      // No fingerprints stored, so enroll a new one
      while(!fingerprint_enroll());
    } else {
      // We need confirmation from an existing fingerprint before being able to enroll a new one
      if (fingerprint_get_id() > 0) {
        LED_SUCCESS;
        while(!fingerprint_enroll() );
      } else {
      }
    }
  } else {
    p = fingerprint_get_id();
    if (p == -1) {
      LED_OFF(LED_RED);
      LED_OFF(LED_BLUE);
      LED_OFF(LED_PURPLE);
    } else if (p > 0) {
      servo_unlock();
    }
  }
}


// << Helper function definitions >> //
// Fingerprint helper function definitions
uint8_t fingerprint_enroll(void) {
  int p = -1;
  LED_SETUP;
  while(p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch(p) {
    case FINGERPRINT_OK:
      // Image taken
      break;
    case FINGERPRINT_NOFINGER:
      // Waiting for finger
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Communication error
      break;
    case FINGERPRINT_IMAGEFAIL:
      // Imaging error
      break;
    default:
      // Unknown error 
      break;
    }
  }

  p = finger.image2Tz(1);
  switch(p) {
    case FINGERPRINT_OK:
      // Image converted
      break;
    case FINGERPRINT_IMAGEMESS:
      // Image too messy
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Communication error
      return p;
    case FINGERPRINT_FEATUREFAIL:
      // Could not find fingerprint features
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      // Could not find fingerprint features
      return p;
    default:
      // Unknown error 
      return p;
  }
  
  // Get user to remove finger
  LED_OFF(LED_PURPLE);
  delay(1000);

  // Get user to place finger again
  LED_ON(LED_BLUE);
  p = 0;

  while(p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  p = -1;

  while(p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch(p) {
    case FINGERPRINT_OK:
      // Image taken
      break;
    case FINGERPRINT_NOFINGER:
      // Waiting for finger
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Communication error
      break;
    case FINGERPRINT_IMAGEFAIL:
      // Imaging error
      break;
    default:
      // Unknown error 
      break;
    }
  }

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      // Image converted
      break;
    case FINGERPRINT_IMAGEMESS:
      // Image too messy
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Communication error
      return p;
    case FINGERPRINT_FEATUREFAIL:
      // Could not find fingerprint features
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      // Could not find fingerprint features
      return p;
    default:
      // Unknown error 
      return p;
  }

  // Create fingerprint model
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    // Prints matched
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    // Communication error
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    // Fingerprints did not match
    return p;
  } else {
    // Unknown error
    return p;
  }

  // TODO: Do something better with ID
  int id = finger.getTemplateCount() + 1;
  if (id > 127) {
    LED_FATAL;
    // TODO: This is not good, enable deletion of fingerprints
  }
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    // Stored
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    // Communication error
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    // Could not store in that location
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    // Error writing to flash !uh oh!
    return p;
  } else {
    // Unknown error 
    return p;
  }

  LED_SUCCESS;
  return true;
}
uint8_t fingerprint_get_id(void) {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      // Image taken
      break;
    case FINGERPRINT_NOFINGER:
      // Waiting for finger
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Communication error
      return 0;
    case FINGERPRINT_IMAGEFAIL:
      // Imaging error
      return 0;
    default:
      // Unknown error 
      return 0;
  }

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      // Image converted
      break;
    case FINGERPRINT_IMAGEMESS:
      // Image too messy
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      // Communication error
      return 0;
    case FINGERPRINT_FEATUREFAIL:
      // Could not find fingerprint features
      return 0;
    case FINGERPRINT_INVALIDIMAGE:
      // Could not find fingerprint features
      return 0;
    default:
      // Unknown error 
      return 0;
  }

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    // Found a match!
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    // Communication error
    return 0;
  } else if (p == FINGERPRINT_NOTFOUND) {
    // Failed to find match
    return 0;
  } else {
    // Unknown error 
    return 0;
  }

  // found a match!
  int id = finger.fingerID;
  int confidence = finger.confidence;

  if (confidence > CONFIDENCE_THRESHOLD) {
    LED_SUCCESS;
    return id;
  } else {
    LED_ERROR;
    return -1;
  }
}

// Display helper function defintions
void display_blank(void) { 
  graphics.clear();
  graphics.set_pen(BLACK);
  graphics.rectangle(Rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT));
  display.update(&graphics);
}
void display_setting_up(void) {
  graphics.clear();
  graphics.set_pen(BLACK);
  graphics.rectangle(Rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT));

  for (int i = 0, n = (int)(devices.size()); i < n; i++) {
    graphics.set_pen(WHITE);
    graphics.text(devices[i].name, Point(25, 53 + (i * 30)), 200, 2);
    graphics.circle(Point(285, 63 + (i * 30)), 10);
    if (devices[i].status == SETUP_SUCCESS) {
      graphics.set_pen(GREEN);
      graphics.circle(Point(285, 63 + (i * 30)), 8);
    } else if (devices[i].status == SETUP_FAILED) {
      graphics.set_pen(RED);
      graphics.circle(Point(285, 63 + (i * 30)), 8);
    }
  }
  // 5 devices, wifi is the last
  // add an extra message underneath WIFI to say "Waiting for WiFi setup, see app!"
  display.update(&graphics);
}
void display_alert(const char* message, const char* submessage) {
  // displays circle in centre of screen with message above and submessage below
  graphics.clear();
  graphics.set_pen(BLACK);
  graphics.rectangle(Rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT));
  display.update(&graphics); 
  graphics.set_pen(PRIMARY);
  graphics.circle(Point(160, 120), 50);
  graphics.set_pen(WHITE);
  graphics.text(message, Point(20, 20), 300, 2);
  graphics.text(submessage, Point(20, 220), 300, 2);
  display.update(&graphics); 
}

void display_pulsing_circle(const char* message, int loops, int rgb) {
  display.set_backlight(255);
  
  Pen fade[255];
  Pen BACKGROUND;

  if (rgb == 0) {
    for (int i = 0; i < 255; i++) {
      fade[i] = graphics.create_pen(255, i, i);
    }
    BACKGROUND = graphics.create_pen(255, 165, 165);
  } else if (rgb == 1) {
    for (int i = 0; i < 255; i++) {
      fade[i] = graphics.create_pen(i, 255, i);
    }
    BACKGROUND = graphics.create_pen(165, 255, 165);
  } else { // sanity!
    for (int i = 0; i < 255; i++) {
      fade[i] = graphics.create_pen(i, 127 + uint8_t(i / 2), 255);
    }
    BACKGROUND = graphics.create_pen(165, 191, 255);
  }

  int32_t delta = 1; // Change in circle size for each iteration

  int32_t size1 = 0; // Initial size of circle 1
  int32_t size2 = 35; // Initial size of circle 2
  int32_t size3 = 70; // Initial size of circle 3
  int32_t size1_max = 35;
  int32_t size2_max = 70;
  int32_t size3_max = 105;
  
  // the 35 is a reasonable constant for 1 loop
  for (int i = 0, n = int(35 * (loops + 0.5f)); i < n; i++) {
    graphics.clear(); // Clear the graphics context
    graphics.set_pen(BACKGROUND);
    graphics.rectangle(Rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT));
    graphics.set_pen(fade[size3 + 60]); // Set color for circle 3
    graphics.circle(Point(160, 135), size3); // Draw circle 3
    graphics.set_pen(fade[size2 + 60]); // Set color for circle 2
    graphics.circle(Point(160, 135), size2); // Draw circle 2
    graphics.set_pen(fade[size1 + 60]); // Set color for circle 1
    graphics.circle(Point(160, 135), size1); // Draw circle 1
    graphics.set_pen(BLACK);
    graphics.text(message, Point(5, 10), 315, 3);
    display.update(&graphics); // Update the display
    size1 += delta; // Increase size of circle 1
    size2 += delta; // Increase size of circle 2
    size3 += delta; // Increase size of circle 3
    if (size1 >= size1_max) { // If circle 1 reaches max size
      size1 = 0; // Reset size of circle 1
    }
    if (size2 >= size2_max) { // If circle 2 reaches max size
      size2 = 35; // Reset size of circle 2
    }
    if (size3 >= size3_max) { // If circle 3 reaches max size
      size3 = 70; // Reset size of circle 3
    }
  }
  graphics.set_pen(BLACK);
  graphics.clear();
  display.set_backlight(0.0);
  display.update(&graphics); 
}

// Servo helper function definitions
void servo_unlock(void) {
  // Note that this is a blocking call, the bootsell button will not be checked until 
  // .. the servo has completed its movement!
  for (int pos = 0; pos <= 180; pos += 1) { // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    servo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(1);                       // waits 1ms for the servo to reach the position
  }    
  display_pulsing_circle("Unlocking...", 1, 1);
  for (int pos = 180; pos >= 0; pos -= 1) { // goes from 180 degrees to 0 degrees
    servo.write(pos);              // tell servo to go to position in variable 'pos'
    delay(1);                       // waits 1ms for the servo to reach the position
  }
}

// Wifi helper function definitions
char wifi_connect(char *name, char *pass) {
  // Connect to Wi-Fi
  WiFi.begin(name, pass);
  timeout = timeout > 30 ? timeout : 30; // 30 * 300 = 9 seconds, reasonable time

  while (WiFi.status() != WL_CONNECTED and timeout > 0) {
    // this is of course blocking!
    delay(300); 
    LED_SETUP;
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
  WiFi.softAP("Door Lock " + mac.substring(0, 6)); // leave password empty for open AP
  // .. as mentioned in PowerPoint we may change to WPA2-PSK later for security..
  server.begin();
  display_pulsing_circle("     Waiting for connection from app!", 3, 2);
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
  if (ssid == 0 || password == "\0") {
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
