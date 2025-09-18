#pragma once

/**
 MIT License, Copyright (c) 2025 Mitsumine Suzu / verylowfreq
 Please refer LICENSE file for full text.
 **/

#include <string.h>

#include <Wire.h>
#include <Print.h>

/**
 * @class TWCLCD1602
 * @brief I2C接続の16x2キャラクターLCDモジュールを制御するためのライブラリです。
 *        このモジュールは、LCD機能に加えて、ロータリーエンコーダとプッシュボタンも内蔵しています。
 *        ArduinoのPrintクラスを継承しているため、print()やprintln()が使用できます。
 */
class TWCLCD1602 : public Print {
public:
  /**
   * @brief デバイスのI2Cアドレス
   */
  static constexpr uint8_t I2C_CLCD_ADDRESS = 0x2f;

  /**
   * @brief I2C通信に使用するTwoWireオブジェクトへの参照
   */
  TwoWire& _wire;

  /**
   * @brief ボタンが押されているかの状態 (`update()`で更新)
   */
  bool _pressed = 0;

  /**
   * @brief ロータリーエンコーダの回転量 (`update()`で更新)
   */
  int8_t _nob = 0;

  /**
   * @brief コンストラクタ
   * @param twoWire I2C通信に使用するTwoWireオブジェクト (例: Wire)
   */
  TWCLCD1602(TwoWire& twoWire) : _wire(twoWire) { }
  
  /**
   * @brief LCDの初期化処理
   */
  void begin() { }

  /**
   * @brief デバイスのI2Cペリフェラルをリセットします。
   *        I2C通信がスタックした場合に、この関数を呼び出して復帰を試みることができます。
   */
  void resetDevice() {
    _wire.beginTransmission(I2C_CLCD_ADDRESS);
    _wire.write(0xFF); // リセットコマンド
    _wire.endTransmission();
    delay(200); // デバイスがリセット処理を完了するのを待つ
  }

  /**
   * @brief ディスプレイの表示をすべてクリアし、カーソルをホームポジションに戻します。
   */
  void clear() {
    beginTransmission();
    wireWrite(0x02); // コマンドモード
    wireWrite(0x01); // クリアコマンド
    endTransmission();
    delay(4); // コマンド処理のための待機
  }

  /**
   * @brief カーソルを左上の初期位置 (0, 0) に移動します。
   */
  void home() {
    beginTransmission();
    wireWrite(0x02); // コマンドモード
    wireWrite(0x02); // ホームコマンド
    endTransmission();
    delay(4); // コマンド処理のための待機
  }

  /**
   * @brief ディスプレイの表示をオフにします。表示内容はRAMに保持されます。
   */
  void noDisplay() {
    beginTransmission();
    wireWrite(0x02); // コマンドモード
    wireWrite(0x08 | 0x00); // 表示オフ
    endTransmission();
    delay(2);
  }

  /**
   * @brief ディスプレイの表示をオンにします。
   */
  void display() {
    beginTransmission();
    wireWrite(0x02); // コマンドモード
    wireWrite(0x08 | 0x04); // 表示オン
    endTransmission();
    delay(2);
  }

  /**
   * @brief バックライトをオフにします。
   */
  void noBacklight() {
    beginTransmission();
    wireWrite(0x03); // バックライト制御コマンド
    wireWrite(0x00); // オフ
    endTransmission();
    delay(2);
  }

  /**
   * @brief バックライトをオンにします。
   */
  void backlight() {
    beginTransmission();
    wireWrite(0x03); // バックライト制御コマンド
    wireWrite(0x01); // オン
    endTransmission();
    delay(2);
  }

  /**
   * @brief カーソルの位置を設定します。
   * @param row 行 (0 or 1)
   * @param col 列 (0 to 15)
   */
  void setCursor(uint8_t row, uint8_t col) {
    row = row % 2;
    col = col % 16;
    
    beginTransmission();
    wireWrite(0x02); // コマンドモード
    wireWrite(0x80 | ((0x40 * row) + col)); // DDRAMアドレス設定
    endTransmission();
    delay(2);
  }

