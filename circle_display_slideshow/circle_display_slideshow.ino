#include <Arduino.h>
#include <SDHCI.h>
#include <File.h>


SDClass SD;  /**< SDClass object */ 

File myFile,dir; /**< File object */ 

#include "LGFX_SPRESENSE_sample.hpp"

extern const uint8_t rgb888[];
extern const uint8_t bgr888[];
extern const uint16_t swap565[];
extern const uint16_t rgb565[];
extern const uint8_t rgb332[];

#include "image_img.h"

//----------------------------------------------------------------------------

static LGFX lcd;

void setup(void)
{
  lcd.init();
  lcd.startWrite();
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);

//SD card init
 Serial.begin(115200);
  while (!Serial) {
    ; /* wait for serial port to connect. Needed for native USB port only */
  }

  /* Initialize SD */
  Serial.print("Insert SD card.");
   while (!SD.begin()) {
    /* wait until SD card is mounted. */
    digitalWrite(LED1, HIGH);
    delay(1000);
    digitalWrite(LED1, LOW);
    delay(1000);
  }

  // /* Create a new directory */
  // // SD.mkdir("dir/");

  // /* Open the file. Note that only one file can be open at a time,
  //    so you have to close this one before opening another. */
  // // myFile = SD.open("dir/test.txt", FILE_WRITE);

  // /* If the file opened okay, write to it */
  // if (myFile) {
  //   Serial.print("Writing to test.txt...");
  //   myFile.println("testing 1, 2, 3.");
  //   /* Close the file */
  //   myFile.close();
  //   Serial.println("done.");
  // } else {
  //   /* If the file didn't open, print an error */
  //   Serial.println("error opening test.txt");
  // }

  // /* Re-open the file for reading */
  myFile = SD.open("spresense_logo.png");
  dir = SD.open("");
  // if (myFile) {
  //   Serial.println("test.txt:");

  //   /* Read from the file until there's nothing else in it */
  //   while (myFile.available()) {
  //     Serial.write(myFile.read());
  //   }
  //   /* Close the file */
  //   myFile.close();
  // } else {
  //   /* If the file didn't open, print an error */
  //   Serial.println("error opening test.txt");
  // }
  lcd.setColorDepth(16);  // LCDを16bitカラーモードに設定する。
  lcd.setSwapBytes(true); // バイト順変換を有効にする。
  lcd.setSwapBytes(true); // バイト順変換を有効にする。

}

void loop(void)
{

    // digitalWrite(LED0, digitalRead(2));
    // delay(100);
    // digitalWrite(LED1, HIGH);
    // delay(100);
    // digitalWrite(LED2, HIGH);
    // delay(100);
    // digitalWrite(LED3, HIGH);
    // delay(1000);

    // digitalWrite(LED0, digitalRead(2));
    // delay(100);
    // digitalWrite(LED1, LOW);
    // delay(100);
    // digitalWrite(LED2, LOW);
    // delay(100);
    // digitalWrite(LED3, LOW);
    // delay(1000);

  // delay(1000);
  while(true){
      myFile = dir.openNextFile();
      // if (!myFile) dir.rewindDirectory();
      if (!myFile) break;

      Serial.println("Attempting to open file: " + String(myFile.name()));      
      if (myFile.isDirectory()) {
      }else{
        String fileName = myFile.name();

        if (strstr(myFile.name(), ".png")) {
          fileName.replace("/mnt/sd0", "");
          lcd.drawPngFile(SD, fileName.c_str(), 0, 0);
          delay(1000);
        }
      }
      myFile.close();

  }

  dir.rewindDirectory();
  
}
  
