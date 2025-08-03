#include <SDHCI.h> // SDHCIライブラリをインクルードします。SDカードとの通信に必要です。
#define LGFX_AUTODETECT // LovyanGFXライブラリの自動検出機能を有効にします。ディスプレイの種類を自動で判別します。
// #include <LovyanGFX.hpp> // LovyanGFXライブラリのメインヘッダファイル。通常はLGFX_AUTODETECTにより自動で含まれます。
#include "LGFX_SPRESENSE_sample.hpp" // SPRESENSEボードに特化したLovyanGFXの設定ファイルです。

#include <Audio.h> // Audioライブラリをインクルードします。音声再生機能を提供します。
AudioClass *theAudio; // AudioClassのポインタを宣言します。音声再生オブジェクトへの参照を保持します。

bool ErrEnd = false; // エラーが発生した際にtrueになるフラグです。

/*
 SDカードの直下のフォルダに
 r.txtとr.png
 g.txtとg.png
 b.txtとb.png
 を格納して下さい。
*/
SDClass SD,theSD;  /**< SDClass object */ // SDカードオブジェクトを宣言します。SDカードへのアクセスに使用します。
File myFile,mysndFile; /**< File object */ // Fileオブジェクトを宣言します。テキストファイルや音声ファイルへのアクセスに使用します。

LGFX lcd; // LGFXクラスのインスタンスを作成します。LCDディスプレイの制御に使用します。
LGFX_Sprite canvas; // LGFX_Spriteクラスのインスタンスを作成します。画面外描画バッファとして使用し、スクロール表示に利用します。

int sndVolume = -160; // サウンドの音量初期値です。-16.0dBに相当します。
// static constexpr char text[] = "Hello world ! こんにちは世界！ this is long long string sample. 寿限無、寿限無、五劫の擦り切れ、海砂利水魚の、水行末・雲来末・風来末、喰う寝る処に住む処、藪ら柑子の藪柑子、パイポ・パイポ・パイポのシューリンガン、シューリンガンのグーリンダイ、グーリンダイのポンポコピーのポンポコナの、長久命の長助"; // スクロールさせるテキストのサンプルです（コメントアウトされています）。
// static constexpr size_t textlen = sizeof(text) / sizeof(text[0]); // サンプルテキストの長さです（コメントアウトされています）。
char text[4096] = ""; // スクロールさせる文字のファイルを格納する配列です（最大4096バイト）。
size_t textlen = 0; // text配列に読み込まれた文字の長さです。
size_t textpos = 0; // text配列から読み取る現在の位置です。

int status = 0; // 現在の表示状態を示すステータスです。0:R(赤), 1:G(緑), 2:B(青)に対応します。


// オーディオアテンションコールバック関数です。オーディオシステムでエラーや警告が発生した際に呼び出されます。
static void audio_attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!"); // シリアルモニタに"Attention!"と表示します。

  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING) // エラーコードが警告レベル以上の場合
    {
      ErrEnd = true; // エラー終了フラグをtrueに設定します。
   }
}

