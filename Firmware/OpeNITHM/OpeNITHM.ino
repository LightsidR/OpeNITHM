#include "PinConfig.h"

#ifdef USB
#include "USBOutput.h"
#else
#include "SerialOutput.h"
#endif

#include "AirSensor.h"
#include "AutoTouchboard.h"
#include "SerialLeds.h"
#include "SerialProcessor.h"
#include <FastLED.h>

SerialProcessor serialProcessor;

KeyState key_states[16];
CRGB leds[16];
byte serialBuffer[100];
bool updateLeds = false;
bool useSerialLeds = false;
int serialLightsCounter;

CRGB led_on = CRGB::Purple;
CRGB led_off = CRGB::Yellow;

float lightIntensity[16];
bool activated = true;

AutoTouchboard *touchboard;
AirSensor *sensor;
Output *output;
SerialLeds *serialLeds;

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE, RGBPIN, LED_ORDER>(leds, 16);

  // Flash LEDs orange 3 times
  for (int i = 0; i < 3; i++)
  {
    for (CRGB& led : leds)
      led = CRGB::Orange;
      
    FastLED.show();
    delay(1000);
    
    for (CRGB& led : leds)
      led = CRGB::Black;
      
    FastLED.show();
    delay(1000);
  }

  // Initialize and calibrate touch sensors
  touchboard = new AutoTouchboard();

  // Initialize the serial LED processor
  serialLeds = new SerialLeds();

  // Initialize air sensor
  // Digital mode calibrations in the constructor
  // Analog mode will automatically calibrate as it starts being read
  sensor = new AirSensor(500, 50);

  // Display the number of air sensors that were calibrated
  for (CRGB& led : leds)
    led = CRGB::Black;
    
  for (int i = 0; i < 6; i++)
  {
    if (sensor->getSensorCalibrated(i))
      leds[i] = CRGB::Green;
    else
      leds[i] = CRGB::Red;
  }
  
  FastLED.show();
  delay(3000);
  serialLightsCounter = 0;

  // Initialize relevant output method / USB or serial
#ifdef USB
  output = new USBOutput();
#else
  output = new SerialOutput();
#endif
}

void loop() {
  // Check for serial messages
  if (Serial.available() >= 100)
  {
    Serial.readBytes(serialBuffer, 100);
    serialProcessor.processBulk(serialBuffer);
  }
  else 
  {
    serialLightsCounter++;  
  }

  // If currently paused through a config command, do not execute main loop
  if (!activated) 
    return;

  // If we haven't received any serial light updates in 5 seconds, just fallback to reactive lighting
  if (serialLightsCounter > 300) 
    useSerialLeds = false;

  // Scan touch keyboard and update lights
  touchboard->scan();
  int index = 0;
  
  for (int i = 0; i < 16; i++)
  {
#ifndef LED_REVERSE
    index = i;
#else
    index = 15 - i;
#endif

    KeyState keyState = touchboard->update(i);

    // handle changing key colors for non-serial LED updates
    if (!useSerialLeds)
    {
      if (lightIntensity[index] > 0.05f)
        lightIntensity[index] -= 0.05f;
  
      // If the key is currently being held, set its color to the on color
      if (keyState == SINGLE_PRESS || keyState == DOUBLE_PRESS)
      {
        lightIntensity[index] = 1.0f;
        leds[index].setRGB(min(led_on.r / 2 + led_on.r / 2 * lightIntensity[index], 255), min(led_on.g / 2 + led_on.g / 2 * lightIntensity[index], 255), min(led_on.b / 2 + led_on.b / 2 * lightIntensity[index], 255));
      }
      else
      {
        // If not, make it the off color
        leds[index].setRGB(led_off.r / 2, led_off.g / 2, led_off.b / 2);
      }
      
      updateLeds = true;  
    }
    // handle changing key colors for serial LED updates
    else 
    {
      if (updateLeds)
      {
        RGBLed temp = serialLeds->getKey(i);
        leds[index].setRGB(temp.r, temp.g, temp.b);
      }
    }

#if !defined(SERIAL_PLOT) && defined(USB)
    if (key_states[i] != keyState)
      output->sendKeyEvent(i, keyState);
#endif

    key_states[i] = keyState;
  }

#ifdef SERIAL_PLOT
  if (PLOT_PIN == -1)
  {
    for (int i = 0; i < 16; i++)
    {
#ifdef SERIAL_RAW_VALUES
      // Print values
      Serial.print(touchboard->getRawValue(i));
#else
      // Print normalized values
      Serial.print(touchboard->getRawValue(i) - touchboard->getNeutralValue(i));
#endif
      Serial.print("\t");
    }
    Serial.println();
  }
  else
  {
    Serial.print(touchboard->getRawValue(PLOT_PIN));
    Serial.println();
  }
#endif

  // Process air sensor hand position
#if !defined(SERIAL_PLOT) && defined(USB)
#ifdef IR_SENSOR_KEY
    output->sendSensor(sensor->getSensorReadings());
#else
    output->sendSensorEvent(sensor->getHandPosition());
#endif
#endif

  // Send update
#if !defined(SERIAL_PLOT) && defined(USB)
  output->sendUpdate();
#endif

  //#if defined(SERIAL_PLOT)
  //  Serial.print("\t");
  //  Serial.println(sensor->getSensorReadings());
  //#endif

  // If the air sensor is calibrated, update lights. The lights will stay red as long as the air sensor is not calibrated.
  if (sensor->isCalibrated() && updateLeds)
  {
    FastLED.show();
    updateLeds = false;
  }
}
