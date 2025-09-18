/**
 MIT License, Copyright (c) 2025 Mitsumine Suzu / verylowfreq
 Please refer LICENSE file for full text.
 **/

#include <Wire.h>

constexpr int PIN_SCL = 1;
constexpr int PIN_SDA = 2;

constexpr uint8_t I2C_CLCD_ADDRESS = 0x2f;

#include "TWCLCD1602.h"

TWCLCD1602 clcd(Wire);

void setup() {
  Serial.begin(115200);
  for (unsigned long start_time = millis(); millis() - start_time <= 3000 && !Serial;) {
    delay(10);
  }
  Serial.println("Initializing...");

  // ESP32の Wire.begin() はピンの指定ができる。
  Wire.begin(PIN_SDA, PIN_SCL);

  // USB-CLCDを開始
  clcd.begin();
  // 表示をクリア
  clcd.clear();
  delay(1000);
  // カーソルを1行目、1文字目に移動
  clcd.setCursor(0, 0);
  // テキストを表示
  clcd.print(" CLCD over I2C  ");

  clcd.setCursor(1, 0);
  clcd.print("Wire & Arduino");

  // バックライトを点灯
  clcd.backlight();
  delay(500);
  // バックライトを消灯
  clcd.noBacklight();
  delay(500);

  Serial.println("Ready.");
}

void loop() {
  static int val = 0;
  
  // 入力情報を読み込み
  clcd.update();

  // つまみの押し込み状態で動作を切り替え
  if (clcd.isPressed()) {
    clcd.noBacklight();
  } else {
    clcd.backlight();
  }

  // つまみの回転方向を取得
  val += clcd.getDirection();

  // 文字の位置をセット
  clcd.setCursor(1, 0);

  // つまみ位置を表示
  clcd.print("val = ");
  clcd.print(val);
  clcd.pritn("      ");

  // ループのインターバルを調整
  delay(10);
}
