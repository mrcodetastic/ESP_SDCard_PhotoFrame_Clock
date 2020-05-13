// This example if for an ESP8266 or ESP32, it renders a Jpeg file
// that is stored in a SD card file. The test image is in the sketch
// "data" folder (press Ctrl+K to see it). You must save the image
// to the SD card using you PC.

// ---------------------------
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// Include the jpeg decoder library
#include <TJpg_Decoder.h>

// Include SD
#define FS_NO_GLOBALS
#include <FS.h>
#ifdef ESP32
  #include "SPIFFS.h" // ESP32 only
#endif

// Include the TFT library https://github.com/Bodmer/TFT_eSPI
#include <Adafruit_GFX.h>    // Core graphics library
#include <Fonts/FreeSansBold18pt7b.h> // Font
#include <Fonts/FreeSans9pt7b.h> // Font
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include "SPI.h"

// ------------ CONFIGURATION ------------

const char* ssid     = "XXX;
const char* password = "YYY";

const unsigned long gmt_offset = 60*60*1; // HACK to add X hours for timezone - BST

const char* sd_card_photo_directory = "/output/";

// ------------ ------------- ------------


// SD Card Select Pin
#define SD_CS     D4

// For the breakout, you can use any 2 or 3 pins
// For the breakout, you can use any 2 or 3 pins
// These pins will also work for the 1.8" TFT shield
#define TFT_CS     D1
#define TFT_RST    -1  // you can also connect this to the Arduino reset in which case, set this #define pin to -1!
#define TFT_DC     D2

// For 1.44" and 1.8" TFT with ST7735 use
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

// Other classes
WiFiUDP       ntpUDP;
NTPClient     timeClient(ntpUDP);
File          root;
String        current_image = "                                       ";
int           prev_minute = 0;
unsigned long last_img_change =  0;
unsigned long last_tft_update =  0;
char          temp_buffer[64];
String        temp_string   = "                                       ";             


// This next function will be called during decoding of the jpeg file to
// render each block to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  //tft.pushImage(x, y, w, h, bitmap);

  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
   tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}


void setup()
{
  Serial.begin(115200);


  // Initialise SD before TFT
  if (!SD.begin(SD_CS)) {
    Serial.println(F("SD.begin failed!"));
    while (1) delay(0);
  }
  Serial.println("\r\nSD Initialisation done.");

  // Use this initializer if you're using a 1.8" TFT
  tft.initR(INITR_BLACKTAB);   // initialize a ST7735S chip, black tab

  // Set the rotation to what the device is at
  tft.setRotation(3);
  
    
  uint16_t time = millis();
  tft.fillScreen(ST77XX_BLACK);
  time = millis() - time;
  Serial.println(time, DEC);
  delay(500);

  // large block of text
  tft.fillScreen(ST77XX_BLACK);
  testdrawtext("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur adipiscing ante sed nibh tincidunt feugiat. Maecenas enim massa, fringilla sed malesuada et, malesuada sit amet turpis. Sed porttitor neque ut ante pretium vitae malesuada nunc bibendum. Nullam aliquet ultrices massa eu hendrerit. Ut sed nisi lorem. In vestibulum purus a tortor imperdiet posuere. ", ST77XX_WHITE);
  delay(1000);


  // COnnect to network
  // We start by connecting to a WiFi network
  tft.fillScreen(ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(0,0);
  tft.println("Connecting to " + String(ssid));

  /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
     would try to act as both a client and an access-point and could cause
     network-issues with your other WiFi-devices on your WiFi-network. */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);


  // Wait until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }

  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(0);
  tft.println("");
  tft.println("");
  tft.println("WiFi connected");
  tft.println("IP address: ");
  tft.println(WiFi.localIP());

  tft.setTextColor(ST77XX_YELLOW);
  tft.println("Starting NTP Client");
  timeClient.begin();
  timeClient.update();  
  tft.println(timeClient.getFormattedTime());
  setTime(timeClient.getEpochTime());
  //tft.println(hourFormat12());

  //TODO: HACK - ADD for BST
  adjustTime(gmt_offset); 
  

  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);

  tft.fillScreen(ST77XX_BLACK);  
    
}


