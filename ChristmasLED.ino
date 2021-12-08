/*
   Heyo!
   So this program uses an Arduino Xiao to light up an ARGB light string based on the WS2812b chip
   Important limitations:
   Current program uses ~14% of total program space (37820 / 262144 bytes)
   The XIAO can output ~500ma over its 5v VCC. That limits each pixel to ~30ma. This corresponds to a maximum TOTAL RGB value of 128.
   (Math: 500ma / 16 pixels = 31.25ma per pixel, round down to 30; 30ma per pixel / 3 ma per pixel = 10ma max per color; contd
   10ma per color / 20ma max per color = 50%; 50% of 256 = 128 RGB per color. This should include a margin of error, but try to keep it below 100.)
   There doesn't appear to be any way to configure true interrupts - more research needed, I'm still a beginner;
   In the mean time, in fading light modes, the sensor polling rate is limited by the iLoopTime. 
   The use of iTimer and iLoopTime and iFadeTime helps control how long fade effects take without slowing down the polling rate with delay()s scattered everywhere.

   Current light modes: 3
   0 = off
   1 = warm yellow, fixed
   2 = alternating red/green, fixed
   3 = alternating red/green, fading

   Future light modes to consider:
   - Warm yellow, candle flicker
   - Red/Green, moving (like fading, but with a 3rd intermediate color to give the illusion of linear motion)
   - Red/yellow/Green, fading, moving, fixed
   - Other holidays

*/

// Includes
#include <FastLED.h>

// Constants
#define cLedCount  16
#define cLedControlPin 6
#define cLiteSensorPin  7
#define cTouchSensorPin 8

// For debugging
// Set to 0 for standard operation
// Set to a non-zero positive integer to enable that light mode
// Set to a non-zero negative integer to turn off the device
#define debugMode 3

// Configuration parameters - Constant
#define iLiteMax 20 // Maximum RGB brightness per color
#define iLoopTime 5 // ms delay between main loops
#define iFadeTime 100 // ms between fade cycles
int iFadeLoops = iFadeTime / iLoopTime;

// Configuration parameters - Variable
int iLiteMode = 0; // Light mode (0=off)
int iCurr = iLiteMax; // Current RGB brightness, used for fading
int iTimer = 0; // Timer used for tracking loops during certain fade cycles
boolean bCurrentTouchValue = LOW;
boolean bLastTouchValue = LOW;
boolean bCount = LOW;

CRGB leds[cLedCount];

void setup() {
  // put your setup code here, to run once:
  if (debugMode > 0) {
    Serial.begin(9600);
    while (!Serial) {;}
  }
  pinMode(cLedControlPin, OUTPUT);
  pinMode(cLiteSensorPin, INPUT);
  pinMode(cTouchSensorPin, INPUT);
  FastLED.addLeds<WS2812, cLedControlPin, GRB>(leds, cLedCount);
}

void loop() {
  // put your main code here, to run repeatedly:

  // poll sensors
  int bCurrentTouchValue = digitalRead(cTouchSensorPin);
  int iLightValue = analogRead(cLiteSensorPin);
  float fLight = iLightValue * 0.0976; // convert absolute lite measurement to %?

  // ignore sensors if debugging
  if (debugMode > 0) {
    iLiteMode = debugMode;
    fLight = debugMode;
  } else if (debugMode < 0) {
    iLiteMode = 0;
    fLight = 100;
  }

  // Handle touch sensor events
  // This is "supposed" to mimic an onReleased event
  // Each touch increments the LiteMode up to 3 modes, then reverts to 0
  // LiteMode 0 = Off
  // Untested, still waiting for the touch sensor to arrive
  if (bCurrentTouchValue == HIGH && bLastTouchValue == LOW) {
    bLastTouchValue = HIGH;
  }
  if (bCurrentTouchValue == LOW && bLastTouchValue == HIGH) {
    bLastTouchValue == LOW;
    iLiteMode++;
    if (iLiteMode > 3) {
      iLiteMode = 0;
    }
  }
  
  // Control code
  // If ambient light is low, we turn the thing on based on iLiteMode
  if (fLight < 50) {
    statusOn();
    switch (iLiteMode) {
      case 0: 
        ledModeOff();
        break;
      case 1:
        ledMode1();
        break;
      case 2: 
        ledMode2();
        break;
      case 3:
        ledMode3();
        break;
    }
  } else {
    statusOff();
    ledModeOff();
  }
  iTimer++;
  delay(iLoopTime); // Main loop will run every iLoopTime
}

// blinks the built-in LED to indicate the program is running
void statusOn() {
  //ledBlink(1500,250);
  digitalWrite(LED_BUILTIN, LOW);
}

// blinks the built-in LED to indicate the program is idle
void statusOff() {
  //ledBlink(250,1500);
  digitalWrite(LED_BUILTIN, HIGH);
}

// blinks the built-in LED based on onDelay and offDelay
void ledBlink(int onDelay, int offDelay) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(onDelay);
  digitalWrite(LED_BUILTIN, LOW);
  delay(offDelay);
}

// Just a test routine to turn on the first addressible LED
void ledTest() {
  leds[0] = CRGB(20, 20, 20);
  FastLED.show();
}

// lites off
void ledModeOff() {
  for (int i = 0; i < cLedCount; i++) {
    leds[i] = CRGB(0, 0, 0);
  }
  FastLED.show();
}

// all on, comfy yellow
void ledMode1() {
  for (int i = 0; i < cLedCount; i++) {
    leds[i] = CRGB(iLiteMax, iLiteMax / 2, 0);
  }
  FastLED.show();
}

// Christmas, red and green alternating, static
void ledMode2() {
  for (int i = 0; i < cLedCount; i++) {
    if (i % 2 == 0) {
      leds[i] = CRGB(iLiteMax, 0, 0);
    } else {
      leds[i] = CRGB(0, iLiteMax, 0);
    }
  }
  FastLED.show();
}

// Christmas, red and green alternating, fade
void ledMode3() {
  // Run this function only once every iSlope number of loops
  // Still working on a way to slow the fade rate at the min/max of the loop
  if (iTimer % iFadeLoops == 0) {
    // Set the color of each LED in the string
    for (int i = 0; i < cLedCount; i++) {
      if (i % 2 == 0) {
        leds[i] = CRGB(iCurr, abs(iLiteMax - iCurr), 0);
      } else {
        leds[i] = CRGB(abs(iLiteMax - iCurr), iCurr, 0);
      }
    }
    FastLED.show();

    // Math the colors to set the LEDs to on the next loop
    if (bCount == LOW) {
      iCurr--;
      if (iCurr == iLiteMax / 2) {
        iCurr--;
      }
      if (iCurr < 1) {
        bCount = HIGH;
      }
    } else {
      iCurr++;
      if (iCurr == iLiteMax / 2) {
        iCurr++;
      }
      if (iCurr >= iLiteMax) {
          bCount = LOW;
      }
    }
  }
}

// alternative fading math
// sourced from arduino.stackexchange.com
// https://arduino.stackexchange.com/questions/18574/fastled-fade-ws2812b
/*
  //start with full red
  leds[0].red = 255;

  //slowly add green
  for( int i=0; i<256; i++ )
  {
  leds[0].green = i;
  delay(10);
  }

  // slowly subtract red
  for( int i=255; i>=0; i-- )
  {
  leds[0].red = i;
  delay(10);
  }
*/
