/*
  Heyo!
   So this program uses an Arduino Xiao to light up an ARGB light string based on the WS2812b chip
   Licensed by MIT license. I appreciate attribution if you use this code publicly. But it's not like I can stop you.

  Requirements
    - An Arduino compatible board (built on a Seeeduino XIAO )
    - TTP223B capacitive or compatible touch sensor
    - TEMT6000 or compatible light sensor
    - WS2812b or compatible Addressable RGB LEDs
    - 1000uf 25v capacitor (anything over 500uf 6v should work)
    - FastLED v3.4 arduino library (available in Arduino Library Manager)
    - FlashStorage v1.0 arduino library (available in Arduino Library Manager)    

  Wiring
    - XIAO VCC -> ARGB VCC, capacitor+
    - XIAO GND -> ARGB GND, capacitor-, TTP223B GND, TEMT6000 GND
    - XIAO +3v -> TTP223B VCC, TEMT6000 VCC
    - XIAO pin9 -> ARGB Data 
    - XIAO pin8 -> TTP223B Data
    - XIAO pin7 -> TEMT6000 Data
    Notes:
      The capacitor is recommended for smoothing power output to the LEDs when the power supply can't keep up with rapid changes in demand. Since this
        project runs directly from the XIAO's +5v, the cap keeps rapid LED changes from killing the CPU. 
      A resistor on the data line is also recommended for preventing voltage spikes that can kill your LEDs. However since the XIAO runs on 3.3v it's 
        barely enough to trigger the data rail on the WS2812b anyway. I skipped the resistor. You do you.
      A breadboard or prototype board for breakout can help keep wires from getting too messy. Soldering all those GND wires to that tiny GND pin on the 
        XIAO is a tight fit. 
      Both the TEMT6000 and the TTP223B can run from 3.3 or 5v. I noticed when running the touch sensor on 5v that touch events would reset the CPU.
        It also had "bounce" effects that had me writing dumb debounce timers to keep from registering duplicate events.
        I had a devil of a time debugging this, thinking it was a problem with Arduino interrupts. But these problems recurred with polling every loop().
        I found that running it on 3.3v resolves these issues.
        I suspect the sensor was overdrawing current on the +5v rail and killing the CPU. Presumably this means I *could* go back
        to using interrupts for touch events, but polling is working and performance isn't terrible so why fix what ain't broke.

  Important limitations:
    - Current program uses ~14% of total available program space
    - The XIAO can output ~500ma over its 5v VCC. That limits each pixel to ~30ma. 
      This corresponds to a maximum TOTAL RGB value of 128.
      (Math: 500ma / 16 pixels = 31.25ma per pixel, round down to 30; 30ma per pixel / 3 ma per pixel = 10ma max per color; 
      10ma per color / 20ma max per color = 50%; 50% of 256 = 128 RGB per color. 
      This should include a margin of error, but try to keep it below 100.)
    - The light sensor works but is not calibrated. Bench testing suggests an optimal value somewhere around 200-300.

  Explanation of function:
    - This sketch works by using iTimer to track the passage of milliseconds.
    - We set iFadeTime to the number of ms we want between each fade step in a "fade" effect.
    - Each loop(), we check the current value of iLiteMode and run the corresponding ledModeX() function.
    - Each loop(), every 1 ms, we increment iTimer.
    - Within each ledModeX() function is an if statement that mods iTimer over iFadeTime so the subsequent commands only run every iFadeTime number
      of milliseconds.
      For example: 
        - Set iFadeTime to 100ms
        - Every loop() iTimer updates, tracking ms passed
        - ledModeX() runs every loop()
        - if (iTimer % iFadeTime == 0) { runs every 100ms}
    - Each ledModeX() function calls a modeSetup() function and sets iFadeTime based on the best setting for that lite effect
    - Each ledModeX() function has an explanation of how it works
    - The iRed/iGreen/iBlue variables are used mostly for fading effects and are set to bytes. 
        - This ensures they max at 255 and roll back over to 0. 
        - We can pass the iRed/G/B values to a sin function, in this case I chose the FastLED function cubicwave8, 
          and get a value that smoothly fades up and down.
        - I chose the cubicwave8 function because it has wide peaks, so pixels sit at the "destination" color longer and spend less time in the
          less pretty transition colors.
        - cubicwave8 outputs a value 0-255. We map this to a range from 0 to i*Max for each color,
          allowing us to centrally manage maximum pixel brightness.
        - The result is a smooth fading transition across any color we want on any pixel. I'm sure there are more elegant solutions out there,
          but I've not found them and this was fun.

  Errata
    - Touch sensors work, but you may want to customize them.
      - Using interrupts gets you fast response when you tap but I didn't like the way it halted the program
      - Polling doesn't seem to have a significant performance impact
    - Touch sensor events are based on "onRelease" type events. This prevents long presses from causing multiple shorter-press events.
    - I use #DEFINE for a lot of constants. Yeah I know the recommendation is to use const instead, but this saves on memory and all my variables have unique n
    - I use RGB instead of HSV cause after testing it was smoother. HSV had noticeable "stepping" and uggly shifting at the low brightness I'm working at.
    - There's still some old code in here from various ideas I toyed with. This ain't clean by any stretech.

  Current light modes: 10
    mode 0 - off
    mode 1 - comfy yellow, static on
    mode 2 - comfy yellow, flicker (candle)
    mode 3 - comfy yellow, randomize
    mode 4 - Christmas, red and green, 3 pixel, chasing
    mode 5 - Christmas, RGB, 1-2/2-3/1-3 pixel, fading
    mode 6 - Rainbow, 3 colors, all pixels, fading
    mode 7 - Rainbow, 3 colors, 3 pixels, fading
    mode 8 - Blue/White, 2 colors, 3 pixels, chasing
    mode 9 - Blue/Purple, 2 colors, 3 pixels, chasing
    mode 10 - Demo mode. Rotate through all modes every 10 seconds

  Future light modes to consider:
   x Warm yellow, candle flicker
   x Red/Green, moving (like fading, but with a 3rd intermediate color to give the illusion of linear motion)
   - Red/yellow/Green, fading, moving, fixed
   - Other holidays
   - Fireflies

  To-Do
   x Additional light modes
   x Improved fading (longer pause between fade steps at min/max of each color, so they stay at red/green longer than the intermediate yellow)
   x Change iLiteMax and adjust lighting modes to use iLiteMax/2 or /3 to keep the luminosity more even
   x Long press on touch sensor for switching the unit full off or enabling demo mode iLiteMode(10)
   x Save the current lite mode to nonvolatile memory to restore prior settings on startup
   - Move the setup() calls for each lite effect to inside the loop function, for more easily adjusting that effect's parameters
   - Second touch sensor for controlling color combinations and fade effects separately (mix and match!)
   - Make chasing code more efficient - that's a LOT of function calls to make them work.
*/

