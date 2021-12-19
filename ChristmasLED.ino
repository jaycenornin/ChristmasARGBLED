/*
   Heyo!
   So this program uses an Arduino Xiao to light up an ARGB light string based on the WS2812b chip
   Licensed by MIT license. I appreciate attribution if you use this code publicly. But it's not like I can stop you.

   Explanation of function:
    - This sketch works by first rate-limiting the loop() to run once every iLoopTime number of ms. 
    - We set iFadeTime to the number of ms we want between each fade step in a "fade" effect.
    - We calculate iFadeLoops to be the number of times loop() should run during iFadeTime.
    - Each loop(), we check the current value of iLiteMode and run the corresponding ledModeX() function, then increment iTimer.
    - Within each ledModeX() function is an if statement that mods iTimer over iFadeLoops so the subsequent commands only run every
      iFadeLoops number of loop() cycles.
      For example: 
        - Set iLoopTime to 1ms
        - Set iFadeTime to 100ms
        - Set iFadeLoops to iFadeTime / iLoopTime
        - Start iTimer at 0 and ++ every loop()
        - loop() runs every 1ms
        - ledModeX() runs every 1ms
        - if (iTimer % iFadeLoops == 0) { runs every 100 loop()s which is every 100ms}
        - If we set iLoopTime to 5ms and iFadeTime to 100ms, loop() runs every 5ms, ledModeX() runs every 5ms,
        - and if(iTimer % iFadeLoops ==0) { runs every 20 loops()s which is every 100ms}        
    - Each ledModeX() function calls a modeSetup() function and sets iFadeTime based on the best setting for that lite effect
    - Each ledModeX() function has an explanation of how it works
    - The iRed/iGreen/iBlue variables are set to bytes. This ensures they max at 255 and roll back over to 0. Useful for mapping
      to a sin function and controlling LED brightness
   
   Important limitations:
    Current program uses ~14% of total available program space

    The XIAO can output ~500ma over its 5v VCC. That limits each pixel to ~30ma. 
      This corresponds to a maximum TOTAL RGB value of 128.
      (Math: 500ma / 16 pixels = 31.25ma per pixel, round down to 30; 30ma per pixel / 3 ma per pixel = 10ma max per color; 
      10ma per color / 20ma max per color = 50%; 50% of 256 = 128 RGB per color. 
      This should include a margin of error, but try to keep it below 100.)
    
    The light sensor works but is not calibrated.

    Touch sensors work, but you may want to customize the debounce timer.
      Using interrupts gets you fast response when you tap, but apparently faulty edge detection and sensor bouncing create unexpected behavior.
        Example: Even when using the FALLING interrupt, loop() halts on a press as though using CHANGE.
        Example: Button presses may not debounce correctly and you'll run the ISR multiple times.
      Barring unexpected performance problems, polling every loop may provide smoother operation, but you'll have to adjust loop and fade timers.

    I'm not a fan of the use of delay() to handle effect timing. So far the only other option I've found is to use if + millis() + math to
      run loop() at full speed, but there doesn't appear to be any way to get a resolution higher than 1ms so it's 6 vs a half dozen.
      I could substitute micros() into that math for ~3μs resolution and I may explore that in the future. This would be valuable for replacing
        interrupts with polling every loop() with low risk of missing an input event.
        But it means a LOT of heavy-duty math to ensure the timing for lighting effects is accurate.
    
    I use RGB instead of HSV cause after testing it was smoother. HSV had noticeable "stepping" and uggly shifting at the low brightness I'm working at.

    There's still some old code in here from various ideas I toyed with. This ain't clean by any stretech.

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
   - Long press on touch sensor for switching the unit full off or enabling demo mode iLiteMode(10)
   - Second touch sensor for controlling color combinations and fade effects separately (mix and match!)
   - Save the current lite mode to nonvolatile memory to restore prior settings on startup
   - Make chasing code more efficient - that's a LOT of function calls to make them work.
*/

// Includes
#include <FastLED.h>

// For debugging
// Set to NULL for normal operation
// Set to 0 to turn off
// Set to a non-zero positive integer to enable that light mode
// Set to -1 to sequently test each LED at RGB(20)
// Side note - NULL throws compiler warnings
#define debugMode NULL

// Constants
#define cLedCount  16
#define cLedControlPin 9
#define cTouchSensorPin 8
#define cLiteSensorPin  7

// Configuration parameters - Constant
#define cLightSwitch 50 // the "percent" value above which the device is off, and below which it is on
#define iLiteMax 60 // Maximum RGB brightness per pixel, don't go over 100 per pixel for 16 pixel string
#define iLoopTime 1 // ms delay between main loops, used with iFadeTime to ensure consistent timing of all light effects
#define iTotalLiteModes 9 // Number of LED lite modes
#define iDebounceTimer 2000 // number of ms for debouncing the touch input

