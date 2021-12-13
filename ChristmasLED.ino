/*
   Heyo!
   So this program uses an Arduino Xiao to light up an ARGB light string based on the WS2812b chip
   
   Important limitations:
    Current program uses ~14% of total available program space

    The XIAO can output ~500ma over its 5v VCC. That limits each pixel to ~30ma. 
      This corresponds to a maximum TOTAL RGB value of 128.
      (Math: 500ma / 16 pixels = 31.25ma per pixel, round down to 30; 30ma per pixel / 3 ma per pixel = 10ma max per color; 
      10ma per color / 20ma max per color = 50%; 50% of 256 = 128 RGB per color. 
      This should include a margin of error, but try to keep it below 100.)
    
    The light sensor works but is not calibrated.

    Touch sensors are not tested (once upon a time "Prime" meant a 2 day celivery guarantee)
      This also means the touchEvent() interrupt and function are untested
    
    I use RGB instead of HSV cause after testing it was smoother. HSV had noticeable "stepping" and uggly shifting at the brightness I'm working at.

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
    mode 10 - Rotate through all modes every 10 seconds

   Future light modes to consider:
   x Warm yellow, candle flicker
   x Red/Green, moving (like fading, but with a 3rd intermediate color to give the illusion of linear motion)
   - Red/yellow/Green, fading, moving, fixed
   - Other holidays

   To-Do
   x Additional light modes
   x Improved fading (longer pause between fade steps at min/max of each color, so they stay at red/green longer than the intermediate yellow)
   - Second touch sensor for controlling color combinations and fade effects separately (mix and match!)
   x Change iLiteMax and adjust lighting modes to use iLiteMax/2 or /3 to keep the luminosity more even
   - Make chasing code more efficient - that's a LOT of function calls to make them work.
*/

// Includes
#include <FastLED.h>

// For debugging
// Comment out for standard operation
// Set to 0 to turn off
// Set to a non-zero positive integer to enable that light mode
// Set to -1 to sequently test each LED at RGB(20)
#define debugMode 10

// Constants
#define cLedCount  16
#define cLedControlPin 6
#define cLiteSensorPin  7
#define cTouchSensorPin 8



// Configuration parameters - Constant
#define cLightSwitch 50 // the "percent" value above which the device is off, and below which it is on
#define iLiteMax 60 // Maximum RGB brightness per pixel, don't go over 100 per pixel for 16 pixel string
#define iLoopTime 5 // ms delay between main loops
#define iTotalLiteModes 7 // Number of LED lite modes

// Configuration parameters - Variable
volatile short iLiteMode = 0; // LED mode (0=off), volatile for interrupts
volatile boolean bModeSetup = 0; // Used to set up each lighting mode, reset by interrupts
long iTimer = 0; // Timer used for tracking loops during certain fade cycles
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
  if (debugMode > 0) {
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
  attachInterrupt(digitalPinToInterrupt(cTouchSensorPin), touchEvent, FALLING); // react to an "onRelease" event from the touch sensor
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
  iTimer++;
  //Serial.print("iTimer: " + iTimer);
  delay(iLoopTime); // Main loop will run every iLoopTime
}

/*********************
 * Support Functions *
 * ******************/


// Handle touch sensor events
// This is "supposed" to mimic an onReleased event
// Each touch increments the LiteMode up to iTotalLiteModes modes, then reverts to 0
// LiteMode 0 = Off
void touchEvent() {
  iLiteMode++;
  if (iLiteMode > iTotalLiteModes) {
    iLiteMode = 0;
  }
  bModeSetup = 0;
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
void ledBlink(short onDelay, short offDelay) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(onDelay);
  digitalWrite(LED_BUILTIN, LOW);
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
  iRedMax = redMax;
  iGreenMax = greenMax;
  iBlueMax = blueMax;
  iRed = red;
  iGreen = green;
  iBlue = blue;
  iFadeTime = fadeTime;
  iFadeLoops = iFadeTime / iLoopTime; // Do this each loop in
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
  if (iTimer % iFadeLoops == 0) { // No need to run this often since it's static
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