// Includes
#include <FastLED.h> // Interface for controlling LEDs
#include <FlashStorage.h> // Interface for using onboard flash as eeprom on devices that don't have it

// For debugging
// Set to NULL for normal operation
// Set to 0 to turn off
// Set to a non-zero positive integer to enable that light mode
// Set to -1 to sequently test each LED at RGB(20)
// Side note - NULL throws compiler warnings; sue me, this is my first time
#define debugMode NULL

// Constants
#define cLedCount  16 // Number of LEDs in your string
#define cLedControlPin 9 // Arduino pin assigned to ARGB Data in
#define cTouchSensorPin 8 // Arduino pin assigned to touch sensor Data
#define cLightSensorPin  7 // Arduino pin assigned to Light sensor Data

// Configuration parameters - Constant
#define cLightSwitch 255 // the ambient light value above which the device is off, and below which it is on
#define iLiteMax 60 // Maximum RGB brightness per pixel, don't go over 100 per pixel for a 16 pixel string without a dedicated LED 5v power supply
#define iTotalLiteModes 9 // Number of LED lite modes (does not include "Off" or "Demo")
#define iPollingInterval 2 // number of ms for debouncing the touch input, value of 1 will always return true and run every loop()
#define iShortPressTime 1500 // max number of ms for a "short press" event aka "tap"
#define iLongPressTime 3000 // min number of ms for a "long press" event aka "5 seconds"
#define iDemoPressTime 7000 // min number of ms for a "long press" to enter demo mode aka "10 seconds"
#define iDemoModeTime 10000 // number of ms to run each light mode for in Demo Mode