// Configuration parameters - Variable
volatile short iLiteMode = 3; // LED mode (0=off), volatile for interrupts
volatile boolean bModeSetup = 0; // Used to set up each lighting mode, reset by interrupts
unsigned long int iReleaseTime = 0;
//unsigned long int iPressTime = 0; // used for interrupts & button press timing
//unsigned long int iLastPressTime = 0;
unsigned long int iCurrentTime = 0;
long iTimer = 0; // Timer used for counting the number of times loop() has run, used with iLoopTime and iFadeTime to regulate LED effects
long iFadeTime = 100; // ms between fade cycles
short iLightValue = 0; // value of ambient light read from sensor
short iCycle = 0; // Repurposed for rotating between modes (ledMode10), 10 seconds
short iFadeLoops = iFadeTime / iLoopTime; // Number of loops per fade step
byte fLight = 0; // mathed value of iLightValue
byte iRedMax, iGreenMax, iBlueMax = 0; // For fading more than 2 colors
byte iRed, iGreen, iBlue = 0; // For fading more than 2 colors
//boolean bCurrentTouchValue = LOW;
//boolean bLastTouchValue = LOW;

CRGB leds[cLedCount];

void setup() {
  // put your setup code here, to run once:

  // enable serial output when debugging
  // Disabled cause this started causing problems; unknown why but it causes the arduino to hang on start
  if (debugMode) {
    Serial.begin(9600);
    /*while (!Serial) {
      ; // wait for Serial to be ready, probably the cause of the hang
    }*/
  }
  // set pins
  pinMode(cLedControlPin, OUTPUT);
  pinMode(cLiteSensorPin, INPUT);
  pinMode(cTouchSensorPin, INPUT);
  // initialize fastLED
  FastLED.addLeds<WS2812, cLedControlPin, GRB>(leds, cLedCount);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 400);
  // initialize interrupts
  attachInterrupt(digitalPinToInterrupt(cTouchSensorPin), touchReleaseEvent, FALLING); // react to an "onRelease" event from the touch sensor
}

