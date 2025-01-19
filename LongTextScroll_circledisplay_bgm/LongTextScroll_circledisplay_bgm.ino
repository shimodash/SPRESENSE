#include <SDHCI.h>
#define LGFX_AUTODETECT
// #include <LovyanGFX.hpp>
#include "LGFX_SPRESENSE_sample.hpp"

#include <Audio.h>
AudioClass *theAudio;

bool ErrEnd = false;

/*
 SDカードの直下のフォルダに
 r.txtとr.png
 g.txtとg.png
 b.txtとb.png
 を格納して下さい。
*/
SDClass SD,theSD;  /**< SDClass object */ 
File myFile,mysndFile; /**< File object */ 

LGFX lcd;
LGFX_Sprite canvas;

int sndVolume = -160;
// static constexpr char text[] = "Hello world ! こんにちは世界！ this is long long string sample. 寿限無、寿限無、五劫の擦り切れ、海砂利水魚の、水行末・雲来末・風来末、喰う寝る処に住む処、藪ら柑子の藪柑子、パイポ・パイポ・パイポのシューリンガン、シューリンガンのグーリンダイ、グーリンダイのポンポコピーのポンポコナの、長久命の長助";
// static constexpr size_t textlen = sizeof(text) / sizeof(text[0]);
char text[4096] = ""; //スクロールさせる文字のファイルは4096byteまで
size_t textlen = 0;
size_t textpos = 0;

int status = 0; // R=0 G=1 B=2


static void audio_attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!");
  
  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING)
    {
      ErrEnd = true;
   }
}