// Configuration parameters - Variable
volatile short iLiteMode = 1; // LED mode (0=off), volatile for interrupts
volatile boolean bModeSetup = 0; // Used to set up each lighting mode, volatile for interrupts
unsigned long iPressTime = 0; // used for calculating short vs long button press
unsigned long iReleaseTime = 0; // same
unsigned long iUptime = 0; // Used to calculate uptime and manage the iTimer
unsigned short iFadeTime = 100; // ms between fade cycles, values of 1 will always return true and run every loop()
unsigned short iLightValue = 0; // value of ambient light read from sensor
unsigned short iTimer = 0; // Timer used with iFadeTime to regulate LED effects
byte iCycle = 0; // Used for rotating between modes (ledMode10), and debugModes
byte iRedMax, iGreenMax, iBlueMax = 0; // For fading
byte iRed, iGreen, iBlue = 0; // For fading
boolean bCurrentTouchValue = LOW; // For reading touch sensor output
boolean bLastTouchValue = LOW; // For calculating short vs long touch sensor events

FlashStorage(diLiteMode, int); // Initialize "Flash Storage" location
CRGB leds[cLedCount];

void setup() {
  // enable serial output when debugging
  if (debugMode) {
    Serial.begin(9600);
  }
  
  // set pin modes
  pinMode(cLedControlPin, OUTPUT);
  pinMode(cLightSensorPin, INPUT);
  pinMode(cTouchSensorPin, INPUT);
  
  // initialize fastLED
  FastLED.addLeds<WS2812, cLedControlPin, GRB>(leds, cLedCount);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 400); // try to keep LED power utilization below 400ma
  
  // initialize interrupts
  // Deprecated in favor of polling, but leaving in for anyone who wants to play with it
  //attachInterrupt(digitalPinToInterrupt(cTouchSensorPin), touchReleaseEvent, FALLING); // react to an "onRelease" event from the touch sensor

  // Pull last stored litemode if available
  if (!debugMode) {iLiteMode = diLiteMode.read();}
}

void loop() {
  // Set iTimer
  // The millis() function just returns uptime in ms
  // By testing it for any increase in value, this should
  // run every 1ms.
  // Combined with iFadeTime, this is used to regulate the speed
  // of lighting effects.
  if (iUptime < millis()) {iUptime = millis();iTimer++;}
  

  // Poll sensors; polls ever iPollingInterval for performance
  // Logic for touch sensor events
  if (iTimer % iPollingInterval == 0) {
    pollSensors();
    // onPress
    if (bLastTouchValue == LOW && bCurrentTouchValue == HIGH) {
      iPressTime = iTimer;
      bLastTouchValue = bCurrentTouchValue;
    } 
    // onRelease
    else if (bLastTouchValue == HIGH && bCurrentTouchValue == LOW) {
      touchReleaseEvent();
      bModeSetup = 0;
      bLastTouchValue = bCurrentTouchValue;
      //diLiteMode.write(iLiteMode); save on change disabled in lieu of autosave
    }
  }

  // Check the value of iLightValue to see if it's evening
  // Turn on if it's evening or night
  if (iLightValue < cLightSwitch) { // is evening
    // Yay status LEDs
    if (debugMode) {ledBlink(500);}
    else {statusOn();}
    // Pick a lite mode, any lite mode
    switch (iLiteMode) {
      case -3:
        // Future debugging use
        break;
      case -2:
        // Future debugging use
        break;
      case -1: 
        ledTest(1000);
        break;
      case 0:
        ledMode0();
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
      case 4:
        ledMode4();
        break;
      case 5:
        ledMode5();
        break;
      case 6:
        ledMode6();
        break;
      case 7:
        ledMode7();
        break;
      case 8:
        ledMode8();
        break;
      case 9:
        ledMode9();
        break;
      case 10:
        ledMode10();
        break;
    }
  } else { // Is day time
    statusOff();
    ledMode0();
    //digitalWrite(LED_BUILTIN, HIGH);
  }
  autoSave();
}

