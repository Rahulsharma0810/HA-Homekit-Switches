/// BASIC CONFIGURATION 
#define ENABLE_WIFI_MANAGER  // if we want to have built-in wifi configuration
                             // Otherwise direct connect ssid and pwd will be used
                             // for Wifi manager need extra library //https://github.com/tzapu/WiFiManager

#define ENABLE_WEB_SERVER    //if we want to have built-in web server /site
#define ENABLE_OTA  //if Over the air update need  , ENABLE_WEB_SERVER must be defined first
#include <Arduino.h>

#ifdef ESP32
#include <SPIFFS.h>
#endif

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include "coredecls.h"
#endif

#ifdef ENABLE_WEB_SERVER
#ifdef ESP8266
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#endif

#ifdef ESP32
#include <WebServer.h>
WebServer server(80);
#endif
#endif 

#if defined(ESP32) && defined(ENABLE_OTA)
#include <Update.h>
#endif

#ifdef ENABLE_WEB_SERVER
#include "spiffs_webserver.h"
bool isWebserver_started = false;
#endif 

const int relay_gpio_1 = 15;
const int relay_gpio_2 = 2;
const int relay_gpio_3 = 4;
const int relay_gpio_4 = 18;
const int relay_gpio_5 = 19;
const int relay_gpio_6 = 21;

#ifdef ENABLE_WIFI_MANAGER
#include <WiFiManager.h>        // https://github.com/tzapu/WiFiManager
#endif

const char* HOSTNAME = "ESpHap";

extern "C" {
#include "homeintegration.h"
}

#ifdef ESP8266
#include "homekitintegrationcpp.h"
#endif

homekit_service_t* hapservice_1 = {0};
homekit_service_t* hapservice_2 = {0};
homekit_service_t* hapservice_3 = {0};
homekit_service_t* hapservice_4 = {0};
homekit_service_t* hapservice_5 = {0};
homekit_service_t* hapservice_6 = {0};

String pair_file_name = "/pair.dat";

bool switchState_1 = false;
bool switchState_2 = false;
bool switchState_3 = false;
bool switchState_4 = false;
bool switchState_5 = false;
bool switchState_6 = false;

const char* stateFileName = "/switch_state.dat";

void set_switch(homekit_characteristic_t* ch, bool new_value, int gpio_pin);

// Web server section
#define ENABLE_OTA  //if OTA need
#include "spiffs_webserver.h"

void saveSwitchStates() {
  File file = SPIFFS.open(stateFileName, "w");
  if (!file) {
    Serial.println("Failed to open state file for writing");
    return;
  }

  file.write((uint8_t*)&switchState_1, sizeof(switchState_1));
  file.write((uint8_t*)&switchState_2, sizeof(switchState_2));
  file.write((uint8_t*)&switchState_3, sizeof(switchState_3));
  file.write((uint8_t*)&switchState_4, sizeof(switchState_4));
  file.write((uint8_t*)&switchState_5, sizeof(switchState_5));
  file.write((uint8_t*)&switchState_6, sizeof(switchState_6));

  file.close();
}

void loadSwitchStates() {
  File file = SPIFFS.open(stateFileName, "r");
  if (!file) {
    Serial.println("No state file found, using default states");
    return;
  }

  file.readBytes((char*)&switchState_1, sizeof(switchState_1));
  file.readBytes((char*)&switchState_2, sizeof(switchState_2));
  file.readBytes((char*)&switchState_3, sizeof(switchState_3));
  file.readBytes((char*)&switchState_4, sizeof(switchState_4));
  file.readBytes((char*)&switchState_5, sizeof(switchState_5));
  file.readBytes((char*)&switchState_6, sizeof(switchState_6));

  file.close();
}

bool getSwitchState(int gpio_pin) {
  switch (gpio_pin) {
    case relay_gpio_1:
      return switchState_1;
    case relay_gpio_2:
      return switchState_2;
    case relay_gpio_3:
      return switchState_3;
    case relay_gpio_4:
      return switchState_4;
    case relay_gpio_5:
      return switchState_5;
    case relay_gpio_6:
      return switchState_6;
    default:
      return false;
  }
}