void setup(void)
{
  lcd.init(); // LCDディスプレイを初期化します。
  lcd.setRotation(3); // LCDの表示方向を回転させます（横長表示になるように）。
  lcd.drawPngFile(SD, "r.png", 0, 0); // SDカードから"r.png"ファイルを読み込み、LCDの(0,0)座標に描画します。
  delay(1000); // 1秒間処理を停止します。
  // 画面が横長になるように回転
  if (lcd.width() < lcd.height()) lcd.setRotation(lcd.getRotation() ^ 1); // LCDの幅が高さより小さい場合（縦長の場合）、回転させて横長にします。

   lcd.setColorDepth(8); // LCDの色深度を8ビットに設定します。
  //  lcd.setFont(&fonts::lgfxJapanMinchoP_32); // フォント設定の例（コメントアウトされています）。
  //  lcd.setFont(&fonts::lgfxJapanMinchoP_16); // フォント設定の例（コメントアウトされています）。
  //  lcd.setFont(&fonts::efontJA_10); // フォント設定の例（コメントアウトされています）。
  //  lcd.setCursor(10, 120); // カーソル位置設定の例（コメントアウトされています）。
  //  lcd.print("明朝体 16 Hello World\nこんにちは世界\n"); // テキスト表示の例（コメントアウトされています）。
  //  lcd.setTextSize(1); // テキストサイズ設定の例（コメントアウトされています）。
  canvas.setColorDepth(8); // キャンバスの色深度を8ビットに設定します。
  canvas.setFont(&fonts::lgfxJapanMinchoP_32); // キャンバスに表示するフォントを設定します。
  canvas.setTextSize(1); // キャンバスに表示するテキストのサイズを設定します。
  canvas.setTextWrap(false);        // 右端到達時のカーソル折り返しを禁止します。
  canvas.createSprite(lcd.width() + 36, 36); // キャンバス用のスプライト（描画領域）を作成します。LCDの幅に1文字分（36ピクセル）の余裕を持たせます。
  canvas.createSprite(lcd.width() + 36*2, 36*1); // キャンバスのスプライトを再作成します。LCDの幅に2文字分（36*2ピクセル）の余裕を持たせます。
  Serial.begin(115200); // シリアル通信を開始し、ボーレートを115200に設定します。

  pinMode(LED0, OUTPUT); // LED0ピンを出力モードに設定します。
  pinMode(LED1, OUTPUT); // LED1ピンを出力モードに設定します。
  pinMode(LED2, OUTPUT); // LED2ピンを出力モードに設定します。
  pinMode(LED3, OUTPUT); // LED3ピンを出力モードに設定します。
  pinMode(2, INPUT_PULLUP); // ピン2を入力モード（内部プルアップ抵抗有効）に設定します。ボタン接続用です。
  pinMode(3, INPUT_PULLUP); // ピン3を入力モード（内部プルアップ抵抗有効）に設定します。ボタン接続用です。
  pinMode(4, INPUT_PULLUP); // ピン4を入力モード（内部プルアップ抵抗有効）に設定します。ボタン接続用です。


  myFile = SD.open("r.txt"); // SDカードから"r.txt"ファイルを開きます。
  size_t count = 0; // 読み込んだバイト数をカウントする変数です。
  while (myFile.available()) { // ファイルから読み取り可能なデータがある限りループします。
    // Serial.write(myFile.read()); // 読み込んだ文字をシリアルモニタに出力する（コメントアウトされています）。
    text[count] = myFile.read(); // ファイルから1バイト読み込み、text配列に格納します。
    count++; // カウントをインクリメントします。
  }
  textlen = count; // textlenに読み込んだ合計バイト数を設定します。
  /* Close the file */
  myFile.close(); // ファイルを閉じます。


  while (!theSD.begin()) // SDカードが初期化（マウント）されるまでループします。
    {
      /* wait until SD card is mounted. */
      Serial.println("Insert SD card."); // シリアルモニタに"Insert SD card."と表示します。
    }

  // start audio system
  theAudio = AudioClass::getInstance(); // AudioClassのシングルトンインスタンスを取得します。

  theAudio->begin(audio_attention_cb); // オーディオシステムを開始し、エラー発生時に呼び出されるコールバック関数を設定します。

  puts("initialization Audio Library"); // シリアルモニタに"initialization Audio Library"と表示します。

  /* Set clock mode to normal */
  theAudio->setRenderingClockMode(AS_CLKMODE_NORMAL); // オーディオレンダリングのクロックモードを通常に設定します。

  /* Set output device to speaker with first argument.
   * If you want to change the output device to I2S,
   * specify "AS_SETPLAYER_OUTPUTDEVICE_I2SOUTPUT" as an argument.
   * Set speaker driver mode to LineOut with second argument.
   * If you want to change the speaker driver mode to other,
   * specify "AS_SP_DRV_MODE_1DRIVER" or "AS_SP_DRV_MODE_2DRIVER" or "AS_SP_DRV_MODE_4DRIVER"
   * as an argument.
   */
  theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, AS_SP_DRV_MODE_LINEOUT,65536,0); // オーディオ出力デバイスをスピーカー/ヘッドホン、ドライバモードをラインアウトに設定します。

  /*
   * Set main player to decode stereo mp3. Stream sample rate is set to "auto detect"
   * Search for MP3 decoder in "/mnt/sd0/BIN" directory
   */
  err_t err = theAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_AUTO, AS_CHANNEL_STEREO); // プレイヤー0をMP3デコード用に初期化します。デコーダはSDカードの"/mnt/sd0/BIN"ディレクトリから検索されます。

  /* Verify player initialize */
  if (err != AUDIOLIB_ECODE_OK) // プレイヤーの初期化が成功したか確認します。
    {
      printf("Player0 initialize error\n"); // エラーメッセージを表示します。
      exit(1); // プログラムを終了します。
    }

  /* Open file placed on SD card */
  mysndFile = theSD.open("Sound.mp3"); // SDカードから"Sound.mp3"ファイルを開きます。
  puts("file open"); // シリアルモニタに"file open"と表示します。

  /* Verify file open */
  if (!mysndFile) // ファイルが開けたか確認します。
    {
      printf("File open error\n"); // エラーメッセージを表示します。
      exit(1); // プログラムを終了します。
    }
  printf("Open! 0x%08lx\n", (uint32_t)mysndFile); // 開いたファイルのハンドル（ポインタ）を16進数で表示します。

  /* Send first frames to be decoded */
  err = theAudio->writeFrames(AudioClass::Player0, mysndFile); // プレイヤー0に最初のオーディオフレームを書き込み、デコードを開始します。
  puts("write Frame first done"); // シリアルモニタに"write Frame first done"と表示します。

  if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND)) // 書き込み中にエラーが発生したか、ファイル終端に達していないかを確認します。
    {
      printf("File Read Error! =%d\n",err); // ファイル読み込みエラーメッセージを表示します。
      mysndFile.close(); // ファイルを閉じます。
      exit(1); // プログラムを終了します。
    }

  puts("Play!"); // シリアルモニタに"Play!"と表示します。
  /* Main volume set to -16.0 dB */
  // theAudio->setVolume(-160); // 音量設定の例（コメントアウトされています）。
  theAudio->setVolume(sndVolume); // 現在の音量設定を適用します。
  puts("set Volume end!"); // シリアルモニタに"set Volume end!"と表示します。
  theAudio->startPlayer(AudioClass::Player0); // プレイヤー0での音声再生を開始します。

}