/*********************
 * Support Functions *
 * ******************/

void pollSensors() {
  // light sensor polling
  iLightValue = analogRead(cLightSensorPin); // Read light measurement
  //Serial.println("iLightValue: " + (String)iLightValue);
  // Read the touch sensor value
  bCurrentTouchValue = digitalRead(cTouchSensorPin);
}

// Handle touch sensor events, interrupt version
// Deprecated.
// This is "supposed" to mimic an onReleased event
/*
void touchReleaseEvent() {
  unsigned long iNow = millis();
  static unsigned long iThen = 0;
  iNow = millis();
  if (iNow - iThen > iPollingInterval) {
    iLiteMode++;
    if (iLiteMode > iTotalLiteModes)
    {
      iLiteMode = 0;
    }
    bModeSetup = 0;
    iThen = iNow;
  }
}
*/

// Handle touch sensor events, polling version
// the interrupt version if I wanna play around with both
void touchReleaseEvent() {
  //if (debugMode) { Serial.println("Released");}
  // Get current uptime, math it, reset setup
  iReleaseTime = iTimer;
  unsigned long iHoldTime = iReleaseTime - iPressTime;
  if (iHoldTime > iDemoModeTime)
  { // Demo Mode Press
    iLiteMode = 10;
  }
  else if (iHoldTime > iLongPressTime)
  { // Long Press, turn off
    iLiteMode = 0;
  }
  else if (iHoldTime < iShortPressTime)
  { // Short Press
    iLiteMode++;
    // Make sure we don't go too high
    if (iLiteMode > iTotalLiteModes)
    {
      if (debugMode) {iLiteMode = -1;} 
      else {iLiteMode = 0;}
    }
  }
}

// Save the current liteMode every 10 minutes if needed
void autoSave() {
  if (iTimer % 600000 == 0) { 
    if (iLiteMode != diLiteMode.read()) {
      diLiteMode.write(iLiteMode);
    }
  }
}

// lights the built-in LED to indicate the program is running
void statusOn() {
  digitalWrite(LED_BUILTIN, LOW);
}

// lights the built-in LED to indicate the program is idle
void statusOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}

// Blinks the built-in LED based on blinkDelay
void ledBlink(unsigned short blinkDelay) {
  if (iTimer % blinkDelay == 0) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}
