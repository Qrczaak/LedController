#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

//************************************* WIFI ***********************************
#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "PASSWORD"
#endif

//***** Konfiguracja WIFI *****
const char* ssid = STASSID;
const char* password = STAPSK;
boolean connection_established = false;

IPAddress staticIP(192, 168, 1, 110); //ESP static ip
IPAddress gateway(192, 168, 1, 1);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
ESP8266WebServer server(80);

//****************************
//*****************************************************************************


//********************************* LED ***************************************
//***** Konfiguracja LED *****
#define NUM_LEDS 44
#define PIN D1
#define COLOR_ORDER GRB
#define BRIGHTNESS  25
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define UPDATES_PER_SECOND 2
CRGB leds[NUM_LEDS];
//****************************

//***** Konfiguracja PIN *****
#define ANALOG_READ A0
#define DELAY 1
//****************************
//*****************************************************************************


//******************************** SOUND **************************************

//Confirmed microphone low value, and max value
#define MIC_LOW 175.0
#define MIC_HIGH 700.0
/** Other macros */
//How many previous sensor values effects the operating average?
#define AVGLEN 15
//How many previous sensor values decides if we are on a peak/HInGH (e.g. in a song)
#define LONG_SECTOR 5

//Ilosc kauczukowych pileczek
#define BallCount 3

//Mneumonics
#define HIGH  3
#define NORMAL 2

//How long do we keep the "current average" sound, before restarting the measuring
#define MSECS 2000
#define CYCLES MSECS / DELAY

/*Sometimes readings are wrong or strange. How much is a reading allowed
to deviate from the average to not be discarded? **/
#define DEV_THRESH 10

//Arduino loop delay
#define DELAY 1

float fscale( float originalMin, float originalMax, float newBegin, float newEnd, float inputValue, float curve);
void insert(int val, int *avgs, int len);
int compute_average(int *avgs, int len);
void visualize_music();

//How many LEDs to we display
int curshow = NUM_LEDS;

/*Not really used yet. Thought to be able to switch between sound reactive
mode, and general gradient pulsing/static color*/
int mode = 0;

//Showing different colors based on the mode.
int songmode = NORMAL;

//Average sound measurement the last CYCLES
unsigned long song_avg;

//The amount of iterations since the song_avg was reset
int iter = 0;

//The speed the LEDs fade to black if not relit
float fade_scale = 1.2;

/*Short sound avg used to "normalize" the input values.
We use the short average instead of using the sensor input directly */
int avgs[AVGLEN] = {-1};

//Longer sound avg
int long_avg[LONG_SECTOR] = {-1};

int rgb_loop_state = 0;
int gr_christmass_loop_state = 0;

//***** Zmienne do odbijania pileczek *****
float Gravity = -9.81;
int StartHeight = 1;
float Height[BallCount];
float ImpactVelocityStart;
float ImpactVelocity[BallCount];
float TimeSinceLastBounce[BallCount];
int   Position[BallCount];
long  ClockTimeSinceLastBounce[BallCount];
float Dampening[BallCount];


//How much to increment or decrement each color every cycle
struct Ball {
  int r;
  int g;
  int b;
};

Ball ball1 = {0,0,0};
Ball ball2 = {0,0,0};
Ball ball3 = {0,0,0};

bool randomBallColors = false;
bool setNewBallColor = true;
//*****************************************


//Keeping track how often, and how long times we hit a certain mode
struct time_keeping {
  unsigned long times_start;
  short times;
};

//How much to increment or decrement each color every cycle
struct color {
  int r;
  int g;
  int b;
};

struct time_keeping high;
struct color Color; 
//*******************************************************************************


//***** *********************** TRYBY ******************************************
enum ledMode {
  reactive_sound,
  snake,
  solid_color,
  moving_color,
  juggle_mode,
  rainbow_pallete,
  rainbow_strip_pallete,
  confetti_mode,
  rgb_loop,
  strobe,
  snow_sparkle,
  fire,
  bouncing_balls,
  meteor_rain,
  gr_christmass_mode,
  half_gr_christmass_mode,
  theaterRun,
  snow_sparkle_christmass,
  off 
};


ledMode currentMode = off;