bool pre_digital_red,pre_digital_green,pre_digital_blue; // 各ボタンの前回のデジタル読み取り値を保持する変数です。

void loop(void)
{
  int32_t cursor_x = canvas.getCursorX() - 1;   // 現在のキャンバスのカーソルX座標を取得し、1ドット左に移動させます。
  if (cursor_x <= 0) // カーソル位置が左端（またはそれより左）に到達した場合、テキストが一周したと判断します。
  {
    textpos = 0;            // テキスト配列の読み取り位置をリセットし、最初から読み直します。
    cursor_x = lcd.width(); // 新しい文字が画面の右端から表示されるように、カーソル位置をLCDの幅に設定します。
  }

  canvas.setCursor(cursor_x, 0); // キャンバスのカーソル位置を更新します。
  canvas.scroll(-1, 0);          // キャンバスの内容を左に1ドットスクロールさせます。


  while (textpos < textlen && cursor_x <= lcd.width()) // テキストの読み取り位置がテキストの総長未満であり、かつカーソルが画面右端内にある間ループします。
  // while (textpos < textlen ) // 画面右端に文字が書けるか判定（コメントアウトされています）。
  {
    canvas.print(text[textpos++]);   // text配列から1バイトずつ文字をキャンバスに出力します（マルチバイト文字もこの処理で動作します）。
    cursor_x = canvas.getCursorX();  // 文字を出力した後のキャンバスのカーソルX座標を取得します。

  //button check and image txt switch
    digitalWrite(LED0, digitalRead(2)); // ピン2のデジタル読み取り値（ボタンの状態）に応じてLED0を点灯/消灯します。
    digitalWrite(LED1, digitalRead(3)); // ピン3のデジタル読み取り値に応じてLED1を点灯/消灯します。
    digitalWrite(LED2, digitalRead(4)); // ピン4のデジタル読み取り値に応じてLED2を点灯/消灯します。

    if(digitalRead(2)==LOW && pre_digital_red==HIGH){ // ピン2がLOW（ボタンが押された）かつ、前回の状態がHIGH（ボタンが離されていた）場合（立ち下がりエッジ検出）
      lcd.drawPngFile(SD, "r.png", 0, 0); // SDカードから"r.png"を読み込み、LCDに描画します。
    //   delay(1000); // 1秒間処理を停止します。
      status =0; // ステータスを赤（0）に設定します。
      cursor_x = 0; // カーソルX座標をリセットします。
      textpos = 0;            // テキスト読み取り位置をリセットします。
      cursor_x = lcd.width(); // カーソルX座標をLCDの幅に設定し、新しいテキストが右端から始まるようにします。
      sndVolume =-160; // 音量を初期値（-160）に設定します。
      theAudio->setVolume(sndVolume); // オーディオの音量を更新します。
      pre_digital_red=LOW; // 赤ボタンの前回状態をLOWに更新します。
      myFile = SD.open("r.txt"); // "r.txt"ファイルを開きます。
      size_t count = 0; // 読み込みカウントをリセットします。


      while (myFile.available()) { // ファイルから読み取り可能なデータがある限りループします。
       text[count] = myFile.read(); // ファイルから1バイト読み込み、text配列に格納します。
        count++; // カウントをインクリメントします。
      }
    textlen = count; // textlenを読み込んだバイト数に更新します。
  /* Close the file */
      myFile.close(); // ファイルを閉じます。
      break; // 現在のwhileループを抜けます。
    }

    if(digitalRead(3)==LOW && pre_digital_green==HIGH){ // ピン3がLOW（ボタンが押された）かつ、前回の状態がHIGH（ボタンが離されていた）場合
      lcd.drawPngFile(SD, "g.png", 0, 0); // SDカードから"g.png"を読み込み、LCDに描画します。
      // delay(1000); // 1秒間処理を停止する（コメントアウトされています）。
      status =1; // ステータスを緑（1）に設定します。
      cursor_x = 0; // カーソルX座標をリセットします。
      textpos = 0;            // テキスト読み取り位置をリセットします。
      cursor_x = lcd.width(); // カーソルX座標をLCDの幅に設定し、新しいテキストが右端から始まるようにします。
      sndVolume +=30; // 音量を30増加させます。
      theAudio->setVolume(sndVolume); // オーディオの音量を更新します。
      printf("sndVolune set =%d\n",sndVolume); // 設定された音量をシリアルモニタに表示します。
      pre_digital_green=LOW; // 緑ボタンの前回状態をLOWに更新します。
    myFile = SD.open("g.txt"); // "g.txt"ファイルを開きます。
    size_t count = 0; // 読み込みカウントをリセットします。
    while (myFile.available()) { // ファイルから読み取り可能なデータがある限りループします。
    // Serial.write(myFile.read()); // 読み込んだ文字をシリアルモニタに出力する（コメントアウトされています）。
      text[count] = myFile.read(); // ファイルから1バイト読み込み、text配列に格納します。
      count++; // カウントをインクリメントします。
    }
    textlen = count; // textlenを読み込んだバイト数に更新します。
  /* Close the file */
    myFile.close(); // ファイルを閉じます。
      break; // 現在のwhileループを抜けます。

    }

    if(digitalRead(4)==LOW && pre_digital_blue==HIGH ){ // ピン4がLOW（ボタンが押された）かつ、前回の状態がHIGH（ボタンが離されていた）場合
      lcd.drawPngFile(SD, "b.png", 0, 0); // SDカードから"b.png"を読み込み、LCDに描画します。
      // delay(1000); // 1秒間処理を停止する（コメントアウトされています）。
      status =2; // ステータスを青（2）に設定します。
      cursor_x = 0; // カーソルX座標をリセットします。
      textpos = 0;            // テキスト読み取り位置をリセットします。
      cursor_x = lcd.width(); // カーソルX座標をLCDの幅に設定し、新しいテキストが右端から始まるようにします。
      sndVolume -=30; // 音量を30減少させます。
      theAudio->setVolume(sndVolume); // オーディオの音量を更新します。
      printf("sndVolune set =%d\n",sndVolume); // 設定された音量をシリアルモニタに表示します。
      pre_digital_blue=LOW; // 青ボタンの前回状態をLOWに更新します。
    myFile = SD.open("b.txt"); // "b.txt"ファイルを開きます。
    size_t count = 0; // 読み込みカウントをリセットします。
    while (myFile.available()) { // ファイルから読み取り可能なデータがある限りループします。
      text[count] = myFile.read(); // ファイルから1バイト読み込み、text配列に格納します。
      count++; // カウントをインクリメントします。
    }
    textlen = count; // textlenを読み込んだバイト数に更新します。
  /* Close the file */
    myFile.close(); // ファイルを閉じます。
      break; // 現在のwhileループを抜けます。

    }

  /* Send new frames to decode in a loop until file ends */
  int err = theAudio->writeFrames(AudioClass::Player0, mysndFile); // プレイヤー0に新しいオーディオフレームを書き込みます。ファイル終端までループします。
  /* Tell when player file ends */
  if (err == AUDIOLIB_ECODE_FILEEND) // ファイル終端に達した場合
    {
      printf("Main player File End!\n"); // シリアルモニタに"Main player File End!"と表示します。
      theAudio->stopPlayer(AudioClass::Player0); // プレイヤー0を停止します。
      mysndFile.close(); // 現在の音声ファイルを閉じます。
      mysndFile = theSD.open("Sound.mp3"); // "Sound.mp3"ファイルを再度開きます。
      puts("file reopen"); // シリアルモニタに"file reopen"と表示します。

      /* Verify file open */
      if (!mysndFile) // ファイルが正常に開けたか確認します。
        {
          printf("File open error\n"); // エラーメッセージを表示します。
          exit(1); // プログラムを終了します。
        }
      printf("Open! 0x%08lx\n", (uint32_t)mysndFile); // 開いたファイルのハンドルを表示します。

      /* Send first frames to be decoded */
      err = theAudio->writeFrames(AudioClass::Player0, mysndFile); // プレイヤー0に最初のオーディオフレームを書き込みます。
      puts("write Frame first done"); // シリアルモニタに"write Frame first done"と表示します。

      if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND)) // 書き込み中にエラーが発生したか、ファイル終端に達していないかを確認します。
        {
          printf("File Read Error! =%d\n",err); // ファイル読み込みエラーメッセージを表示します。
          mysndFile.close(); // ファイルを閉じます。
          exit(1); // プログラムを終了します。
        }

      puts("RePlay!"); // シリアルモニタに"RePlay!"と表示します。
      /* Main volume set to -16.0 dB */
      // theAudio->setVolume(-160); // 音量設定の例（コメントアウトされています）。
      // puts("set Volume end!"); // シリアルモニタに"set Volume end!"と表示する（コメントアウトされています）。
      theAudio->startPlayer(AudioClass::Player0); // プレイヤー0での音声再生を再開します。

    }

  /* Show error code from player and stop */
  if (err) // エラーコードが0以外の場合（エラーが発生した場合）
    {
      printf("Main player error code: %d\n", err); // プレイヤーのエラーコードを表示します。
      // goto stop_player; // stop_playerラベルにジャンプする（コメントアウトされています）。
    }

  if (ErrEnd) // エラー終了フラグがtrueの場合
    {
      printf("Error End\n"); // シリアルモニタに"Error End"と表示します。
      printf("Retry\n"); // シリアルモニタに"Retry"と表示します。

      theAudio->stopPlayer(AudioClass::Player0); // プレイヤー0を停止します。
      mysndFile.close(); // 音声ファイルを閉じます。

      err = theAudio->initPlayer(AudioClass::Player0, AS_CODECTYPE_MP3, "/mnt/sd0/BIN", AS_SAMPLINGRATE_AUTO, AS_CHANNEL_STEREO); // プレイヤー0をMP3デコード用に再初期化します。

      /* Verify player initialize */
     if (err != AUDIOLIB_ECODE_OK) // プレイヤーの再初期化が成功したか確認します。
      {
        printf("Player0 initialize error\n"); // エラーメッセージを表示します。
        // exit(1); // プログラムを終了する（コメントアウトされています）。
      }


      mysndFile = theSD.open("Sound.mp3"); // "Sound.mp3"ファイルを再度開きます。
      puts("file reopen"); // シリアルモニタに"file reopen"と表示します。

      /* Verify file open */
      if (!mysndFile) // ファイルが正常に開けたか確認します。
        {
          printf("File open error\n"); // エラーメッセージを表示します。
          exit(1); // プログラムを終了します。
        }
      printf("Open! 0x%08lx\n", (uint32_t)mysndFile); // 開いたファイルのハンドルを表示します。

      /* Send first frames to be decoded */
      err = theAudio->writeFrames(AudioClass::Player0, mysndFile); // プレイヤー0に最初のオーディオフレームを書き込みます。
      puts("write Frame first done"); // シリアルモニタに"write Frame first done"と表示します。

      if ((err != AUDIOLIB_ECODE_OK) && (err != AUDIOLIB_ECODE_FILEEND)) // 書き込み中にエラーが発生したか、ファイル終端に達していないかを確認します。
        {
          printf("File Read Error! =%d\n",err); // ファイル読み込みエラーメッセージを表示します。
          mysndFile.close(); // ファイルを閉じます。
          exit(1); // プログラムを終了します。
        }

      puts("RePlay!"); // シリアルモニタに"RePlay!"と表示します。
      /* Main volume set to -16.0 dB */
      theAudio->setVolume(-160); // 音量を-160に設定します。
      sndVolume = -160; // sndVolumeも-160に設定します。
      puts("set Volume end!"); // シリアルモニタに"set Volume end!"と表示します。
      theAudio->startPlayer(AudioClass::Player0); // プレイヤー0での音声再生を再開します。
      ErrEnd = false; // エラー終了フラグをリセットします。
      // goto stop_player; // stop_playerラベルにジャンプする（コメントアウトされています）。
    }

  pre_digital_red  = digitalRead(2); // ピン2の現在のデジタル読み取り値をpre_digital_redに保存します。
  pre_digital_green= digitalRead(3); // ピン3の現在のデジタル読み取り値をpre_digital_greenに保存します。
  pre_digital_blue =digitalRead(4); // ピン4の現在のデジタル読み取り値をpre_digital_blueに保存します。

  } // whileループの終了

  canvas.pushSprite(&lcd, 0, 104); // キャンバスの内容をLCDの(0, 104)座標に転送して表示します。

  return; // loop関数を終了します。

stop_player: // stop_playerラベル。エラー発生時などにジャンプしてオーディオを停止します。
  theAudio->stopPlayer(AudioClass::Player0); // プレイヤー0を停止します。
  mysndFile.close(); // 音声ファイルを閉じます。
  theAudio->setReadyMode(); // オーディオシステムをレディモードに設定します。
  theAudio->end(); // オーディオシステムを終了します。
  exit(1); // プログラムを終了します。
}