void setup(void) 
{
  lcd.init();
  lcd.setRotation(3);
  lcd.drawPngFile(SD, "r.png", 0, 0);
  delay(1000);
  // 画面が横長になるように回転
  if (lcd.width() < lcd.height()) lcd.setRotation(lcd.getRotation() ^ 1);

   lcd.setColorDepth(8);
  //  lcd.setFont(&fonts::lgfxJapanMinchoP_32);
  //  lcd.setFont(&fonts::lgfxJapanMinchoP_16);
  //  lcd.setFont(&fonts::efontJA_10);
  //  lcd.setCursor(10, 120);
  //  lcd.print("明朝体 16 Hello World\nこんにちは世界\n");
  //  lcd.setTextSize(1); 
  canvas.setColorDepth(8);
  canvas.setFont(&fonts::lgfxJapanMinchoP_32);
  canvas.setTextSize(1); 
  canvas.setTextWrap(false);        // 右端到達時のカーソル折り返しを禁止
  canvas.createSprite(lcd.width() + 36, 36); // 画面幅+１文字分の横幅を用意
  canvas.createSprite(lcd.width() + 36*2, 36*1); // 画面幅+１文字分の横幅を用意
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


  while (!theSD.begin())
    {
      /* wait until SD card is mounted. */
      Serial.println("Insert SD card.");
    }

  // start audio system
  theAudio = AudioClass::getInstance();

  theAudio->begin(audio_attention_cb);

  puts("initialization Audio Library");

  /* Set clock mode to normal */
  theAudio->setRenderingClockMode(AS_CLKMODE_NORMAL);

  /* Set output device to speaker with first argument.
   * If you want to change the output device to I2S,
   * specify "AS_SETPLAYER_OUTPUTDEVICE_I2SOUTPUT" as an argument.
   * Set speaker driver mode to LineOut with second argument.
   * If you want to change the speaker driver mode to other,
   * specify "AS_SP_DRV_MODE_1DRIVER" or "AS_SP_DRV_MODE_2DRIVER" or "AS_SP_DRV_MODE_4DRIVER"
   * as an argument.
   */
  theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, AS_SP_DRV_MODE_LINEOUT,65536,0);

  /*
   * Set main player to decode stereo mp3. Stream sample rate is set to "auto detect"
   * Search for MP3 decoder in "/mnt/sd0/BIN" directory
   */
  err_t err = theAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_AUTO, AS_CHANNEL_STEREO);

  /* Verify player initialize */
  if (err != AUDIOLIB_ECODE_OK)
    {
      printf("Player0 initialize error\n");
      exit(1);
    }

  /* Open file placed on SD card */
  mysndFile = theSD.open("Sound.mp3");
  puts("file open");

  /* Verify file open */
  if (!mysndFile)
    {
      printf("File open error\n");
      exit(1);
    }
  printf("Open! 0x%08lx\n", (uint32_t)mysndFile);

  /* Send first frames to be decoded */
  err = theAudio->writeFrames(AudioClass::Player0, mysndFile);
  puts("write Frame first done");

  if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND))
    {
      printf("File Read Error! =%d\n",err);
      mysndFile.close();
      exit(1);
    }

  puts("Play!");
  /* Main volume set to -16.0 dB */
  // theAudio->setVolume(-160);
  theAudio->setVolume(sndVolume);
  puts("set Volume end!");
  theAudio->startPlayer(AudioClass::Player0);

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
  // while (textpos < textlen ) // 画面右端に文字が書けるか判定
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
    sndVolume =-160;
    theAudio->setVolume(sndVolume);
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
    sndVolume +=10;
    theAudio->setVolume(sndVolume);
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
    sndVolume -=10;
    theAudio->setVolume(sndVolume);
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

  /* Send new frames to decode in a loop until file ends */
  int err = theAudio->writeFrames(AudioClass::Player0, mysndFile);
  /*  Tell when player file ends */
  if (err == AUDIOLIB_ECODE_FILEEND)
    {
      printf("Main player File End!\n");
      theAudio->stopPlayer(AudioClass::Player0);
      mysndFile.close();
      mysndFile = theSD.open("Sound.mp3");
      puts("file reopen");

      /* Verify file open */
      if (!mysndFile)
        {
          printf("File open error\n");
          exit(1);
        }
      printf("Open! 0x%08lx\n", (uint32_t)mysndFile);

      /* Send first frames to be decoded */
      err = theAudio->writeFrames(AudioClass::Player0, mysndFile);
      puts("write Frame first done");

      if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND))
        {
          printf("File Read Error! =%d\n",err);
          mysndFile.close();
          exit(1);
        }

      puts("RePlay!");
      /* Main volume set to -16.0 dB */
      // theAudio->setVolume(-160);
      // puts("set Volume end!");
      theAudio->startPlayer(AudioClass::Player0);

    }

  /* Show error code from player and stop */
  if (err)
    {
      printf("Main player error code: %d\n", err);
      // goto stop_player;
    }

  if (ErrEnd)
    {
      printf("Error End\n");
      printf("Retry\n");
      
      theAudio->stopPlayer(AudioClass::Player0);
      mysndFile.close();

      err = theAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_AUTO, AS_CHANNEL_STEREO);

      /* Verify player initialize */
     if (err != AUDIOLIB_ECODE_OK)
      {
        printf("Player0 initialize error\n");
        // exit(1);
      }


      mysndFile = theSD.open("Sound.mp3");
      puts("file reopen");

      /* Verify file open */
      if (!mysndFile)
        {
          printf("File open error\n");
          exit(1);
        }
      printf("Open! 0x%08lx\n", (uint32_t)mysndFile);

      /* Send first frames to be decoded */
      err = theAudio->writeFrames(AudioClass::Player0, mysndFile);
      puts("write Frame first done");

      if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND))
        {
          printf("File Read Error! =%d\n",err);
          mysndFile.close();
          exit(1);
        }

      puts("RePlay!");
      /* Main volume set to -16.0 dB */
      theAudio->setVolume(-160);
      sndVolume = -160;
      puts("set Volume end!");
      theAudio->startPlayer(AudioClass::Player0);
      ErrEnd = false;
      // goto stop_player;
    }

  }

  canvas.pushSprite(&lcd, 0, 104);

  return;

stop_player:
  theAudio->stopPlayer(AudioClass::Player0);
  mysndFile.close();
  theAudio->setReadyMode();
  theAudio->end();
  exit(1);
}