void setup() {
#ifdef ESP8266 
  disable_extra4k_at_link_time();
#endif 
  Serial.begin(115200);
  delay(10);

#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    // Serial.print("SPIFFS Mount failed");
  }
#endif
#ifdef ESP8266
  if (!SPIFFS.begin()) {
    Serial.print("SPIFFS Mount failed");
  }
#endif

  Serial.print("Free heap: ");
  // Serial.println(system_get_free_heap_size());

  pinMode(relay_gpio_1, OUTPUT);
  pinMode(relay_gpio_2, OUTPUT);
  pinMode(relay_gpio_3, OUTPUT);
  pinMode(relay_gpio_4, OUTPUT);
  pinMode(relay_gpio_5, OUTPUT);
  pinMode(relay_gpio_6, OUTPUT);

  init_hap_storage();

  set_callback_storage_change(storage_changed);

  /// We will use for this example only one accessory (possible to use a several on the same esp)
  // Our accessory type is light bulb, apple interface will properly show that
  hap_setbase_accessorytype(homekit_accessory_category_switch);
  /// init base properties
  hap_initbase_accessory_service("ST Smart Switches", "6 Switch", "0", "EspHapSwitch", "1.0");

  // We will add six switch services and keep pointers for later use
  hapservice_1 = hap_add_switch_service("Switch 1", switch_callback, (void*)&relay_gpio_1);
  hapservice_2 = hap_add_switch_service("Switch 2", switch_callback, (void*)&relay_gpio_2);
  hapservice_3 = hap_add_switch_service("Switch 3", switch_callback, (void*)&relay_gpio_3);
  hapservice_4 = hap_add_switch_service("Switch 4", switch_callback, (void*)&relay_gpio_4);
  hapservice_5 = hap_add_switch_service("Switch 5", switch_callback, (void*)&relay_gpio_5);
  hapservice_6 = hap_add_switch_service("Switch 6", switch_callback, (void*)&relay_gpio_6);

#ifdef ENABLE_WIFI_MANAGER   
  startWiFiManager();
#else

  WiFi.mode(WIFI_STA);
  WiFi.begin((char*)ssid, (char*)password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

#endif
  Serial.println(PSTR("WiFi connected"));
  Serial.println(PSTR("IP address: "));
  Serial.println(WiFi.localIP());

  // Load the previous switch states
  loadSwitchStates();

  hap_init_homekit_server();   

#ifdef ENABLE_WEB_SERVER
  String strIp = String(WiFi.localIP()[0]) + String(".") + String(WiFi.localIP()[1]) + String(".") + String(WiFi.localIP()[2]) + String(".") + String(WiFi.localIP()[3]);    
#ifdef ESP8266
  if (hap_homekit_is_paired()) {
#endif
    Serial.println(PSTR("Setting web server"));
    SETUP_FILEHANDLES
    server.on("/get", handleGetVal);
    server.on("/set", handleSetVal);   
    server.begin(); 
    Serial.println(String("Web site http://") + strIp);  
    Serial.println(String("File system http://") + strIp + String("/browse")); 
    Serial.println(String("Update http://") + strIp + String("/update"));     
    isWebserver_started = true;
#ifdef ESP8266
  } else
    Serial.println(PSTR("Web server is NOT SET, waiting for pairing"));
#endif

#endif
}

void handleGetVal() {
  server.send(200, FPSTR(TEXT_PLAIN), getSwitchState(relay_gpio_1) ? "1" : "0");
}