ledMode randomLedModes[] = {juggle_mode,rainbow_pallete,confetti_mode,moving_color,snow_sparkle,meteor_rain,rgb_loop};
ledMode randomChLedModes[] = {gr_christmass_mode,half_gr_christmass_mode,theaterRun,snow_sparkle_christmass};
unsigned long oldTime = 0L;
unsigned long oldChTime = 0L;
unsigned long change_mode_time = 4000;
unsigned long change_ch_mode_time = 8000;

int chrismtass_mode = 0;
boolean randomMode = false;
boolean randomChMode = false;
//**************************************************************************88

int delayTime = 500;
int color_led_index = 0;
int colorDelay = 100;
int adjust_brightness = 64;
uint8_t gHue = 0;
CRGBPalette16 currentPalette;
TBlendType    currentBlending;

/**Funtion to check if the lamp should either enter a HIGH mode,
or revert to NORMAL if already in HIGH. If the sensors report values
that are higher than 1.1 times the average values, and this has happened
more than 30 times the last few milliseconds, it will enter HIGH mode. 
TODO: Not very well written, remove hardcoded values, and make it more
reusable and configurable.  */
void check_high(int avg) {
  if (avg > (song_avg/iter * 1.1))  {
    if (high.times != 0) {
      if (millis() - high.times_start > 200.0) {
        high.times = 0;
        songmode = NORMAL;
      } else {
        high.times_start = millis();  
        high.times++; 
      }
    } else {
      high.times++;
      high.times_start = millis();

    }
  }
  if (high.times > 30 && millis() - high.times_start < 50.0)
    songmode = HIGH;
  else if (millis() - high.times_start > 200) {
    high.times = 0;
    songmode = NORMAL;
  }
}

//Main function for visualizing the sounds in the lamp
void visualize_music() {
  int sensor_value, mapped, avg, longavg;

  //Actual sensor value
  sensor_value = analogRead(ANALOG_READ);
  //If 0, discard immediately. Probably not right and save CPU.
  if (sensor_value < 40)
    return;

  //Discard readings that deviates too much from the past avg.
  mapped = (float)fscale(MIC_LOW, MIC_HIGH, MIC_LOW, (float)MIC_HIGH, (float)sensor_value, 2.0);
  avg = compute_average(avgs, AVGLEN);

  if (((avg - mapped) > avg*DEV_THRESH)) //|| ((avg - mapped) < -avg*DEV_THRESH))
    return;
  
  //Insert new avg. values
  insert(mapped, avgs, AVGLEN); 
  insert(avg, long_avg, LONG_SECTOR); 

  //Compute the "song average" sensor value
  song_avg += avg;
  iter++;
  if (iter > CYCLES) {  
    song_avg = song_avg / iter;
    iter = 1;
  }
    
  longavg = compute_average(long_avg, LONG_SECTOR);

  //Check if we enter HIGH mode 
  check_high(longavg);  

  if (songmode == HIGH) {
    fade_scale = 3;
    Color.r =  8;
    Color.g = 1;
    Color.b = -2;
  }
  else if (songmode == NORMAL) {
    fade_scale = 3;
    Color.r = -1;
    Color.b = 6;
    Color.g = -2;
  }

  //Decides how many of the LEDs will be lit
  curshow = fscale(MIC_LOW, MIC_HIGH, 0.0, (float)NUM_LEDS, (float)avg, -1);

  /*Set the different leds. Control for too high and too low values.
          Fun thing to try: Dont account for overflow in one direction, 
    some interesting light effects appear! */
  for (int i = 0; i < NUM_LEDS; i++) 
    //The leds we want to show
    if (i < curshow) {
      if (leds[i].r + Color.r > 255)
        leds[i].r = 255;
      else if (leds[i].r + Color.r < 0)
        leds[i].r = 0;
      else
        leds[i].r = leds[i].r + Color.r;
          
      if (leds[i].g + Color.g > 255)
        leds[i].g = 255;
      else if (leds[i].g + Color.g < 0)
        leds[i].g = 0;
      else 
        leds[i].g = leds[i].g + Color.g;

      if (leds[i].b + Color.b > 255)
        leds[i].b = 255;
      else if (leds[i].b + Color.b < 0)
        leds[i].b = 0;
      else 
        leds[i].b = leds[i].b + Color.b;  
      
    //All the other LEDs begin their fading journey to eventual total darkness
    } else {
      leds[i] = CRGB(leds[i].r/fade_scale, leds[i].g/fade_scale, leds[i].b/fade_scale);
    }
  FastLED.show(); 
}
//Compute average of a int array, given the starting pointer and the length
int compute_average(int *avgs, int len) {
  int sum = 0;
  for (int i = 0; i < len; i++)
    sum += avgs[i];

  return (int)(sum / len);

}