  /**
   * @brief 1文字をディスプレイに書き込みます。
   * @param ch 書き込む文字
   * @return 書き込んだバイト数 (常に1)
   */
  size_t write(uint8_t ch) override {
    beginTransmission();
    wireWrite(0x01); // データモード
    wireWrite(ch);
    endTransmission();
    return 1;
  }

  /**
   * @brief 文字列をディスプレイに書き込みます。
   * @param buffer 書き込むデータのポインタ
   * @param size 書き込むデータのサイズ
   * @return 書き込んだバイト数
   */
  size_t write(const uint8_t *buffer, size_t size) override {
    const size_t orig_size = size;
    const uint8_t* p = buffer;
    while (size > 0) {
      size_t txlen = size <= (MAX_DATA_LENGTH / 2) ? size : (MAX_DATA_LENGTH / 2);
      beginTransmission();
      for (size_t i = 0; i < txlen; i++) {
        wireWrite(0x01); // データモード
        wireWrite(p[i]);
      }
      endTransmission();
      p += txlen;
      size -= txlen;
    }
    return orig_size;
  }

  /**
   * @brief デバイスからロータリーエンコーダとボタンの状態を読み込み、内部状態を更新します。
   *        この関数をループ内で定期的に呼び出す必要があります。
   */
  void update() {
    uint8_t buf[8];
    _wire.beginTransmission(I2C_CLCD_ADDRESS);
    _wire.write(0x04); // 入力状態読み出しコマンド
    int result = _wire.endTransmission(false);
    if (result != 0) {
      // Failed to communicate with USB-LCD over I2C
      return;
    }
    _wire.requestFrom((uint8_t)I2C_CLCD_ADDRESS, (uint8_t)8);
    for (int i = 0; i < 8 && _wire.available(); i++) {
      buf[i] = Wire.read();
    }
    Wire.endTransmission();
    _pressed = buf[0]; // 1バイト目: ボタンの状態
    _nob = (int8_t)buf[1]; // 2バイト目: エンコーダの回転量
  }

  /**
   * @brief ボタンが押されているかを取得します。
   * @return `update()`を呼び出した後のボタンの状態 (true: 押されている, false: 押されていない)
   */
  bool isPressed() {
    return _pressed;
  }

  /**
   * @brief ロータリーエンコーダの回転方向を取得します。
   * @return `update()`を呼び出した後の回転方向 (1: 時計回り, -1: 反時計回り, 0: 回転なし)
   */
  int getDirection() {
    if (_nob > 0) {
      return 1;
    } else if (_nob < 0) {
      return -1;
    } else {
      return 0;
    }
  }

private:
  // I2C通信の1トランザクションで送信できる最大データ長
  static constexpr uint8_t MAX_DATA_LENGTH = 30;
  // 送信データを一時的に格納するバッファ
  uint8_t buffer[MAX_DATA_LENGTH];
  // 現在バッファに格納されているデータ長
  uint8_t datalen;

  /**
   * @brief I2C送信トランザクションを開始するために、内部バッファを初期化します。
   */
  void beginTransmission() {
    memset(buffer, 0x00, MAX_DATA_LENGTH);
    datalen = 0;
  }

  /**
   * @brief 送信する1バイトのデータを内部バッファに追加します。
   * @param data 送信するデータ
   */
  void wireWrite(uint8_t data) {
    if (datalen == MAX_DATA_LENGTH) {
      return;
    }
    buffer[datalen] = data;
    datalen += 1;
  }

  /**
   * @brief 内部バッファに格納されたデータをI2Cで一括送信します。
   * @return Wire.endTransmission()の戻り値
   */
  int endTransmission() {
    _wire.beginTransmission(I2C_CLCD_ADDRESS);
    for (int i = 0; i < datalen; i++) {
      _wire.write(buffer[i]);
    }
    int result = _wire.endTransmission();
    return result;
  }
};
