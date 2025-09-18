/**
 * USB-CLCD1602をUSBホストとして制御するためのライブラリ ヘッダーファイル
 */

 /**
 MIT License, Copyright (c) 2025 Mitsumine Suzu / verylowfreq
 Please refer LICENSE file for full text.
 **/

#ifndef USBH_CLCD_H_
#define USBH_CLCD_H_

#include "Adafruit_TinyUSB.h"
#include "Print.h"

/**
 * @class USBH_CLCD
 * @brief USB-CLCD1602を制御するためのメインクラス
 */
class USBH_CLCD : public Print {
public:
  /**
   * @brief コンストラクタ
   * @param usb_host Adafruit_USBH_Hostオブジェクトへの参照
   */
  USBH_CLCD(Adafruit_USBH_Host& usb_host) : _usb_host(usb_host) {
    _dev_addr = 0;
    _instance = 0;
    _is_sending = false;
    _last_poll_ms = 0;
    _is_pressed = false;
    _rotation_accumulator = 0;
  }

  /**
   * @brief ライブラリの初期化処理。setup()内で呼び出します。
   */
  bool begin(void);

  /**
   * @brief ライブラリの更新処理。main loop()の中で毎回呼び出す必要があります。
   */
  void update(void);

  /**
   * @brief USB-CLCDが接続され、利用可能な状態かを確認します。
   * @return true 利用可能
   * @return false 利用不可
   */
  bool available(void);

  // --- LCD制御メソッド ---

  /** @brief 画面をクリアします。 */
  void clear(void);

  /**
   * @brief カーソル位置を設定します。
   * @param row 行 (0 or 1)
   * @param col 列 (0 to 15)
   */
  void setCursor(uint8_t row, uint8_t col);

  /**
   * @brief バックライトのON/OFFを制御します。
   * @param on trueでON, falseでOFF
   */
  void backlight(bool on);

  /**
   * @brief 1文字をLCDに書き込みます。(Printクラスのオーバーライド)
   */
  size_t write(uint8_t c) override;

  /**
   * @brief 文字列をLCDに書き込みます。(Printクラスのオーバーライド)
   */
  size_t write(const uint8_t* buffer, size_t len) override;

  // --- 入力取得メソッド ---

  /**
   * @brief ボタンが現在押されているかを取得します。
   * @return true 押されている
   * @return false 押されていない
   */
  bool isPressed(void);

  /**
   * @brief 前回の呼び出しから蓄積されたエンコーダーの回転量を取得します。
   * @return int8_t 回転量。取得後、内部の累積値は0にリセットされます。
   */
  int8_t readRotation(void);

  // --- スケッチ側から呼び出されるコールバックハンドラ ---
  // これらはTinyUSBのグローバルコールバックから呼び出されることを想定しています。

  /** @brief デバイスがマウントされたときの処理 */
  bool mount_cb(uint8_t dev_addr);
  /** @brief デバイスがアンマウントされたときの処理 */
  void umount_cb(uint8_t dev_addr);
  /** @brief デバイスからHIDレポートを受信したときの処理 */
  void report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);
  /** @brief デバイスへのHIDレポート送信が完了したときの処理 */
  void report_sent_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len);

private:
  /** @brief HIDレポートを送信する内部ヘルパー関数 */
  void send_packet(const uint8_t* packet, uint16_t len);
  /** @brief 単一のコマンドを送信する内部ヘルパー関数 */
  void send_command(uint8_t command, uint8_t data);

  // USBホストスタックへの参照
  Adafruit_USBH_Host& _usb_host;

  // デバイス情報
  uint8_t _dev_addr;   // デバイスアドレス
  uint8_t _instance;   // HIDインターフェースのインスタンス番号

  // 状態管理
  bool _is_sending;         // データ送信中フラグ
  uint32_t _last_poll_ms;   // 最終ポーリング時刻
  
  // 入力状態
  bool _is_pressed;                 // ボタンの押下状態
  int8_t _rotation_accumulator;   // 回転量の累積値
};

#endif