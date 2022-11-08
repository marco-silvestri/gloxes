#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <ESPAsyncWebServer.h>
#include "fauxmoESP.h"
#include <Adafruit_NeoPixel.h>

#define WIFI_SSID "your ssid"
#define WIFI_PASS "your password"

fauxmoESP fauxmo;
AsyncWebServer server(80);

// -----------------------------------------------------------------------------
#define SERIAL_BAUDRATE               115200
//declare switching pins
#define RGBCTL                        D4 //control pin

int numPixels = 39;
int i; // i=1 warm, i=2 white, i=3 pink
int brightness = 5;


// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(numPixels, RGBCTL, NEO_GRB + NEO_KHZ800);

// -----------------------------------------------------------------------------
// Wifi
// -----------------------------------------------------------------------------

void wifiSetup()
{
  WiFi.mode(WIFI_STA);
  Serial.printf("[WIFI] Connecting to %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Connected!
  Serial.printf("[WIFI] STATION Mode, SSID: %s, IP address: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

}

void serverSetup()
{
  // Custom entry point (not required by the library, here just as an example)
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "Hello, world");
  });

  // These two callbacks are required for gen1 and gen3 compatibility
  server.onRequestBody([](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data))) return;
    // Handle any other body request here...
  });

  server.onNotFound([](AsyncWebServerRequest * request) {
    String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
    if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body)) return;
    // Handle not found request here...
  });

  // Start the server
  server.begin();
}

void setup()
{
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println();
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT);
  wifiSetup();
  serverSetup();

  // Set fauxmoESP to not create an internal TCP server and redirect requests to the server on the defined port
  // The TCP port must be 80 for gen3 devices (default is 1901)
  // This has to be done before the call to enable()
  fauxmo.createServer(false);
  fauxmo.setPort(80); // This is required for gen3 devices

  // You have to call enable(true) once you have a WiFi connection
  // You can enable or disable the library at any moment
  // Disabling it will prevent the devices from being discovered and switched
  fauxmo.enable(true);

  // You can use different ways to invoke alexa to modify the devices state:
  // "Alexa, turn kitchen on" ("kitchen" is the name of the first device below)
  // "Alexa, turn on kitchen"
  // "Alexa, set kitchen to fifty" (50 means 50% of brightness)

  // Add virtual devices

  fauxmo.addDevice("Gloxes");

  strip.begin();
  strip.show();
  strip.setBrightness(5);
  i = 0;

  digitalWrite(LED_BUILTIN, HIGH); //switch off the on board led

  // You can add more devices
  //fauxmo.addDevice("light 3");
  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
    // Callback when a command from Alexa is received.
    // You can use device_id or device_name to choose the element to perform an action onto (relay, LED,...)
    // State is a boolean (ON/OFF) and value a number from 0 to 255 (if you say "set kitchen light to 50%" you will receive a 128 here).
    // Just remember not to delay too much here, this is a callback, exit as soon as possible.
    // If you have to do something more involved here set a flag and process it in your main loop.
    // if (0 == device_id) digitalWrite(RELAY1_PIN, state);
    // if (1 == device_id) digitalWrite(RELAY2_PIN, state);
    // if (2 == device_id) analogWrite(LED1_PIN, value);
    Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
    // For the example we are turning the same LED on and off regardless fo the device triggered or the value
    //------------------------------------------------------------------------------
    //Switching action on detection of device name
    //------------------------------------------------------------------------------

    if ( (strcmp(device_name, "Gloxes") == 0) ) {
      Serial.println("You said Gloxes");
      if (state) {
        i = 1;
        brightness = value;
        Serial.println("Brightness:");
        Serial.println(brightness);
      }
      else {
        allOff();
        i = 0;
      }
    }
  });
}

void loop() {

  // fauxmoESP uses an async TCP server but a sync UDP server
  // Therefore, we have to manually poll for UDP packets
  fauxmo.handle();
  startShow(i);

  // This is a sample code to output free heap every 5 seconds
  // This is a cheap way to detect memory leaks
  static unsigned long last = millis();
  if (millis() - last > 5000) {
    last = millis();
    //Serial.printf("[MAIN] Free heap: %d bytes\n", ESP.getFreeHeap());
    ESP.getFreeHeap();
  }

}

//Colors
void startShow(int i)
{
  switch (i)
  {
    case 0: colorWipe(strip.Color(0, 0, 0), 50);          // off
      Serial.println("I'm off");
      break;
    case 1: colorWipe(strip.Color(250, 220, 50), 50);        // on
      Serial.println("I'm on"); 
      break;
  }
}

// switch on
void switchOn(uint32_t c, uint8_t wait)
{
  if (brightness > 254) brightness = 254;
  strip.setBrightness(brightness);
  for (uint16_t i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, c);
    strip.show();
    //delay(wait);
  }
}

// on led at a time
void colorWipe(uint32_t c, uint8_t wait)
{
  if (brightness > 254) brightness = 254;
  strip.setBrightness(brightness);
  for (uint16_t i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

// all off
void allOff()
{
  for ( int i = strip.numPixels(); i == 0; i--)
  {
    strip.setPixelColor(i, 0, 0, 0 );
  }
  strip.show();
}
