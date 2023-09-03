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

homekit_service_t* hapservice = {0};

String pair_file_name = "/pair.dat";

void switch_callback(homekit_characteristic_t* ch, homekit_value_t value, void* context);

// Web server section
#define ENABLE_OTA  //if OTA need
#include "spiffs_webserver.h"

bool getSwitchVal() {
  if (hapservice) {
    homekit_characteristic_t* ch = homekit_service_characteristic_by_type(hapservice, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      return ch->value.bool_value;
    }
  }
  return false;
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

  // we will add only one light bulb service and keep a pointer for nest using
  hapservice = hap_add_switch_service("Switch", switch_callback, (void*)&relay_gpio_1);

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
  server.send(200, FPSTR(TEXT_PLAIN), getSwitchVal() ? "1" : "0");
}

void handleSetVal() {
  if (server.args() != 2) {
    server.send(505, FPSTR(TEXT_PLAIN), "Bad args");
    return;
  }

  const String varName = server.arg("var");
  const bool newValue = (server.arg("val") == "true");

  // Check which switch is being requested
  if (varName == "ch1" && hapservice) {
    set_switch(getSwitchVal(), newValue, relay_gpio_1, hapservice);
  } else if (varName == "ch2" && hapservice) {
    set_switch(getSwitchVal(), newValue, relay_gpio_2, hapservice);
  } else if (varName == "ch3" && hapservice) {
    set_switch(getSwitchVal(), newValue, relay_gpio_3, hapservice);
  } else if (varName == "ch4" && hapservice) {
    set_switch(getSwitchVal(), newValue, relay_gpio_4, hapservice);
  } else if (varName == "ch5" && hapservice) {
    set_switch(getSwitchVal(), newValue, relay_gpio_5, hapservice);
  } else if (varName == "ch6" && hapservice) {
    set_switch(getSwitchVal(), newValue, relay_gpio_6, hapservice);
  }
}

void loop() {
#ifdef ESP8266
  hap_homekit_loop();
#endif

  if (isWebserver_started)
    server.handleClient();
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
  if (fsDAT)
    fsDAT.readBytes(buf, size);
 
  hap_init_storage_ex(buf, size);
  if (fsDAT)
    fsDAT.close();
  delete []buf;
}

void storage_changed(char * szstorage, int bufsize) {
  SPIFFS.remove(pair_file_name);
  File fsDAT = SPIFFS.open(pair_file_name, "w+");
  if (!fsDAT) {
    Serial.println("Failed to open pair.dat");
    return;
  }
  fsDAT.write((uint8_t*)szstorage, bufsize);
  fsDAT.close();
}

// can be used for any logic, it will automatically inform Apple about state changes
void set_switch(bool currentVal, bool newVal, int gpioPin, homekit_service_t* hapservice) {
  Serial.println(String("set_switch:") + String(newVal ? "True" : "False")); 
  digitalWrite(gpioPin, newVal ? HIGH : LOW);
  
  if (hapservice) {
    Serial.println("notify hap"); 
    // getting on/off characteristic
    homekit_characteristic_t * ch = homekit_service_characteristic_by_type(hapservice, HOMEKIT_CHARACTERISTIC_ON);
    if (ch) {
      if (ch->value.bool_value != newVal) {  // will notify only if different
        ch->value.bool_value = newVal;
        homekit_characteristic_notify(ch, ch->value);
      }
    }
  }
}

void switch_callback(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
  Serial.println("switch_callback");
  set_switch(getSwitchVal(), ch->value.bool_value, *(int*)context, hapservice);
}

#ifdef ENABLE_WIFI_MANAGER
void startWiFiManager() {
  WiFiManager wifiManager;

  // Define your custom AP SSID and password here
  const char* customAPName = "RVS SmartHome Relays";
  const char* customAPPassword = "CustomAPPassword";

  // Start WiFiManager and configure Wi-Fi with your custom AP credentials
  if (!wifiManager.autoConnect(customAPName, customAPPassword)) {
    // Failed to connect or configure, handle as needed
    // You can restart or take other actions here
    ESP.restart();
    delay(1000);
  }
}
#endif // ENABLE_WIFI_MANAGER