//Insert a value into an array, and shift it down removing
//the first value if array already full 
void insert(int val, int *avgs, int len) {
  for (int i = 0; i < len; i++) {
    if (avgs[i] == -1) {
      avgs[i] = val;
      return;
    }  
  }

  for (int i = 1; i < len; i++) {
    avgs[i - 1] = avgs[i];
  }
  avgs[len - 1] = val;
}

//Function imported from the arduino website.
//Basically map, but with a curve on the scale (can be non-uniform).
float fscale( float originalMin, float originalMax, float newBegin, float
    newEnd, float inputValue, float curve){

  float OriginalRange = 0;
  float NewRange = 0;
  float zeroRefCurVal = 0;
  float normalizedCurVal = 0;
  float rangedValue = 0;
  boolean invFlag = 0;


  // condition curve parameter
  // limit range

  if (curve > 10) curve = 10;
  if (curve < -10) curve = -10;

  curve = (curve * -.1) ; // - invert and scale - this seems more intuitive - postive numbers give more weight to high end on output 
  curve = pow(10, curve); // convert linear scale into lograthimic exponent for other pow function

  // Check for out of range inputValues
  if (inputValue < originalMin) {
    inputValue = originalMin;
  }
  if (inputValue > originalMax) {
    inputValue = originalMax;
  }

  // Zero Refference the values
  OriginalRange = originalMax - originalMin;

  if (newEnd > newBegin){ 
    NewRange = newEnd - newBegin;
  }
  else
  {
    NewRange = newBegin - newEnd; 
    invFlag = 1;
  }

  zeroRefCurVal = inputValue - originalMin;
  normalizedCurVal  =  zeroRefCurVal / OriginalRange;   // normalize to 0 - 1 float

  // Check for originalMin > originalMax  - the math for all other cases i.e. negative numbers seems to work out fine 
  if (originalMin > originalMax ) {
    return 0;
  }

  if (invFlag == 0){
    rangedValue =  (pow(normalizedCurVal, curve) * NewRange) + newBegin;

  }
  else     // invert the ranges
  {   
    rangedValue =  newBegin - (pow(normalizedCurVal, curve) * NewRange); 
  }

  return rangedValue;
}

 

 
void handleOff(){
   currentMode = off;
   off_led();
}

void setColorValues(int number){
      Color.r = number >> 16;
      Color.g = number >> 8 & 0xFF;
      Color.b = number & 0xFF;
}

void readColors(){
    if(server.argName(0) != NULL && server.argName(0) == "col"){
      String argument = server.arg(0);
      // Get rid of '#' and convert it to integer
      int number = (int) strtol( &argument[0], NULL, 16);
      // Split them up into r, g, b values
      setColorValues(number);
    }
}

void handleColor(){
    randomMode = false;
    off_led();
    readColors();
    currentMode = moving_color;
}

void handleSolidColor(){
    randomMode = false;
    off_led();
    readColors();
    currentMode = solid_color;
}

void movingColorDisplay(){
        if(color_led_index != 0){
          leds[color_led_index-1] = CRGB(Color.g,Color.r,Color.b);
        }
        leds[color_led_index] = CRGB(Color.g,Color.r,Color.b);
        if(color_led_index!=NUM_LEDS){
          leds[color_led_index+1] = CRGB(Color.g,Color.r,Color.b); 
        }
      FastLED.show();
      delay(colorDelay);
      if(color_led_index != 0){
          leds[color_led_index-1] = CRGB(0,0,0);
        }
        leds[color_led_index] = CRGB(0,0,0);
        if(color_led_index!=NUM_LEDS){
          leds[color_led_index+1] = CRGB(0,0,0); 
        }

      if(color_led_index != NUM_LEDS){
        color_led_index++;
      }
      else{
        color_led_index=0;
      }
}

void handleConfettiMode(){
  randomMode = false;
  off_led();
  currentMode=confetti_mode;
}

void handleRainbowPallete(){
  off_led();
  currentMode=rainbow_pallete;
}

void handleRainbowStripPallete(){
  off_led();
  currentMode=rainbow_strip_pallete;
}