void loop() {
  // put your main code here, to run repeatedly:
  if (debugMode) { // ignore sensors in debugmode
    switch (debugMode)
    {
      default:
        iLiteMode = debugMode;
        fLight = debugMode;
        break;
    }
    // to-do: read input from the serial port and set iLiteMode to any read integer
  } else { 
    // poll sensors
    // int bCurrentTouchValue = digitalRead(cTouchSensorPin); // Disabled while testing interrupts
    iLightValue = analogRead(cLiteSensorPin);
    fLight = map(iLightValue,0,255,0,100); // convert absolute light measurement to % (not sure how high iLightValue goes)
  }

  // Handle touch sensor events
  // This is "supposed" to mimic an onReleased event
  // Each touch increments the LiteMode up to iTotalLiteModes modes, then reverts to 0
  // LiteMode 0 = Off
  /* off while I test interrupts
    if (bCurrentTouchValue == HIGH && bLastTouchValue == LOW) {
    bLastTouchValue = HIGH;
    }
    if (bCurrentTouchValue == LOW && bLastTouchValue == HIGH) {
    bLastTouchValue == LOW;
    iLiteMode++;
    if (iLiteMode > iTotalLiteModes) {
      iLiteMode = 0;
    }
    }
  */

  // trying to pull short press vs long press out of the interrupt
  // gave up on this for now, slowly moving the code towards polling instead
  // Keeping this old code cause it may be useful in polling short vs long as well
  /*iCurrentTime = millis();
  // Short Press
  if (iPressTime - iCurrentTime < 2000 && digitalRead(cTouchSensorPin) == LOW)
  {
    iLiteMode++;
    if (iLiteMode > iTotalLiteModes)
    {
      iLiteMode = 0;
    }
    bModeSetup = 0;
    iLastPressTime = iPressTime;
  }
  else if (iPressTime - iCurrentTime > 2000 && iPressTime - iCurrentTime < 5000 && digitalRead(cTouchSensorPin) == HIGH)
  {
    iLiteMode = 10;
    bModeSetup = 0;
    iLastPressTime = iPressTime;
  } */

  if (fLight < cLightSwitch) {
    statusOn();
    switch (iLiteMode) {
      case -1: 
        ledTest();
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
  } else {
    statusOff();
    ledMode0();
  }
  // Increment iTimer
  // This is used to count the number of times loop() has run
  // Combined with iLoopTime and iFadeTime, this is used to regulate
  // the speed of lighting effects.
  iTimer++;
  //Serial.println("iTimer: " + iTimer);
  delay(iLoopTime); // Main loop will run every iLoopTime ms
}

/*********************
 * Support Functions *
 * ******************/


// Handle touch sensor events
// This is "supposed" to mimic an onReleased event
// Each touch increments the LiteMode up to iTotalLiteModes modes, then reverts to 0
// LiteMode 0 = Off
// Calling this on an interrupt works, but has undesirable side effects 
// (appears to be improper edge detection in the arduino "FALLING" triger)
// (calling noInterrupts() from within the ISR could be fun to try)
void touchReleaseEvent() {
  static unsigned long iLastPressTime = 0;
  iReleaseTime = millis();
  if (iReleaseTime - iLastPressTime > iDebounceTimer) {
    iLiteMode++;
    if (iLiteMode > iTotalLiteModes)
    {
      iLiteMode = 0;
    }
    bModeSetup = 0;
    iLastPressTime = iReleaseTime;
  }
}


// lights the built-in LED to indicate the program is running
void statusOn() {
  //ledBlink(1500,250);
  digitalWrite(LED_BUILTIN, LOW);
}

// lights the built-in LED to indicate the program is idle
void statusOff() {
  //ledBlink(250,1500);
  digitalWrite(LED_BUILTIN, HIGH);
}

// blinks the built-in LED based on onDelay and offDelay
// Note: XIAO ties LOW to LED-ON
void ledBlink(short onDelay, short offDelay) {
  digitalWrite(LED_BUILTIN, LOW);
  delay(onDelay);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(offDelay);
}

// Just a test routine to turn on one LED at a time
void ledTest() {
  for (short i=0; i < cLedCount; i++){
    leds[i] = CRGB(20, 20, 20);
    leds[i-1] = CRGB(0, 0, 0);
    FastLED.show();
    delay(250);
  }
}

// generic function to set up variables for each lighting mode
void modeSetup(int redMax, int greenMax, int blueMax, int red, int green, int blue, int fadeTime) {
  iRedMax = redMax; // Maximum value for red LED
  iGreenMax = greenMax;
  iBlueMax = blueMax;
  iRed = red; // Current value for red LED
  iGreen = green;
  iBlue = blue;
  iFadeTime = fadeTime; // The fade time value appropriate for the calling light effect
  iFadeLoops = iFadeTime / iLoopTime; // recalculate iFadeLoops with the updated iFadeTime
  bModeSetup = 1;
}

/************************
 * LED Strip Lite Modes *
 * *********************/

// lites off
void ledMode0() {
  for (short i = 0; i < cLedCount; i++) {
    leds[i] = CRGB(0, 0, 0);
  }
  FastLED.show();
  // reset important things to defaults
  modeSetup(0,0,0,0,0,0,5); 
  bModeSetup = 0;
  iCycle = 0;
}

// comfy yellow, static
void ledMode1() {
  // set up this mode
  if (bModeSetup == 0) {
    modeSetup(iLiteMax/2, iLiteMax/4, 0, 255, 127, 0, 1000); 
  }
  if (iTimer % iFadeLoops == 0) { // No need to run this often since it's a static effect
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
void ledMode2() {
  // set up this mode
  if (bModeSetup == 0) {
    modeSetup(iLiteMax/2, iLiteMax/4, 0, 255, 127, 0, 10); 
  }
  if (iTimer % iFadeLoops == 0) {
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
void ledMode3() {
  // set up this mode
  if (bModeSetup == 0) {
    // 2 second cycle time
    // should probably define a constant for easier changes 
    modeSetup(iLiteMax/2, iLiteMax/4, 0, 255, 127, 0, 2000); 
  }
  if (iTimer % iFadeLoops == 0) {
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
void ledMode4() {
  // set up this mode
  if (bModeSetup == 0) {
    modeSetup(iLiteMax/2, iLiteMax/2, 0, 127, 0, 0, 5); 
  }
  // Run this function only once every iFadeLoops number of loops
  if (iTimer % iFadeLoops == 0) {
    // Set the color of each LED in the string
    for (short i = 0; i < cLedCount; i++) {
      if (i % 3 == 0)
      {
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

    // Math the colors
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
  if (bModeSetup == 0) {
    modeSetup(iLiteMax/2, iLiteMax/2, 0, 127, 0, 0, 5); 
  }
  // Run this function only once every iFadeLoops number of loops
  if (iTimer % iFadeLoops == 0) {
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
  if (bModeSetup == 0) {
    modeSetup(iLiteMax/3, iLiteMax/3, iLiteMax/3, 255, 127, 0, 5); 
  }
  // Run this function only once every iFadeLoops number of loops
  if (iTimer % iFadeLoops == 0) {
    
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
  if (bModeSetup == 0) {
    modeSetup(iLiteMax/3, iLiteMax/3, iLiteMax/3, 127, 63, 0, 5); 
  }
  // Run this function only once every iFadeLoops number of loops
  if (iTimer % iFadeLoops == 0) {
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
  // Run this function only once every iFadeLoops number of loops
  if (bModeSetup == 0) {
    modeSetup(iLiteMax/3, iLiteMax/3, iLiteMax/3, 0, 0, 255, 5); // set up this mode
  }
  if (iTimer % iFadeLoops == 0) {
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
  // Run this function only once every iFadeLoops number of loops
  if (bModeSetup == 0) {
    modeSetup(iLiteMax/3, iLiteMax/15, iLiteMax/3, 0, 0, 255, 5); // set up this mode
  }
  if (iTimer % iFadeLoops == 0) {
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
  if (iTimer % 2000 == 0 && iTimer != 0) {
    iCycle++;
    bModeSetup = 0;
  }
}

