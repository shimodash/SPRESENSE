
//#include <LovyanGFX.hpp>
#include "LGFX_SPRESENSE_sample.hpp"

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <bluetooth/ble_gatt.h>


extern const uint16_t image_0[],image_1[],img_qr[];

static constexpr int image_width = 240;
static constexpr int image_height = 320;
//----------------------------------------------------------------------------

static uint8_t ble_recive_data = 0x0; //update on ble_peripheral


static LGFX lcd;

void setup(void)
{
  lcd.init();
  lcd.startWrite();
  setup_ble(); // from ble_peripheral
}

void loop(void)
{

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
//  lcd.clear(TFT_DARKGREY);
  lcd.setColorDepth(16);  // LCDを16bitカラーモードに設定する。
  lcd.setSwapBytes(true); // バイト順変換を有効にする。
  int len = image_width * image_height;

  // 画像の幅と高さをsetAddrWindowで事前に設定し、writePixelsで描画します。
  lcd.setAddrWindow(0, 0, image_width, image_height);         // 描画範囲を設定。

  if(ble_recive_data==0x10){
    //display QR code
    lcd.pushImage((240-216)/2, (320-216)/2, 216, 216, img_qr);
  }else if(ble_recive_data==0x20){
    lcd.pushImage(0, 0, image_width, image_height, image_1);
  }else{
    lcd.pushImage(0, 0, image_width, image_height, image_0);
    }

  loop2_ble(); // from ble_peripheral
}