void handleJuggleMode(){
  off_led();
  currentMode=juggle_mode;
}


void solidColorDisplay(){
  fill_solid( leds, NUM_LEDS, CRGB(Color.g,Color.r,Color.b));
  FastLED.show();
  currentMode==off;
}

void rainbowPalleteDisplay(){
    currentPalette = RainbowColors_p;
    currentBlending = LINEARBLEND;           
    showPallete();
}

int rainbow_r = 255;
int rainbow_g = 0;
int rainbow_b = 0;

void rainbowPalleteStripDisplay(){
  fill_solid(leds, NUM_LEDS, CRGB(rainbow_r, rainbow_g,rainbow_b));

  if(rainbow_r > 0 && rainbow_g < 255 && rainbow_b == 0){
    rainbow_g++;
  }
  else if(rainbow_r > 0 && rainbow_g == 255 && rainbow_b == 0){
    rainbow_r--;
  }
  else if(rainbow_g == 255 && rainbow_r == 0 && rainbow_b < 255){
    rainbow_b++;
  }
  else if(rainbow_b == 255 && rainbow_g > 0 && rainbow_r == 0){
    rainbow_g--;
  }
  else if(rainbow_r < 255 && rainbow_g == 0 && rainbow_b == 255){
    rainbow_r++;
  }
  else if(rainbow_r == 255 && rainbow_b > 0 && rainbow_g == 0){
    rainbow_b--;
  }
  
  FastLED.show();
  FastLED.delay(1000/colorDelay); 
}

void cloudPalleteDisplay(){
    currentPalette=CloudColors_p;
    currentBlending = LINEARBLEND;           
    showPallete();
}

void juggleModeDisplay(){
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }

    // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/colorDelay); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

void confettiModeDisplay(){
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);

    // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/colorDelay); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

void SetupPurpleAndGreenPalette()
{
    CRGB purple = CHSV( HUE_PURPLE, 255, 255);
    CRGB green  = CHSV( HUE_GREEN, 255, 255);
    CRGB black  = CRGB::Black;
    
    currentPalette = CRGBPalette16(
                                   green,  green,  black,  black,
                                   purple, purple, black,  black,
                                   green,  green,  black,  black,
                                   purple, purple, black,  black );
}

void showPallete(){    
    static uint8_t startIndex = 0;
    startIndex = startIndex + 1; /* motion speed */
    
    FillLEDsFromPaletteColors( startIndex);
    
    FastLED.show();
    FastLED.delay(1000 / colorDelay);
}



void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
    uint8_t brightness = 255;
    
    for( int i = 0; i < NUM_LEDS; i++) {
        //if(i!=24){
          leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);  
        //}
        colorIndex += 3;
    }
}

void handleBrightness(){
  if(server.argName(0) != NULL && server.argName(0) == "brightness"){
      String argument = server.arg(0);
      int newBrightness = argument.toInt();
      if (newBrightness > 40){
        newBrightness = 40;
      }
      else if(newBrightness < 1){
        newBrightness = 1;
      }
      FastLED.setBrightness(  newBrightness );
    }
}

void handleColorDelay(){
  if(server.argName(0) != NULL && server.argName(0) == "delay"){
      String argument = server.arg(0);
      int tempDelay = argument.toInt();
      if (tempDelay > 1000){
        colorDelay = 1000;
      }
      else if(tempDelay < 1){
        colorDelay = 1;
      }
      else {
        colorDelay = tempDelay;
      }
    }
    else{
      colorDelay = 100;   
    }
}

void resetAverageValues(){
   //bootstrap average with some low values
  for (int i = 0; i < AVGLEN; i++) {  
    insert(250, avgs, AVGLEN);
  }

  //Initial values
  high.times = 0;
  high.times_start = millis();
  Color.r = 0;  
  Color.g = 0;
  Color.b = 1;
}

void handleReactive(){
  off_led();
  resetAverageValues();
  FastLED.setBrightness(3);
  currentMode=reactive_sound;
}

void reactiveSoundDisplay(){
  switch(mode) {
    case 0:
      visualize_music();
      break;
    default:
      break;
  }
    delay(DELAY);       // delay in between reads for stability
}

void off_led(){
    randomMode=false;
    randomChMode=false;
    fill_solid( leds, NUM_LEDS, CRGB(0,0,0));
    FastLED.show();
}