// Blinks the built-in LED based on onDelay and offDelay
// Note: XIAO ties LOW to LED-ON
void ledBlink(unsigned short onDelay, unsigned short offDelay) {
  boolean bLED = digitalRead(LED_BUILTIN);
  if (iTimer % onDelay == 0 && bLED == HIGH) {
    digitalWrite(LED_BUILTIN, LOW);
  } else if (iTimer % offDelay == 0 && bLED == LOW) {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

// generic function to set up variables for each lighting mode
void modeSetup(byte redMax, byte greenMax, byte blueMax, byte red, byte green, byte blue, int fadeTime) {
  iRedMax = redMax; // Maximum value for red LED
  iGreenMax = greenMax;
  iBlueMax = blueMax;
  iRed = red; // Current value for red LED
  iGreen = green;
  iBlue = blue;
  iTimer = iFadeTime;
  iFadeTime = fadeTime; // The fade time value appropriate for the calling light effect
  bModeSetup = 1;
}

// Just a test routine to turn on one LED at a time
void ledTest(int speed) {
  if (bModeSetup != 1) {
    for (short i=0; i < cLedCount; i++) {
      leds[i] = CRGB(0, 0, 0);
    }
    iCycle = 0;
    bModeSetup = 1;
  }
  if (iTimer % speed == 0) {
    for (byte i=0; i < cLedCount; i++) {leds[i] = CRGB(0, 0, 0);}
    leds[iCycle] = CRGB(20, 20, 20);
    FastLED.show();
    iCycle++;
    if (iCycle >= cLedCount) {iCycle = 0;}
  }
}

/************************
 * LED Strip Lite Modes *
 * *********************/

// lites off
void ledMode0()
{
  if (bModeSetup != 1)
  {
    modeSetup(0, 0, 0, 0, 0, 0, 10);
    iCycle = 0;
  }
  if (iTimer % iFadeTime == 0)
  {
    for (byte i = 0; i < cLedCount; i++) {leds[i] = CRGB(0, 0, 0);}
    FastLED.show();
    // reset important things to defaults
  }
}

// comfy yellow, static
void ledMode1() {
  // set up this mode
  if (bModeSetup != 1) {
    modeSetup(iLiteMax/2, iLiteMax/4, 0, 255, 127, 0, 50); 
  }
  if (iTimer % iFadeTime == 0) { 
    byte iRTemp = map(iRed, 0, 255, 0, iRedMax);
    byte iGTemp = map(iGreen, 0, 255, 0, iGreenMax);
    byte iBTemp = map(iBlue, 0, 255, 0, iBlueMax);
    for (short i = 0; i < cLedCount; i++) {
      leds[i] = CRGB(iRTemp, iGTemp, iBTemp);
    }
  }
  FastLED.show();
}

// comfy yellow, flicker
// I'd like to rewrite this to smooth transitions between levels
// Less "flashing lights"-y and easier for people with photo-
// sensitive siezures etc.
void ledMode2() {
  // set up this mode
  if (bModeSetup != 1) {
    modeSetup(iLiteMax/2, iLiteMax/4, 0, 255, 127, 0, 10); 
  }
  if (iTimer % iFadeTime == 0) {
    for (short i = 0; i < cLedCount; i++) {
      // We modify i-RGB directly here, to save a whopping 3 bytes of memory
      // Watch out cause other modes rely on RGB being 0-255 values
      iRed = random(5, iRedMax);
      iGreen = iRed/2;
      iBlue = 0;
      leds[i] = CRGB(iRed, iGreen, iBlue);
    }
  FastLED.show();
  }
}

// comfy yellow, random on/off cycles
// Same as flicker, just much slower
// Same someday as flicker...
void ledMode3() {
  // set up this mode
  if (bModeSetup != 1) {
    // 2 second cycle time
    // should probably define a constant for easier changes 
    modeSetup(iLiteMax/2, iLiteMax/4, 0, 255, 127, 0, 2000); 
  }
  if (iTimer % iFadeTime == 0) {
    for (short i = 0; i < cLedCount; i++) {
      // We modify i-RGB directly here, to save a whopping 3 bytes of memory
      // Watch out cause other modes rely on RGB being 0-255 values
      iRed = random(0, iRedMax);
      iGreen = iRed/2;
      iBlue = 0;
      leds[i] = CRGB(iRed, iGreen, iBlue);
    }
  FastLED.show();
  }
}

// Christmas, 2 colors, 3 pixels, chasing
// Red and Green fading along the string
void ledMode4() {
  // set up this mode
  if (bModeSetup != 1) {
    modeSetup(iLiteMax/2, iLiteMax/2, 0, 127, 0, 0, 10); 
  }
  // Run this function only once every iFadeTime number of ms
  if (iTimer % iFadeTime == 0) {
    // Set the color of each LED in the string
    for (short i = 0; i < cLedCount; i++) {
      if (i % 3 == 0)
      {
        // Math. Take value of iRed/G/B and pass it to a sin function
        // Take the result and map it between 0 and iRedMax/G/B
        byte iRTemp = map(cubicwave8(iRed), 0, 255, 0, iRedMax);
        byte iGTemp = map(cubicwave8(iGreen), 0, 255, 0, iGreenMax);
        byte iBTemp = map(cubicwave8(iBlue), 0, 255, 0, iBlueMax);
        if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp);
      }
      else if (i % 3 == 1)
      {
        byte iRTemp = map(cubicwave8(iRed + 85), 0, 255, 0, iRedMax);
        byte iGTemp = map(cubicwave8(iGreen + 85), 0, 255, 0, iGreenMax);
        byte iBTemp = map(cubicwave8(iBlue + 85), 0, 255, 0, iBlueMax);
        if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp);
      }
      else if (i % 3 == 2)
      {
        byte iRTemp = map(cubicwave8(iRed + 170), 0, 255, 0, iRedMax);
        byte iGTemp = map(cubicwave8(iGreen + 170), 0, 255, 0, iGreenMax);
        byte iBTemp = map(cubicwave8(iBlue + 170), 0, 255, 0, iBlueMax);
        if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp);
      }
      else
      {
        //Something went wrong
      }
    }
    FastLED.show();

    // Increment the colors
    // These roll over to 0 after 255,
    // Which is perfect, since cubicwave8
    // returns 0 for both 0 and 255 inputs
    iRed++;
    iGreen++;
    iBlue++;
  }
}

