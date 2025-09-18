/**
 MIT License, Copyright (c) 2025 Mitsumine Suzu / verylowfreq
 Please refer LICENSE file for full text.
 **/

/** USBH_CLCDライブラリの簡単な使用例スケッチ
 * 
 * このスケッチは、USBH_CLCDライブラリの基本的な使い方を示します。
 * 以下の内容を含みます:
 *  - USBホストスタックとライブラリの初期化
 *  - USB-CLCD1602の接続・切断の検出
 *  - LCDへのテキスト表示
 *  - ロータリーエンコーダーとプッシュボタンからの入力読み取り
 */

// USBホスト機能のためのAdafruit TinyUSBライブラリ
#include "Adafruit_TinyUSB.h"

// USB-CLCD1602用ライブラリ
#include "USBH_CLCD.h"

// USBホストスタックのためのインスタンスを生成します。
// このオブジェクトが低レベルなUSBホストの動作を管理します。
Adafruit_USBH_Host USBHost;

// CLCDライブラリのインスタンスを生成します。
// 動作にはUSBHostオブジェクトへの参照が必要です。
USBH_CLCD clcd(USBHost);

// エンコーダーからの累積回転量を保持するためのグローバル変数。
int32_t total_rotation = 0;

//====================================================================+
//  セットアップ
//====================================================================+
void setup() {
  // デバッグ出力用のシリアル通信を初期化します。
  Serial.begin(115200);
  // シリアルポートが接続されるのを最大3秒間待ちます。
  while (!Serial && millis() < 3000) delay(10);

  Serial.println("USB-CLCD Host Library Example");

  // USBホストスタックを初期化します。
  // ポート番号は使用するボードのハードウェアに依存します。
  // - 0: RP2040 (Raspberry Pi Pico など)
  // - 1: ESP32-S3
  USBHost.begin(0); 

  // CLCDライブラリを初期化します。
  clcd.begin();
}

//====================================================================+
//  ループ
//====================================================================+
void loop() {
  // `loop`関数の中では、必ず `USBHost.task()` を呼び出す必要があります。
  // これが全てのバックグラウンドUSBタスクを処理します。
  USBHost.task();

  // `clcd.update()` も同様に呼び出す必要があります。
  // これはデバイスに新しい入力レポート（ボタンやエンコーダーの状態）を問い合わせます。
  clcd.update();

  // デバイス接続時に一度だけ初期化コードを実行するための静的フラグ。
  static bool is_initialized = false;

  // `clcd.available()` は、USB-CLCDが接続され準備ができたときにtrueを返します。
  if (clcd.available()) {
    
    // このブロックは、デバイスが最初に検出されたときに一度だけ実行されます。
    if (!is_initialized) {
      is_initialized = true;
      Serial.println("CLCD is available. Initializing display.");
      delay(200); // 接続後の安定化のために少し待機
      
      clcd.clear();          // LCD画面をクリア
      clcd.backlight(true);  // バックライトを点灯
      
      // LCDに固定のテキストを表示
      clcd.setCursor(0, 0);  // カーソルを1行目の1文字目に移動
      clcd.print("USBH_CLCD");
      clcd.setCursor(1, 0);  // カーソルを2行目の1文字目に移動
      clcd.print("Hello, world.");

      delay(1000); // 初期メッセージを1秒間表示
    }

    // --- 入力処理 ---

    // 前回の呼び出し以降に蓄積された回転量を読み取ります。
    // 回転がなければ0を返します。
    int8_t rotation = clcd.readRotation();
    if (rotation != 0) {
      total_rotation += rotation;
      Serial.printf("Rotation: %d, Total: %d\n", rotation, total_rotation);
    }

    // 現在のボタンの状態を確認します。
    if (clcd.isPressed()) {
      Serial.println("Button is pressed.");
    }

    // --- 画面更新 ---

    // 2行目の表示を現在の入力状態で更新します。
    clcd.setCursor(1, 0);
    clcd.printf("Rot:%-5d Btn:%d", total_rotation, clcd.isPressed());

  } else {
    // デバイスが切断されたら、初期化フラグをリセットします。
    is_initialized = false;
  }
  
  delay(20); // ループが速くなりすぎないように少し待機します。
}


//====================================================================+
//  TinyUSBホストコールバック
//====================================================================+
// これらのグローバル関数は、TinyUSBライブラリによって呼び出される関数です。
// このライブラリでは、イベントを`clcd`オブジェクトにそのまま受け渡すだけでOKです。
//====================================================================+

// 新しいデバイスがマウント（接続）されたときに呼ばれます。
void tuh_mount_cb(uint8_t dev_addr) {
  // このデバイスをライブラリが認識するかどうか問い合わせます。
  if (clcd.mount_cb(dev_addr)) {
    Serial.printf("USB-CLCD mounted (Address: %d)\n", dev_addr);
  }
}

// デバイスがアンマウント（切断）されたときに呼ばれます。
void tuh_umount_cb(uint8_t dev_addr) {
  Serial.printf("Device unmounted (Address: %d)\n", dev_addr);
  // アンマウントイベントをライブラリに通知します。
  clcd.umount_cb(dev_addr);
}

// デバイスからHIDレポートを受信したときに呼ばれます。
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // 受信したレポートをライブラリに渡して処理させます。
  clcd.report_received_cb(dev_addr, instance, report, len);
}

// デバイスにレポートが正常に送信された後に呼ばれます。
void tuh_hid_report_sent_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len) {
  // レポートの送信完了をライブラリに通知します。
  clcd.report_sent_cb(dev_addr, idx, report, len);
}