void handleRgbLoop(){
  off_led();
  currentMode=rgb_loop;
}

void handleGrChristmassMode(){
  off_led();
  currentMode=gr_christmass_mode;
}

void handleHalfGrChristmassMode(){
  off_led();
  currentMode=half_gr_christmass_mode;
}

void handleStrobe(){
  off_led();
  currentMode=strobe;
}

void handleTheaterRun(){
  off_led();
  currentMode=theaterRun;
}

void handleSnowSparkleChristmass(){
  off_led();
  currentMode=snow_sparkle_christmass;
}

void handleSnowSparkle(){
  off_led();
  currentMode=snow_sparkle;
}

void handleFire(){
  off_led();
  currentMode=fire;
}

void handleBouncingBalls(){
  off_led();
  setNewBallColor = true;
  readRandomBallColors();
  currentMode=bouncing_balls;
}

void readRandomBallColors(){
    randomBallColors = false;
    if(server.argName(0) != NULL && server.argName(0) == "random"){
      String argument = server.arg(0);
      if(argument.equals("true")){
        randomBallColors = true;
      }
    }
}

void handleMeteorRain(){
  off_led();
  currentMode=meteor_rain;
}

void handleRandom(){
  off_led();
  randomMode=true;
  setRandomMode();
}

void handleRandomChristmass(){
  off_led();
  randomChMode=true;
  setRandomChMode();
}
void BouncingColoredBalls(byte colors[][3]) {
    delay(0);
    for (int i = 0 ; i < BallCount ; i++) {
      delay(0);
      TimeSinceLastBounce[i] =  millis() - ClockTimeSinceLastBounce[i];
      Height[i] = 0.5 * Gravity * pow( TimeSinceLastBounce[i]/1000 , 2.0 ) + ImpactVelocity[i] * TimeSinceLastBounce[i]/1000;
 
      if ( Height[i] < 0 ) {                      
        Height[i] = 0;
        ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
        ClockTimeSinceLastBounce[i] = millis();
 
        if ( ImpactVelocity[i] < 0.01 ) {
          ImpactVelocity[i] = ImpactVelocityStart;
        }
      }
      Position[i] = round( Height[i] * (NUM_LEDS - 1) / StartHeight);
    }
    
    for (int i = 0 ; i < BallCount ; i++) {
      delay(0);
      setPixel(Position[i],colors[i][0],colors[i][1],colors[i][2]);
    }
   
    showStrip();
    setAll(0,0,0);
  
}

void Strobe(byte red, byte green, byte blue, int StrobeCount, int FlashDelay){
  for(int j = 0; j < StrobeCount; j++) {
    setAll(red,green,blue);
    showStrip();
    delay(FlashDelay);
    setAll(0,0,0);
    showStrip();
    delay(FlashDelay);
  }
}

void theaterChase(byte red, byte green, byte blue, int SpeedDelay) {
  for (int j=0; j<2; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      int c = 1;   
      for (int i=0; i < NUM_LEDS; i=i+3) {
        int r = 0;
        int g = 0;
        int b = 0;
        if(j==1){
          r = 0;
          g = 255;
          b = 0;
          if(c%2==0){
            r=255;
            g=0;
          } 
        }
        else{
          r = 255;
          g = 0;
          b = 0;
          if(c%2==0){
            r=0;
            g=255;
          } 
        }

        setPixel(i+q, r, g, b);    //turn every third pixel on
        c++;
      }
      showStrip();
     
      delay(SpeedDelay);
      for (int i=0; i < NUM_LEDS; i=i+3) {
        setPixel(i+q, 0,0,0);     //turn every third pixel off     
      }
      
    }
  }
}