// Christmas, 3 colors, 3 pixels, fading
// Fades R, G, and B, between two colors each pixel
// Very Christmassy, even tho it technically has all the colors
void ledMode5() {
  // set up this mode
  if (bModeSetup != 1) {
    modeSetup(iLiteMax/2, iLiteMax/2, 0, 127, 0, 0, 10); 
  }
  // Run this function only once every iFadeTime number of ms
  if (iTimer % iFadeTime == 0) {
    // Set the color of each LED in the string
    for (short i = 0; i < cLedCount; i++) {
      byte iRTemp = map(cubicwave8(iRed),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen),0,255,0,iGreenMax);
      byte iBTemp = map(cubicwave8(iBlue),0,255,0,iBlueMax);
      if (debugMode) {Serial.println("Mode: 5, Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
      if (i % 3 == 0) {
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
      } else if (i % 3 == 1) {
        leds[i] = CRGB(iGTemp, iBTemp, iRTemp); 
      } else if (i % 3 == 2) {
        leds[i] = CRGB(iBTemp, iRTemp, iGTemp); 
      } else {
        //Something went wrong
      }
    }
    FastLED.show();

    // Math the colors 
    iRed++;
    iGreen++;
    iBlue++;
  }
}

// Rainbow, 3 colors, all pixels, fading
void ledMode6() {
  
  // set up this mode
  if (bModeSetup != 1) {
    modeSetup(iLiteMax/3, iLiteMax/3, iLiteMax/3, 255, 127, 0, 10); 
  }
  // Run this function only once every iFadeTime number of ms
  if (iTimer % iFadeTime == 0) {
    
    // Set the color of each LED in the string
    for (short i = 0; i < cLedCount; i++) {
      byte iRTemp = map(cubicwave8(iRed),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen),0,255,0,iGreenMax);
      byte iBTemp = map(cubicwave8(iBlue),0,255,0,iBlueMax);
      if (debugMode) {Serial.println("Mode: 5, Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
      leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
    }
    FastLED.show();

    // Math the colors
    iRed++;
    iGreen++;
    iBlue++;
  }
}

// Rainbow, 3 colors, 3 pixels, fading
void ledMode7() {
  // set up this mode
  if (bModeSetup != 1) {
    modeSetup(iLiteMax/3, iLiteMax/3, iLiteMax/3, 127, 63, 0, 10); 
  }
  // Run this function only once every iFadeTime number of ms
  if (iTimer % iFadeTime == 0) {
    // Set the color of each LED in the string
    for (short i = 0; i < cLedCount; i++) {
      byte iRTemp = map(cubicwave8(iRed),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen),0,255,0,iGreenMax);
      byte iBTemp = map(cubicwave8(iBlue),0,255,0,iBlueMax);
      if (debugMode) {Serial.println("Mode: 5, Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
      if (i % 3 == 0) {
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
      } else if (i % 3 == 1) {
        leds[i] = CRGB(iGTemp, iBTemp, iRTemp); 
      } else if (i % 3 == 2) {
        leds[i] = CRGB(iBTemp, iRTemp, iGTemp); 
      } else {
        //Something went wrong
      }
    }
    FastLED.show();

    // Math the colors
    iRed++;
    iGreen++;
    iBlue++;
  }
}

// Blue/White, 2 colors, 3 pixels, chasing
void ledMode8() {
  // Run this function only once every iFadeTime number of ms
  if (bModeSetup != 1) {
    modeSetup(iLiteMax/3, iLiteMax/3, iLiteMax/3, 0, 0, 255, 10); // set up this mode
  }
  if (iTimer % iFadeTime == 0) {
    // Set the color of each LED in the string
    for (short i = 0; i < cLedCount; i++) {
      if (i % 3 == 0) {
      byte iRTemp = map(cubicwave8(iRed),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen),0,255,0,iGreenMax);
      byte iBTemp = iBlueMax;
      if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
      } else if (i % 3 == 1) {
      byte iRTemp = map(cubicwave8(iRed+85),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen+85),0,255,0,iGreenMax);
      byte iBTemp = iBlueMax;
      if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
      } else if (i % 3 == 2) {
      byte iRTemp = map(cubicwave8(iRed+170),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen+170),0,255,0,iGreenMax);
      byte iBTemp = iBlueMax;
      if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
      } else {
        //Something went wrong
      }
    }
    FastLED.show();

    // Math the colors
    iRed++;
    iGreen++;
    iBlue++;
  }
}

