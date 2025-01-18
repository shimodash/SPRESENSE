#include <SDHCI.h>
#define LGFX_AUTODETECT
// #include <LovyanGFX.hpp>
#include "LGFX_SPRESENSE_sample.hpp"

/*
 SDカードの直下のフォルダに
 r.txtとr.png
 g.txtとg.png
 b.txtとb.png
 を格納して下さい。
*/
SDClass SD;  /**< SDClass object */ 
File myFile; /**< File object */ 

LGFX lcd;
LGFX_Sprite canvas;

// static constexpr char text[] = "Hello world ! こんにちは世界！ this is long long string sample. 寿限無、寿限無、五劫の擦り切れ、海砂利水魚の、水行末・雲来末・風来末、喰う寝る処に住む処、藪ら柑子の藪柑子、パイポ・パイポ・パイポのシューリンガン、シューリンガンのグーリンダイ、グーリンダイのポンポコピーのポンポコナの、長久命の長助";
// static constexpr size_t textlen = sizeof(text) / sizeof(text[0]);
char text[4096] = ""; //スクロールさせる文字のファイルは4096byteまで
size_t textlen = 0;
size_t textpos = 0;

int status = 0; // R=0 G=1 B=2

void setup(void) 
{
  lcd.init();
  lcd.setRotation(3);
  lcd.drawPngFile(SD, "r.png", 0, 0);
  delay(1000);
  // 画面が横長になるように回転
  if (lcd.width() < lcd.height()) lcd.setRotation(lcd.getRotation() ^ 1);

  canvas.setColorDepth(8);
  canvas.setFont(&fonts::lgfxJapanMinchoP_32);
  canvas.setTextSize(1); 
  canvas.setTextWrap(false);        // 右端到達時のカーソル折り返しを禁止
  canvas.createSprite(lcd.width() + 36, 36); // 画面幅+１文字分の横幅を用意
  // canvas.createSprite(lcd.width() + 36*2, 36*1); // 画面幅+１文字分の横幅を用意
  Serial.begin(115200);

  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);


  myFile = SD.open("r.txt");
  size_t count = 0;
  while (myFile.available()) {
    // Serial.write(myFile.read());
    text[count] = myFile.read();
    count++;
  }
  textlen = count;
  /* Close the file */
  myFile.close();
}

void loop(void)
{
  int32_t cursor_x = canvas.getCursorX() - 1;   // 現在のカーソル位置を取得し、1ドット左に移動
  if (cursor_x <= 0) // カーソル位置が左端に到達している場合は一周したと判断
  {
    textpos = 0;            // 文字列の読取り位置をリセット
    cursor_x = lcd.width(); // 新たな文字が画面右端に描画されるようにカーソル位置を変更
  }

  canvas.setCursor(cursor_x, 0); // カーソル位置を更新
  canvas.scroll(-1, 0);          // キャンバスの内容を1ドット左にスクロール
  while (textpos < textlen && cursor_x <= lcd.width()) // 画面右端に文字が書けるか判定
  {
    canvas.print(text[textpos++]);   // 1バイトずつ出力 (マルチバイト文字でもこの処理で動作します)
    cursor_x = canvas.getCursorX();  // 出力後のカーソル位置を取得

  //button check and image txt switch
    digitalWrite(LED0, digitalRead(2));
    digitalWrite(LED1, digitalRead(3));
    digitalWrite(LED2, digitalRead(4));
    if(digitalRead(2)==LOW){
    lcd.drawPngFile(SD, "r.png", 0, 0);
    delay(1000);
      status =0;
      cursor_x = 0;
      textpos = 0;            // 文字列の読取り位置をリセット
      cursor_x = lcd.width(); // 新たな文字が画面右端に描画されるようにカーソル位置を変更
    myFile = SD.open("r.txt");
    size_t count = 0;
    while (myFile.available()) {
      text[count] = myFile.read();
      count++;
    }
    textlen = count;
  /* Close the file */
    myFile.close();      
      break;
    }
    if(digitalRead(3)==LOW){
    lcd.drawPngFile(SD, "g.png", 0, 0);
    delay(1000);
      status =1;
      cursor_x = 0;
      textpos = 0;            // 文字列の読取り位置をリセット
      cursor_x = lcd.width(); // 新たな文字が画面右端に描画されるようにカーソル位置を変更
    myFile = SD.open("g.txt");
    size_t count = 0;
    while (myFile.available()) {
    // Serial.write(myFile.read());
      text[count] = myFile.read();
      count++;
    }
    textlen = count;
  /* Close the file */
    myFile.close();      
      break;

    }
    if(digitalRead(4)==LOW){
    lcd.drawPngFile(SD, "b.png", 0, 0);
    delay(1000);
      status =2;
      cursor_x = 0;
      textpos = 0;            // 文字列の読取り位置をリセット
      cursor_x = lcd.width(); // 新たな文字が画面右端に描画されるようにカーソル位置を変更
    myFile = SD.open("b.txt");
    size_t count = 0;
    while (myFile.available()) {
      text[count] = myFile.read();
      count++;
    }
    textlen = count;
  /* Close the file */
    myFile.close();      
      break;

    }

  }

  canvas.pushSprite(&lcd, 0, 104);
}