void meteorRain(byte red, byte green, byte blue, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {  
  setAll(0,0,0);
 
  for(int i = 0; i < NUM_LEDS+NUM_LEDS; i++) {
   
   
    // fade brightness all LEDs one step
    for(int j=0; j<NUM_LEDS; j++) {
      if( (!meteorRandomDecay) || (random(10)>5) ) {
        fadeToBlack(j, meteorTrailDecay );        
      }
    }
   
    // draw meteor
    for(int j = 0; j < meteorSize; j++) {
      if( ( i-j <NUM_LEDS) && (i-j>=0) ) {
        setPixel(i-j, red, green, blue);
      }
    }
   
    showStrip();
    delay(SpeedDelay);
  }
}

void fadeToBlack(int ledNo, byte fadeValue) {
 #ifdef ADAFRUIT_NEOPIXEL_H
    // NeoPixel
    uint32_t oldColor;
    uint8_t r, g, b;
    int value;
   
    oldColor = strip.getPixelColor(ledNo);
    r = (oldColor & 0x00ff0000UL) >> 16;
    g = (oldColor & 0x0000ff00UL) >> 8;
    b = (oldColor & 0x000000ffUL);

    r=(r<=10)? 0 : (int) r-(r*fadeValue/256);
    g=(g<=10)? 0 : (int) g-(g*fadeValue/256);
    b=(b<=10)? 0 : (int) b-(b*fadeValue/256);
   
    strip.setPixelColor(ledNo, r,g,b);
 #endif
 #ifndef ADAFRUIT_NEOPIXEL_H
   // FastLED
   leds[ledNo].fadeToBlackBy( fadeValue );
 #endif  
}

void Fire(int Cooling, int Sparking, int SpeedDelay) {
  static byte heat[NUM_LEDS];
  int cooldown;
 
  // Step 1.  Cool down every cell a little
  for( int i = 0; i < NUM_LEDS; i++) {
    cooldown = random(0, ((Cooling * 10) / NUM_LEDS) + 2);
   
    if(cooldown>heat[i]) {
      heat[i]=0;
    } else {
      heat[i]=heat[i]-cooldown;
    }
  }
 
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }
   
  // Step 3.  Randomly ignite new 'sparks' near the bottom
  if( random(255) < Sparking ) {
    int y = random(7);
    heat[y] = heat[y] + random(160,255);
    //heat[y] = random(160,255);
  }

  // Step 4.  Convert heat to LED colors
  for( int j = 0; j < NUM_LEDS; j++) {
    setPixelHeatColor(j, heat[j] );
  }

  showStrip();
  delay(SpeedDelay);
}

void setPixelHeatColor (int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature/255.0)*191);
 
  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252
 
  // figure out which third of the spectrum we're in:
  if( t192 > 0x80) {                     // hottest
    setPixel(Pixel, 255,255,heatramp );
  } else if( t192 > 0x40 ) {             // middle
    setPixel(Pixel, heatramp, 255, 0);
  } else {                               // coolest
    setPixel(Pixel, 0, heatramp, 0);
  }
}

void SnowSparkle(byte red, byte green, byte blue, int SparkleDelay, int SpeedDelay, int sparkleRed, int sparkleGreen, int sparkleBlue, boolean sparkle) {
  setAll(red,green,blue);

  int Pixel = random(NUM_LEDS);
  if(sparkle){
    setPixel(Pixel,sparkleRed,sparkleGreen,sparkleBlue);
  }
  else{
    setPixel(Pixel,0xff,0xff,0xff);
  }
  showStrip();
  delay(SparkleDelay);
  setPixel(Pixel,red,green,blue);
  showStrip();
  delay(SpeedDelay);
}

void RGBLoop(){

    // Fade IN
    for(int k = 0; k < 256; k++) {
      switch(rgb_loop_state) {
        case 0: setAll(k,0,0); break;
        case 1: setAll(0,k,0); break;
        case 2: setAll(0,0,k); break;
      }
      showStrip();
      delay(3);
    }
    // Fade OUT
    for(int k = 255; k >= 0; k--) {
      switch(rgb_loop_state) {
        case 0: setAll(k,0,0); break;
        case 1: setAll(0,k,0); break;
        case 2: setAll(0,0,k); break;
      }
      showStrip();
      delay(3);
    }
  if(rgb_loop_state < 2){
    rgb_loop_state++;
  }
  else{
    rgb_loop_state = 0;
  }
}

void RGChrismtassLed(){

    // Fade IN
    for(int k = 0; k < 256; k++) {
      switch(gr_christmass_loop_state) {
        case 0: setAll(k,0,0); break;
        case 1: setAll(0,k,0); break;;
      }
      showStrip();
      delay(3);
    }
    // Fade OUT
    for(int k = 255; k >= 0; k--) {
      switch(gr_christmass_loop_state) {
        case 0: setAll(k,0,0); break;
        case 1: setAll(0,k,0); break;;
      }
      showStrip();
      delay(3);
    }
  if(gr_christmass_loop_state < 1){
    gr_christmass_loop_state++;
  }
  else{
    gr_christmass_loop_state = 0;
  }
}