// Blue/Purple, 2 colors, 3 pixels, chasing
void ledMode9() {
  // Run this function only once every iFadeTime number of ms
  if (bModeSetup != 1) {
    modeSetup(iLiteMax/3, iLiteMax/15, iLiteMax/3, 0, 0, 255, 10); // set up this mode
  }
  if (iTimer % iFadeTime == 0) {
    // Set the color of each LED in the string
    for (short i = 0; i < cLedCount; i++) {
      if (i % 3 == 0) {
      byte iRTemp = map(cubicwave8(iRed),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen),0,255,0,iGreenMax);
      byte iBTemp = iBlueMax;
      if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
      } else if (i % 3 == 1) {
      byte iRTemp = map(cubicwave8(iRed+85),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen+85),0,255,0,iGreenMax);
      byte iBTemp = iBlueMax;
      if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
      } else if (i % 3 == 2) {
      byte iRTemp = map(cubicwave8(iRed+170),0,255,0,iRedMax);
      byte iGTemp = map(cubicwave8(iGreen+170),0,255,0,iGreenMax);
      byte iBTemp = iBlueMax;
      if (debugMode) {Serial.println("LED 3: Red: " + (String)iRTemp + ", Green: " + iGTemp + ", Blue: " + iBTemp);}
        leds[i] = CRGB(iRTemp, iGTemp, iBTemp); 
      } else {
        //Something went wrong
      }
    }
    FastLED.show();

    // Math the colors
    iRed++;
    iGreen++;
    iBlue++;
  }
}

// cycle all modes
void ledMode10() {
  // set up this mode
  // this mode runs every loop by design
  if (iCycle < 1 || iCycle > 9) {
    iCycle = 1;
  }
  // cycle thorugh other modes
  switch (iCycle) {
    case 1:
      ledMode1();
      break;
    case 2:
      ledMode2();
      break;
    case 3:
      ledMode3();
      break;
    case 4:
      ledMode4();
      break;
    case 5:
      ledMode5();
      break;
    case 6:
      ledMode6();
      break;
    case 7:
      ledMode7();
      break;
    case 8:
      ledMode8();
      break;
    case 9:
      ledMode9();
      break;
  }
  // every 10 seconds, switch modes
  // might should define a constant for this
  if (iTimer % iDemoModeTime == 0 && iTimer != 0) {
    iCycle++;
    bModeSetup = 0;
  }
}

// alternative fading math
// sourced from arduino.stackexchange.com
// https://arduino.stackexchange.com/questions/18574/fastled-fade-ws2812b
// It uses delay() which is horible, but it demonstrates
// some functions in FastLED that I wasn't aware of. Namely, the
// leds[].color variables. Didn't know those were a thing. 
// (Insert comment about FastLED's terrible documentation here)
// They will come in very handy for future lite modes.
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
