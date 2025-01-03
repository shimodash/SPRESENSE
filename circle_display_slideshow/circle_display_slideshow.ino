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

static constexpr int image_width = 33;
static constexpr int image_height = 31;
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
    ; /* wait until SD card is mounted. */
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

/*
  画像データを描画する関数は幾つか種類があります。

方法１．事前に描画範囲を設定しておき、次にデータの長さを指定して描画する方法
方法２．描画する座標と幅・高さを指定してデータを描画する方法



方法１．事前に描画範囲を設定しておき、次にデータの長さを指定して描画する方法

この方法では、setWindow/setAddrWindow関数で描画範囲を設定したあと、
writePixels/pushPixels関数で画像データの長さを指定して描画します。

  setWindow( x0, y0, x1, y1 );   // 描画範囲の指定。左上座標と右下座標を指定します。
  setAddrWindow( x, y, w, h );   // 描画範囲の指定。左上座標と幅と高さを指定します。

setWindow は画面外の座標を指定した場合の動作は保証されません。
setAddrWindow は描画範囲外が指定された場合は範囲内に調整されます。
 ※ ただし自動調整された結果、実際に設定される幅や高さが指定した値より小さくなる可能性があるので注意が必要です。

  writePixels   ( *data, len, swap );  // 画像を描画する。(事前にstartWrite、事後にendWriteが必要）
  pushPixels    ( *data, len, swap );  // 画像を描画する。(startWrite・endWriteは不要）

 ※ writePixelsはAdafruitGFX由来の関数で、pushPixelsはTFT_eSPI由来の関数です。
    描画内容は同等ですが、startWrite/endWriteが自動で行われるか否かが違います。

第１引数：画像データのポインタ（データ型に応じて色の形式を判断して変換が行われます。）
第２引数：画像データのピクセル数（バイト数でない点に注意。）
第３引数：バイト順変換フラグ（省略時は事前にsetSwapBytes関数で設定した値が使用されます。）

第１引数のdataの型に基づいて色の形式変換が行われます。
  uint8_t*  の場合、 8bitカラー RGB332として扱います。
  uint16_t* の場合、16bitカラー RGB565として扱います。
  void*     の場合、24bitカラー RGB888として扱います。
 ※ （３バイトのプリミティブ型が無いため、void*型を24bitカラー扱いとしています）

 ※ LCDに描画する際に、LCDの色数モードに応じて色形式の変換が自動的に行われます。
*/
  lcd.clear(TFT_DARKGREY);
  lcd.setColorDepth(16);  // LCDを16bitカラーモードに設定する。
  lcd.setSwapBytes(true); // バイト順変換を有効にする。
  int len = image_width * image_height;

/*
  // 画像の幅と高さをsetAddrWindowで事前に設定し、writePixelsで描画します。
  lcd.setAddrWindow(0, 0, image_width, image_height);         // 描画範囲を設定。
  lcd.writePixels((uint16_t*)rgb565, len); // RGB565の16bit画像データを描画。

  // データとバイト順変換の指定が一致していない場合、色化けします。
  lcd.setAddrWindow(0, 40, image_width, image_height);
  // 第3引数でfalseを指定することでバイト順変換の有無を指定できます。
  lcd.writePixels((uint16_t*)rgb565, len, false); // RGB565の画像をバイト順変換無しで描画すると色が化ける。

  // 描画範囲が画面外にはみ出すなどして画像の幅や高さと合わなくなった場合、描画結果が崩れます。
  lcd.setAddrWindow(-1, 80, image_width, image_height); // X座標が-1（画面外）のため、正しく設定できない。
  lcd.writePixels((uint16_t*)rgb565, len); // 描画先の幅と画像の幅が不一致のため描画内容が崩れる。

  // データと型が一致していない場合も、描画結果が崩れます。
  lcd.setAddrWindow(0, 120, image_width, image_height);
  // RGB565のデータをわざとuint8_tにキャストし、RGB332の8bitカラーとして扱わせる。
  lcd.writePixels((uint8_t*)rgb565, len);  // 画像の形式と型が一致していないため描画が乱れる。

  // データと型が一致していれば、描画先の色数に合わせて適切な形式変換が行われます。
  lcd.setAddrWindow(0, 160, image_width, image_height);
  lcd.writePixels((uint8_t*)rgb332, len);  // RGB332のデータでも16bitカラーのLCDに正しく描画できる。


// ※ LCDへの画像データの送信は、メモリの若いアドレスにあるデータから順に1Byte単位で送信されます。
//    このため、例えばRGB565の16bit型のデータを素直にuint16_tの配列で用意すると、送信の都合としてはバイト順が入れ替わった状態になります。
//    この場合は事前にsetSwapBytes(true)を使用したり、第３引数にtrueを指定する事で、バイト順の変換が行われて正常に描画できます。
//    なお用意する画像データを予め上位下位バイトを入れ替えた状態で作成すれば、この変換は不要になり速度面で有利になります。

  lcd.setAddrWindow(40,  0, image_width, image_height);
  lcd.writePixels((uint16_t*)swap565, len, false); // 予め上位下位が入れ替わった16bitデータの場合はバイト順変換を無効にする。

  lcd.setAddrWindow(40, 40, image_width, image_height);
  lcd.writePixels((uint16_t*)swap565, len, true);  // 逆に、予め上位下位が入れ替わったデータにバイト順変換を行うと色が化ける。

  lcd.setAddrWindow(40, 80, image_width, image_height);
  lcd.writePixels((void*)rgb888, len, true);  // 24bitのデータも同様に、RGB888の青が下位側にあるデータはバイト順変換が必要。

  lcd.setAddrWindow(40, 120, image_width, image_height);
  lcd.writePixels((void*)bgr888, len, false);  // 同様に、BGR888の赤が下位側にあるデータはバイト順変換は不要。

  lcd.setAddrWindow(40, 160, image_width, image_height);
  lcd.writePixels((void*)bgr888, len, true);  // 設定を誤ると、色が化ける。（赤と青が入れ替わる）

  lcd.display();
  delay(4000);
  lcd.clear(TFT_DARKGREY);
*/
/*
方法２．描画する座標と幅・高さを指定してデータを描画する方法

この方法では、pushImage関数を用いて描画範囲と描画データを指定して描画します。

  pushImage( x, y, w, h, *data);                  // 指定された座標に画像を描画する。

方法１と違い、画面外にはみ出す座標を指定しても描画が乱れることはありません。（はみ出した部分は描画されません。）
方法１と違い、バイト順の変換を指定する引数が無いため、事前にsetSwapBytesによる設定が必要です。
なお方法１と同様に、dataの型に応じて色変換が行われます。
*/

  lcd.setSwapBytes(true); // バイト順変換を有効にする。

  // 描画先の座標と画像の幅・高さを指定して画像データを描画します。