void HalfRGChrismtassLed(){
    int firstLed = 0;
    int halfLed = (NUM_LEDS / 2) - 1;
    int lastLed = NUM_LEDS;
    // Fade IN
    for(int k = 0; k < 256; k++) {
      switch(gr_christmass_loop_state) {
        case 0: setLedsFromTo(k,0,0, firstLed, halfLed); setLedsFromTo(0,k,0,halfLed+1,lastLed); break;
        case 1: setLedsFromTo(0,k,0, firstLed, halfLed); setLedsFromTo(k,0,0,halfLed+1,lastLed); break;
      }
      showStrip();
      delay(3);
    }
    // Fade OUT
    for(int k = 255; k >= 0; k--) {
      switch(gr_christmass_loop_state) {
        case 0: setLedsFromTo(k,0,0, firstLed, halfLed); setLedsFromTo(0,k,0,halfLed+1,lastLed); break;
        case 1: setLedsFromTo(0,k,0, firstLed, halfLed); setLedsFromTo(k,0,0,halfLed+1,lastLed); break;
      }
      showStrip();
      delay(3);
    }
  if(gr_christmass_loop_state < 1){
    gr_christmass_loop_state++;
  }
  else{
    gr_christmass_loop_state = 0;
  }
}

void showStrip() {
 #ifdef ADAFRUIT_NEOPIXEL_H
   // NeoPixel
   strip.show();
 #endif
 #ifndef ADAFRUIT_NEOPIXEL_H
   // FastLED
   FastLED.show();
 #endif
}

void setPixel(int Pixel, byte red, byte green, byte blue) {
 #ifdef ADAFRUIT_NEOPIXEL_H
   // NeoPixel
   strip.setPixelColor(Pixel, strip.Color(red, green, blue));
 #endif
 #ifndef ADAFRUIT_NEOPIXEL_H
   // FastLED
   leds[Pixel].r = red;
   leds[Pixel].g = green;
   leds[Pixel].b = blue;
 #endif
}

void setAll(byte red, byte green, byte blue) {
  for(int i = 0; i < NUM_LEDS; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}

void setLedsFromTo(byte red, byte green, byte blue, int firstLed, int lastLed){
  for(int i = firstLed; i < lastLed; i++ ) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}

void showRandomProjection(){
  randomMode = true;
}

void setRandomMode(){
  int len = sizeof(randomLedModes) / sizeof(randomLedModes[0]);
  int random_projection = random(0,len);
  currentMode = randomLedModes[random_projection];
}

void setRandomChMode(){
  currentMode = randomChLedModes[chrismtass_mode];
  int arraySize = sizeof(randomChLedModes) / sizeof(randomChLedModes[0]);
  if((arraySize-1)==chrismtass_mode){
    chrismtass_mode = 0;
  }
  else{
    chrismtass_mode++;
  }
}
 
void setup(){
  delay(500);

  FastLED.addLeds<LED_TYPE, PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );
  fill_solid( leds, NUM_LEDS, CRGB(0,0,0));
  FastLED.show();
  
  delay(2000);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.config(staticIP,gateway,subnet);
  WiFi.begin(ssid, password);



  int i = 0;
  connection_established = true;
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    leds[i] = CRGB::Blue;
    FastLED.show();
    delay(500);
    Serial.print(".");
    if(i==NUM_LEDS){
      connection_established = false;
      break;
    }
    i++;
  }
  if(connection_established){
    fill_solid( leds, NUM_LEDS, CRGB::Red);
    FastLED.show();
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
   
    if (MDNS.begin("esp8266")) {
      Serial.println("MDNS responder started");
    }
  
    server.on("/color", handleColor);
    server.on("/solid-color", handleSolidColor);
    server.on("/rainbow", handleRainbowPallete);
    server.on("/rainbow-strip", handleRainbowStripPallete);
    server.on("/confetti", handleConfettiMode);
    server.on("/juggle", handleJuggleMode);
    server.on("/color-delay", handleColorDelay);
    server.on("/brightness", handleBrightness);
    server.on("/reactive", handleReactive);
    server.on("/rgb-loop", handleRgbLoop);
    server.on("/strobe", handleStrobe);
    server.on("/snow-sparkle", handleSnowSparkle);
    server.on("/fire", handleFire);
    server.on("/bouncing-balls", handleBouncingBalls);
    server.on("/meteor-rain", handleMeteorRain);
    server.on("/random", handleRandom);
    server.on("/rb-christmass", handleGrChristmassMode);
    server.on("/half-rb-christmass", handleHalfGrChristmassMode);
    server.on("/theater-run", handleTheaterRun);
    server.on("/snow-sparkle-christmass", handleSnowSparkleChristmass);
    server.on("/random-christmass", handleRandomChristmass);
    server.on("/off", handleOff);
  
    server.begin();
    Serial.println("HTTP server started");
    currentPalette = CloudColors_p; 
    resetAverageValues();
    
    ImpactVelocityStart = sqrt( -2 * Gravity * StartHeight );
    for (int i = 0 ; i < BallCount ; i++) {  
      ClockTimeSinceLastBounce[i] = millis();
      Height[i] = StartHeight;
      Position[i] = 0;
      ImpactVelocity[i] = ImpactVelocityStart;
      TimeSinceLastBounce[i] = 0;
      Dampening[i] = 0.90 - float(i)/pow(BallCount,2);
    }
  
    delay(1000);
  }
  else{
    fill_solid( leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(1000);
    currentMode = rainbow_strip_pallete;
    //randomMode=true;
  } 
}