void loop()
{
  tft.setCursor(0,0);

  
  
  if (prev_minute == minute() )
  {
    Serial.println("It's the same minute... skipping");
    Serial.print("Hour: "); Serial.print(hour(), DEC);
    Serial.print(" Minute: "); Serial.print(minute(), DEC);
    Serial.println("");
    delay(5000);
    return;
  }
  
  prev_minute = minute();

  if (  hour() > 22 || (hour() >= 0 && hour() < 7) ) // Configure these thresholds as you see fit
  {
    
       Serial.println("NIGHT MODE ENABLED...");
       tft.setTextColor(0x6B6D);
       tft.fillScreen(ST77XX_BLACK);
  }
  else
  {
      tft.setTextColor(ST77XX_WHITE);
    
     /*
     // CHange image every 10 hours or so       
     if ( (now() - last_img_change) > (60*60*10))
     {
        root = SD.open(sd_card_photo_directory);          
        current_image = getNextPhoto(root);    
        last_img_change = now();
     }
     */

     // CHange image every day
     if ( day() != last_img_change)
     {
        root = SD.open(sd_card_photo_directory);          
        current_image = getNextPhoto(root);    
        last_img_change = day();
     }     
    
    
    // Else show the image
    Serial.println("/output/" + current_image);
  
    // Time recorded for test purposes
    uint32_t t = millis();
  
    // Get the width and height in pixels of the jpeg if you wish
    uint16_t w = 0, h = 0;
    //TJpgDec.getSdJpgSize(&w, &h, "/IMG_3100.jpg");
    TJpgDec.getSdJpgSize(&w, &h, String(sd_card_photo_directory) + current_image);
    Serial.print("Width = "); Serial.print(w); Serial.print(", height = "); Serial.println(h);

    if (w == 0 || h == 0) last_img_change = 100; // force image change next time
  
    // Draw the image, top left at 0,0
    //TJpgDec.drawSdJpg(0, 0, "/IMG_3100.jpg");
    TJpgDec.drawSdJpg(0, 0, String(sd_card_photo_directory) + current_image);
  
    // How much time did rendering take
    t = millis() - t;
    Serial.print(t); Serial.println(" ms to decode jpeg");
  }

   tft.setTextColor(ST77XX_WHITE);  

   // TIME               
   memset( temp_buffer, '\0', sizeof(char)*32 );
    
   tft.setFont(&FreeSansBold18pt7b);
   sprintf( temp_buffer, "%02d:%02d", (int)hour(), (int)minute() );
      
   tft.setCursor(14, tft.height()/2);
   //drawCentreString(temp_buffer, tft.width()/2, tft.height()/2);   
   tft.print(temp_buffer);
   
   // DAY
   memset( temp_buffer, '\0', sizeof(char)*32 );
   tft.setFont(&FreeSans9pt7b);

   temp_string = String(dayStr(weekday()));
   temp_string.toCharArray(temp_buffer,32);
   tft.setCursor(16, (tft.height()/2)+ 26);
   tft.print(temp_buffer);

   // DATE
   memset( temp_buffer, '\0', sizeof(char)*32 );
   tft.setFont(&FreeSans9pt7b);

   temp_string = String(day()) + " " + String(monthShortStr(month()));
   temp_string.toCharArray(temp_buffer,32);
   tft.setCursor(14, (tft.height()/2)+ 23*2);
   tft.setTextColor(0xDEFB);  
   tft.print(temp_buffer);
   
      
   // drawCentreString(temp_buffer, tft.width()/2, tft.height()/2);
   // scrolltext(0, 30, "David Prentice is a jolly good chap.  1000 pixels/s.  System Font",            1, NULL, 2);

   // printDirectory(root, 0);

   //Serial.print("the next filename would be: ");
   //nextfile = getNextPhoto(root);

   last_tft_update = now();
}

int current_photo_dir_index = 0;
String getNextPhoto(File dir) {

  String firstfile = "                                                          ";
  int counter = 0;
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    
  if (entry.isDirectory()) {
      // Serial.println("/");
      //    printDirectory(entry, numTabs + 1);
      Serial.println("Skipping directory");
    } else {

      if (counter == 0)
      {
        firstfile = entry.name();
      }
      
      counter++;
      // files have sizes, directories do not
      //Serial.print("\t\t");
      //Serial.print(entry.size(), DEC);

      Serial.print("...");
      Serial.print(counter, DEC);
      if (counter > current_photo_dir_index)
      {      
        current_photo_dir_index = counter;
        Serial.print("Selecting file: ");
        Serial.println(entry.name());
        return String(entry.name());
      }

    } // file
    entry.close();
  }

  Serial.println("We Exhausted all the available files!");
    current_photo_dir_index = 0;
    return firstfile;
}


void testdrawtext(char *text, uint16_t color) {
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}


/*
 * 
 * // NOT USED

// https://forum.arduino.cc/index.php?topic=642749.0
void drawCentreString(const char *buf, int x, int y)
{
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    tft.setCursor(x - w / 2, y);
    tft.print(buf);
}

void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.print(entry.size(), DEC);
      time_t cr = entry.getCreationTime();
      time_t lw = entry.getLastWrite();
      struct tm * tmstruct = localtime(&cr);
      Serial.printf("\tCREATION: %d-%02d-%02d %02d:%02d:%02d", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
      tmstruct = localtime(&lw);
      Serial.printf("\tLAST WRITE: %d-%02d-%02d %02d:%02d:%02d\n", (tmstruct->tm_year) + 1900, (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
    }
    entry.close();
  }
}




// https://forum.arduino.cc/index.php?topic=483635.0
// Not used

void scrolltext(int x, int y, const char *s, uint8_t dw = 1, const GFXfont *f = NULL, int sz = 1)
{
    int16_t x1, y1, wid = tft.width(), inview = 1;
    uint16_t w, h;
    tft.setFont(f);
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.setTextSize(sz);
    tft.setTextWrap(false);
    tft.getTextBounds((char*)s, x, y, &x1, &y1, &w, &h);
    //    w = strlen(s) * 6 * sz;

    for (int steps = wid + w; steps >= 0; steps -= dw) {
        x = steps - w;
        if (f != NULL) {
            inview = wid - x;
            if (inview > wid) inview = wid;
            if (inview > w) inview = w;
            tft.fillRect(x > 0 ? x : 0, y1, inview + dw, h, ST77XX_BLACK);
        }
        x -= dw;
        tft.setCursor(x, y);
        tft.print(s);
        if (f == NULL) tft.print("  "); //rubout trailing chars
        delay(25);
    }
}
*/