//  lcd.pushImage(   0, 0, image_width, image_height, (uint16_t*)rgb565); // RGB565の16bit画像データを描画。
  // lcd.pushImage(   0, 0, 240, 240, (uint16_t*)sunrise); // RGB565の16bit画像データを描画。
  // lcd.drawPngFile(SD, "spresense_logo.png", 0, 0);
  // String fileName = String(myFile.name());
  String fileName = myFile.name();
  fileName.replace("/mnt/sd0", "");
  lcd.drawPngFile(SD, fileName.c_str(), 0, 0);
  
  // lcd.drawPngFile(SD, myFile.name(), 0, 0);
  // lcd.drawPngFile(SD, String(myFile.name()).c_str(), 0, 0);

  delay(1000);
// 入力端子の実装がなければ以下のループを実施
  while(true){
      myFile = dir.openNextFile();
      if (!myFile) dir.rewindDirectory();

      String fileName = myFile.name();
      Serial.println("Attempting to open file: " + String(myFile.name()));      
      if (myFile.isDirectory()) {
      }else{
        if (strstr(myFile.name(), ".png")) {
          fileName.replace("/mnt/sd0", "");
          lcd.drawPngFile(SD, fileName.c_str(), 0, 0);
          delay(1000);
          if(digitalRead(2)== 0){ // Spresenseのロゴの表示後にpin2が押されていたらループを抜ける
          break;
          delay(1000);
          }
        }
    // lcd.drawPngFile(SD, "snake_240x240.png", 0, 0);
    // lcd.drawPngFile(SD, "kansya_240.png", 0, 0);
    // delay(1000);
    // lcd.drawPngFile(SD, "2025.png", 0, 0);
    // delay(1000);
    // lcd.drawPngFile(SD, "spresense_logo.png", 0, 0);
    // delay(1000);
      }
  }

  while(true){
//  lcd.pushImage(   0, 0, 240, 240, (uint16_t*)kansya); // RGB565の16bit画像データを描画。
//  delay(1000);

  
      digitalWrite(LED0, digitalRead(2));
      delay(100);
      digitalWrite(LED1, digitalRead(3));
      delay(100);
      digitalWrite(LED2, digitalRead(4));
    if(digitalRead(2)== 0){
      // lcd.pushImage(   0, 0, 240, 240, (uint16_t*)myms); // RGB565の16bit画像データを描画。
    lcd.drawPngFile(SD, "snake_240x240.png", 0, 0);
    }

    if(digitalRead(3)== 0){
      // lcd.pushImage(   0, 0, 240, 240, (uint16_t*)snake); // RGB565の16bit画像データを描画。
    lcd.drawPngFile(SD, "kansya_240.png", 0, 0);
    }

    if(digitalRead(4)== 0){
      // lcd.pushImage(   0, 0, 240, 240, (uint16_t*)y2025); // RGB565の16bit画像データを描画。
    lcd.drawPngFile(SD, "2025.png", 0, 0);

    }
  }
  
}
  