void loop(){
  if(connection_established){
    server.handleClient();
  }
   
   if(currentMode==moving_color){
        movingColorDisplay();
   }
   else if(currentMode==reactive_sound){
        reactiveSoundDisplay();
   }
   else if(currentMode==solid_color){
        solidColorDisplay();
   }
   else if(currentMode==rainbow_pallete){
        rainbowPalleteDisplay();
   }
   else if(currentMode==rainbow_strip_pallete){
        rainbowPalleteStripDisplay();
   }
   else if(currentMode==juggle_mode){
        juggleModeDisplay();
   }
   else if(currentMode==confetti_mode){
        confettiModeDisplay();
   }
   else if(currentMode==rgb_loop){
       RGBLoop();
   }
   else if(currentMode==strobe){
      Strobe(0xff, 0xff, 0xff, 15, 65);
      delay(1000);
   }
   else if(currentMode==snow_sparkle){
      SnowSparkle(0x10, 0x10, 0x10, 20, random(100,1000),NULL,NULL,NULL,false);
   }
   else if(currentMode==gr_christmass_mode){
      RGChrismtassLed();
   }
   else if(currentMode==half_gr_christmass_mode){
      HalfRGChrismtassLed();
   }
   else if(currentMode==fire){
      Fire(55,105,55);
   }
   else if(currentMode==theaterRun){
      theaterChase(0xff,0,0,80);
   }
   else if(currentMode==snow_sparkle_christmass){
     FastLED.setBrightness(8);
     SnowSparkle(0, 255, 0, 20, random(100,1000),255,0,0,true);
   }
   else if(currentMode==bouncing_balls){
      if(setNewBallColor == true){
        if(randomBallColors == false){
          ball1 = (Ball){0,255,0};
          ball2 = (Ball){255,0,0};
          ball3 = (Ball){0,0,255};
        }
        else{
          ball1 = (Ball){random(0,255),random(0,255),random(0,255)};
          ball2 = (Ball){random(0,255),random(0,255),random(0,255)};
          ball3 = (Ball){random(0,255),random(0,255),random(0,255)};
        }
        setNewBallColor = false;
      }
      byte colors[3][3] = { {ball1.r, ball1.g,ball1.b},
                {ball2.r, ball2.g,ball2.b},
                {ball3.r, ball3.g,ball3.b} };

      BouncingColoredBalls(colors);
   }
   else if(currentMode==meteor_rain){
        meteorRain(0xff,0xff,0xff,6, 80, true, 50);
   }

  if(randomMode){
    unsigned long currentTime = millis();
    if (currentTime - oldTime > change_mode_time){
        setRandomMode();
        oldTime = currentTime;
    }
   }
   
   if(randomChMode){
    unsigned long currentTime = millis();
    if (currentTime - oldChTime > change_ch_mode_time){
        setRandomChMode();
        oldChTime = currentTime;
    }
   }
  if(connection_established){
   MDNS.update(); 
  }
    
}
