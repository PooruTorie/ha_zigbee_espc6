#ifndef ZIGBEE_MODE_ED
#error "Zigbee coordinator/router mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

// Zigbee dimmable light and range extender configuration
#define ZIGBEE_LIGHT_ENDPOINT 10

uint8_t led = D2;
uint8_t button = BOOT_PIN;

// ESP32 PWM dimming settings
constexpr int PWM_FREQ = 500;
constexpr int PWM_RESOLUTION = 16;

// variables for dimming
int maxLevelTransformed = pow(2, PWM_RESOLUTION)-1; // internal max brightness level
int levelTransformed;                               // zigbee brightness value that got transformed into internal brightness level
int lastLevelTransformed;                           // internal brightness level of last brightness change

// create endpoints
ZigbeeDimmableLight zbDimmableLight = ZigbeeDimmableLight(ZIGBEE_LIGHT_ENDPOINT);


/********************* LED functions **************************/
// function to transform zigbee brightness values (0-254) to the values fitting the PWM_RESOLUTION with gamma correction
int transformBrightness(int value) {
    float gamma = 2.2;
    return (int) (pow(value / 255.0, gamma) * maxLevelTransformed);
}

void setLight(bool state, uint8_t level) {

  // transform zigbee brightness level (0-254) to internal level
  levelTransformed = transformBrightness(level);

  // ignore brightness changes between two equal values
  if(levelTransformed == lastLevelTransformed) return;

  // change brightness
  if (!state) {
    ledcFade(led, lastLevelTransformed, 0, 100); // smooth zigbee brightness changes happen every 100 ms
    return;
  }
  ledcFade(led, lastLevelTransformed, levelTransformed, 100); // smooth zigbee brightness changes happen every 100 ms

  // log brightness changes
  Serial.print("[Brightness] Zigbee: ");
  Serial.print(level);
  Serial.print(" | Internal before: ");
  Serial.print(lastLevelTransformed);
  Serial.print(" | Internal target: ");
  Serial.print(levelTransformed);
  Serial.print(" / ");
  Serial.println(maxLevelTransformed);

  // remember internal level for next brightness change for smooth zigbee brightness changes
  lastLevelTransformed = levelTransformed;
}

// identify function - 100% example code
void identify(uint16_t time) {
  static uint8_t blink = 1;
  log_d("Identify called for %d seconds", time);
  if (time == 0) {
    // If identify time is 0, stop blinking and restore light as it was used for identify
    zbDimmableLight.restoreLight();
    return;
  }
  ledcWrite(led, maxLevelTransformed * blink);
  blink = !blink;
}

/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Init LED and turn it off
  ledcAttach(led, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(led, 0);

  // Init button for factory reset
  pinMode(button, INPUT_PULLUP);

  // Set Zigbee device name and model
  zbDimmableLight.setManufacturerAndModel("ESP32C6", "ZigbeeDimmer");

  // Optional: Set callback function for device identify
  zbDimmableLight.onIdentify(identify);

  // Set power source
  zbDimmableLight.setPowerSource(ZB_POWER_SOURCE_MAINS);

  // Set callback function for light change
  zbDimmableLight.onLightChange(setLight);

  // Add endpoints to Zigbee Core
  Serial.println("Adding ZigbeeLight endpoint to Zigbee Core");
  Zigbee.addEndpoint(&zbDimmableLight);

// start Zigbee in Router mode with custom or default configuration
    // use default Zigbee configuration for Zigbee Extender
    // When all EPs are registered, start Zigbee as ROUTER device
  if (!Zigbee.begin(ZIGBEE_END_DEVICE)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }
  // wait for connection an blink LED until connection established
  Serial.println("Connecting to network");
  bool ledOn = false;
  while (!Zigbee.connected()) {
    Serial.print(".");
    if (ledOn) {
      ledcWrite(led, 0);
    } else {
      ledcWrite(led, static_cast<int>(maxLevelTransformed * 0.1));
    }
    ledOn = !ledOn;
    delay(500);
  }
  Serial.println();
  Serial.println("Connected");
  Serial.println();
  ledcWrite(led, 0);
}

void loop() {
  // Checking button for factory reset
  if (digitalRead(button) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    int startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        // If key pressed for more than 3secs, factory reset Zigbee and reboot
        Serial.println("Resetting Zigbee to factory and rebooting in 1s.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    // Increase blightness by 50 every time the button is pressed
    zbDimmableLight.setLightState(!zbDimmableLight.getLightState());
  }
  delay(100);
}