void handleSetVal() {
  if (server.args() != 2) {
    server.send(505, FPSTR(TEXT_PLAIN), "Bad args");
    return;
  }

  const String varName = server.arg("var");
  const bool newValue = (server.arg("val") == "true");

  // Check which switch is being requested
  if (varName == "ch1" && hapservice_1) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice_1, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      set_switch(ch, newValue, relay_gpio_1);
    }
  } else if (varName == "ch2" && hapservice_2) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice_2, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      set_switch(ch, newValue, relay_gpio_2);
    }
  } else if (varName == "ch3" && hapservice_3) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice_3, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      set_switch(ch, newValue, relay_gpio_3);
    }
  } else if (varName == "ch4" && hapservice_4) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice_4, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      set_switch(ch, newValue, relay_gpio_4);
    }
  } else if (varName == "ch5" && hapservice_5) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice_5, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      set_switch(ch, newValue, relay_gpio_5);
    }
  } else if (varName == "ch6" && hapservice_6) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice_6, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      set_switch(ch, newValue, relay_gpio_6);
    }
  }
}

void loop() {
#ifdef ESP8266
  hap_homekit_loop();
#endif

  if (isWebserver_started) {
    server.handleClient();
  }
}

void init_hap_storage() {
  Serial.print("init_hap_storage");
    
  File fsDAT = SPIFFS.open(pair_file_name, "r");
  
  if (!fsDAT) {
    Serial.println("Failed to read pair.dat");
    SPIFFS.format(); 
  }
  
  int size = hap_get_storage_size_ex();
  char* buf = new char[size];
  memset(buf, 0xff, size);
  if (fsDAT) {
    fsDAT.readBytes(buf, size);
  }

  hap_init_storage_ex(buf, size);
  if (fsDAT) {
    fsDAT.close();
  }
  delete []buf;
}

void storage_changed(char* szstorage, int bufsize) {
  SPIFFS.remove(pair_file_name);
  File fsDAT = SPIFFS.open(pair_file_name, "w+");
  if (!fsDAT) {
    Serial.println("Failed to open pair.dat");
    return;
  }
  fsDAT.write((uint8_t*)szstorage, bufsize);
  fsDAT.close();
}

// Function to update switch state and save it
void set_switch(homekit_characteristic_t* ch, bool new_value, int gpio_pin) {
  Serial.println(String("set_switch:") + String(new_value ? "True" : "False")); 
  digitalWrite(gpio_pin, new_value ? HIGH : LOW);
  
  if (ch) {
    Serial.println("notify hap"); 
    // Getting on/off characteristic
    if (ch->value.bool_value != new_value) {  // Notify only if different
      ch->value.bool_value = new_value;
      homekit_characteristic_notify(ch, ch->value);
    }
  }

  // Update the switch state variable
  switch (gpio_pin) {
    case relay_gpio_1:
      switchState_1 = new_value;
      break;
    case relay_gpio_2:
      switchState_2 = new_value;
      break;
    case relay_gpio_3:
      switchState_3 = new_value;
      break;
    case relay_gpio_4:
      switchState_4 = new_value;
      break;
    case relay_gpio_5:
      switchState_5 = new_value;
      break;
    case relay_gpio_6:
      switchState_6 = new_value;
      break;
  }

  // Save the switch states to a file
  saveSwitchStates();
}

void switch_callback(homekit_characteristic_t* ch, homekit_value_t value, void* context) {
  int* gpio_pin = (int*)context;
  Serial.println("switch_callback");
  set_switch(ch, value.bool_value, *gpio_pin);
}

#ifdef ENABLE_WIFI_MANAGER
void startWiFiManager() {
  WiFiManager wifiManager;

  // Define your custom AP SSID and password here
  const char* customAPName = "RVS SmartHome Relays";
  const char* customAPPassword = "NoInternet";

  // Start WiFiManager and configure Wi-Fi with your custom AP credentials
  if (!wifiManager.autoConnect(customAPName, customAPPassword)) {
    // Failed to connect or configure, handle as needed
    // You can restart or take other actions here
    ESP.restart();
    delay(1000);
  }
}
#endif // ENABLE_WIFI_MANAGER
