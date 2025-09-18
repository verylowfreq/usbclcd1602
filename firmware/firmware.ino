/**
 MIT License, Copyright (c) 2025 Mitsumine Suzu / verylowfreq
 Please refer LICENSE file for full text.
 **/

/*********************************************************************
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 Copyright (c) 2019 Ha Thach for Adafruit Industries
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

#undef DEBUG
// #define DEBUG

#include <Wire.h>
#include <HardwareTimer.h>

#include <Adafruit_TinyUSB.h>

constexpr uint16_t USB_VID = 0xf055;
constexpr uint16_t USB_PID = 0x6584;
const char* USB_MANUFACTURER = "MSLab";
const char* USB_PRODUCT = "USB-CLCD1602";

constexpr uint8_t CLCD_ROWS = 2;
constexpr uint8_t CLCD_COLS = 16;
constexpr int PIN_CLCD_EN = PB5;
constexpr int PIN_CLCD_RW = PB4;
constexpr int PIN_CLCD_RS = PB3;
constexpr int PINS_CLCD_DATA[8] = { PA1, PA2, PA3, PA4, PA5, PA6, PA7, PB0 };
constexpr int PIN_CLCD_BKL = PA15;

constexpr int PIN_RE_PUSH = PA0;
constexpr int PIN_RE_A = PA10;
constexpr int PIN_RE_B = PA9;

// HID report descriptor using TinyUSB's template
// Generic In Out with 64 bytes report (max)
uint8_t const desc_hid_report[] = {
    // TUD_HID_REPORT_DESC_GENERIC_INOUT(64)
    HID_USAGE_PAGE_N ( HID_USAGE_PAGE_VENDOR, 2 ),
    HID_USAGE        ( 0x01 ),
    HID_COLLECTION   ( HID_COLLECTION_APPLICATION ),

      /* Input */
      HID_USAGE       ( 0x02                                   ),
      HID_LOGICAL_MIN ( 0x00                                   ),
      HID_LOGICAL_MAX ( 0xff                                   ),
      HID_REPORT_SIZE ( 8                                      ),
      HID_REPORT_COUNT( 8                            ),
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

      /* Output */
      HID_USAGE       ( 0x03                                    ),
      HID_LOGICAL_MIN ( 0x00                                    ),
      HID_LOGICAL_MAX ( 0xff                                    ),
      HID_REPORT_SIZE ( 8                                       ),
      HID_REPORT_COUNT( 64                                      ),
      HID_OUTPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE  ),
    HID_COLLECTION_END
};


// I2Cスレーブアドレス
#define SLAVE_ADDRESS 0x2F

// 送信するデータ（8バイト）
uint8_t tx_data[8] = {};
uint8_t command_byte = 0;
static uint8_t byte_count = 0;


class USBCLCD : public Print {
public:
  uint8_t row = 0;
  uint8_t col = 0;


  void begin() {
    pinMode(PIN_CLCD_EN, OUTPUT);
    pinMode(PIN_CLCD_RW, OUTPUT);
    pinMode(PIN_CLCD_RS, OUTPUT);
    pinMode(PIN_CLCD_BKL, OUTPUT);
    for (size_t i = 0; i < 8; i++) {
      pinMode(PINS_CLCD_DATA[i], OUTPUT);
    }

    set_read_write(true);
    delay(2);
    send_command(0x30);
    delay(2);
    send_command(0x30);
    delay(2);
    send_command(0x30);
    delay(2);
    send_command(0x38);
    delay(2);
    send_command(0x0c);
    delay(2);
    send_command(0x01);
    delay(2);
    send_command(0x06);
    delay(2);

  }

  void set_backlight(bool enabled) {
    digitalWrite(PIN_CLCD_BKL, enabled ? HIGH : LOW);
  }

  void set_read_write(bool is_write) {
    digitalWrite(PIN_CLCD_RW, is_write ? LOW : HIGH);
  }

  void set_command_data(bool is_data) {
    digitalWrite(PIN_CLCD_RS, is_data ? HIGH : LOW);
  }

  void set_data(uint8_t data) {
    for (size_t i = 0; i < 8; i++) {
      digitalWrite(PINS_CLCD_DATA[i], data & (0x01 << i) ? HIGH : LOW);
    }
  }

  void send_command(uint8_t command) {
    send(command, false);
  }

  void send_data(uint8_t data) {
    send(data, true);
    delayMicroseconds(50);
  }

  void send(uint8_t data, bool is_data) {
    digitalWrite(PIN_CLCD_EN, HIGH);
    delayMicroseconds(20);
    set_command_data(is_data);
    set_data(data);
    digitalWrite(PIN_CLCD_EN, LOW);
    delayMicroseconds(20);
  }

  void set_cursor(uint8_t row, uint8_t col) {
    row = row % CLCD_ROWS;
    col = col % CLCD_COLS;
    send_command(0x80 | (0x40 * (row) + col));
    this->row = row;
    this->col = col;
  }

  void clear() {
    send_command(0x01);
  }

  size_t write(uint8_t ch) override {
    if (ch == '\n') {
      set_cursor(this->row++, 0);
      return 1;
    }
    send_data(ch);
    col += 1;
    if (col == 16) {
      col = 0;
      row += 1;
      if (row == 2) {
        row = 0;
      }
    }
    return 1;
  }
};


class EncoderButton {
public:
  // コンストラクタ：A相、B相、押しボタンのピン番号を指定
  EncoderButton(uint8_t pinA, uint8_t pinB, uint8_t pinPush)
    : _pinA(pinA), _pinB(pinB), _pinPush(pinPush),
      _lastStateA(LOW), _stableStateA(LOW), _direction(0), _lastButtonState(HIGH),
      _buttonState(HIGH), _lastDebounceTime(0), _lastDebounceTimeA(0),
      _clicked(false), _pressingDown(false), _lastAB(0), _accumulator(0) {}

  // 初期化処理：setup()内で呼び出す
  void begin() {
    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);
    pinMode(_pinPush, INPUT_PULLUP);
    _lastStateA = digitalRead(_pinA);
    _stableStateA = _lastStateA;
    _lastDebounceTimeA = 0;

    // クアドラチャ用に A/B の初期状態を取得
    _lastAB = ((uint8_t)digitalRead(_pinA) << 1) | (uint8_t)digitalRead(_pinB);
    _accumulator = 0;
  }

  // 状態更新：loop()内で毎回呼び出す
  void update() {
    updateRotation();
    updateClick();
  }

  // 回転方向を取得する
  // 戻り値： 1 (時計回り), -1 (反時計回り), 0 (回転なし)
  // この関数を呼び出すと、内部の回転状態はリセットされます。
  int getDirection() {
    int dir = _direction;
    _direction = 0; // 読み取ったらリセット
    return dir;
  }

  // ボタンがクリックされたかを取得する
  // 戻り値： true (クリックされた), false (クリックされていない)
  // この関数を呼び出すと、内部のクリック状態はリセットされます。
  bool isClicked() {
    bool ret = _clicked;
    _clicked = false; // 読み取ったらリセット
    return ret;
  }

  bool isPressingDown() {
    return _pressingDown;
  }

// private:
  // ピン番号
  uint8_t _pinA;
  uint8_t _pinB;
  uint8_t _pinPush;

  // 回転検出用変数
  int _lastStateA;             // 生データの直近値
  int _stableStateA;           // デバウンス後の安定した状態
  unsigned long _lastDebounceTimeA; // A相の最終変化時刻
  volatile int _direction;

  // クアドラチャ用：直前の AB 組
  uint8_t _lastAB;

  // 遷移累積（一定数の遷移で1クリックとする）
  int _accumulator;
  static const int QUAD_PER_DETENT = 4; // 4遷移で1デタント（必要なら2などに変更）

  // ボタン検出用変数
  bool _lastButtonState;
  bool _buttonState;
  unsigned long _lastDebounceTime;
  bool _clicked;
  bool _pressingDown;
  static const unsigned long DEBOUNCE_DELAY = 3; // チャタリング防止の時間 (ms)

  // 回転状態を更新する内部関数（クアドラチャの全遷移を使い、所定遷移で1カウント）
  void updateRotation() {
    // 4-state decoding (robust against phase) using transition lookup
    uint8_t a = digitalRead(_pinA);
    uint8_t b = digitalRead(_pinB);
    uint8_t cur = (a << 1) | b;

    if (cur != _lastAB) {
      // index = (prev << 2) | cur  (4-bit)
      static const int8_t table[16] = {
        0,  -1,  1,  0,
        1,   0,  0, -1,
       -1,   0,  0,  1,
        0,   1, -1,  0
      };
      uint8_t idx = (_lastAB << 2) | cur;
      int8_t delta = table[idx & 0x0f]; // +1/-1 per valid transition
      if (delta != 0) {
        _accumulator += delta;
        // 一定遷移数に達したら 1 クリック分として _direction に反映
        int steps = _accumulator / QUAD_PER_DETENT; // 符号付きのステップ数
        if (steps != 0) {
          _direction += steps; // +1 or -1 (または複数)
          _accumulator -= steps * QUAD_PER_DETENT;
        }
      }
      _lastAB = cur;
    }
  }

  // ボタンの状態を更新する内部関数
  void updateClick() {
    bool reading = digitalRead(_pinPush);

    // 前回の状態と違う場合、チャタリングの可能性あり
    if (reading != _lastButtonState) {
      _lastDebounceTime = millis();
    }

    // チャタリング時間を超えて状態が安定している場合
    if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY) {
      // 状態が変化していれば更新
      if (reading != _buttonState) {
        _buttonState = reading;
        // 押された瞬間（HIGHからLOWになった時）にクリックと判定
        if (_buttonState == LOW) {
          _clicked = true;
          _pressingDown = true;
        } else {
          _pressingDown = false;
        }
      }
    }
    _lastButtonState = reading;
  }
};


Adafruit_USBD_HID usb_hid;

USBCLCD clcd;

EncoderButton rotaryEncoder(PIN_RE_A, PIN_RE_B, PIN_RE_PUSH);


bool connected_usb = false;
bool connected_i2c = false;

bool connected() {
  return connected_usb || connected_i2c;
}

constexpr uint16_t in_report_length = 8;
uint8_t prev_report[in_report_length] = {};

char usb_serialstring[24 + 1] = {};


int rotary_encoder_value = 0;




void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  for (unsigned long start = millis(); millis() - start < 3000 && !Serial;) {
    delay(100);
  }
#endif

#ifndef DEBUG
  TinyUSBDevice.clearConfiguration();
#endif

  TinyUSBDevice.setID(USB_VID, USB_PID);

  TinyUSBDevice.setManufacturerDescriptor(USB_MANUFACTURER);
  TinyUSBDevice.setProductDescriptor(USB_PRODUCT);
  get_unique_id(usb_serialstring);
  TinyUSBDevice.setSerialDescriptor(usb_serialstring);

  usb_hid.enableOutEndpoint(true);
  usb_hid.setPollInterval(2);
  usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  usb_hid.setStringDescriptor("USB-CLCD1602");
  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();

  // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
#ifndef DEBUG
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
#endif

#ifdef DEBUG
  Serial.println("Initializing...");
#endif

  clcd.begin();
  clcd.set_backlight(true);

  rotaryEncoder.begin();

  clcd.print("USB-CLCD1602  v1");

  Wire.begin(SLAVE_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

#ifdef DEBUG
  Serial.printf("USBDevice Serial: \"%s\"\r\n", usb_serialstring);
  Serial.println("Ready.");
#endif
}


void receiveEvent(int num_of_Bytes) {
  connected_i2c = true;

  if (num_of_Bytes == 1) {
    uint8_t command = Wire.read();
    if (command == 0xFF) {
      clcd.set_backlight(false);
      Wire.end();
      Wire.begin(SLAVE_ADDRESS);
      Wire.onReceive(receiveEvent);
      Wire.onRequest(requestEvent);
      delay(10);
      clcd.begin();
      delay(10);
      clcd.clear();
      delay(10);
      return;
    }
  }

  num_of_Bytes = (num_of_Bytes / 2) * 2;
  for (int i = 0; i < num_of_Bytes; i += 2) {
    if (Wire.available() < 2) {
      break;
    }
    uint8_t command_byte = Wire.read();
    uint8_t data_byte = Wire.read();

    if (command_byte == 0x00) {
      continue;
    } else if (command_byte == 0x01) {
      clcd.send_data(data_byte);

    } else if (command_byte == 0x02) {
      clcd.send_command(data_byte);

    } else if (command_byte == 0x03) {
      clcd.set_backlight(data_byte != 0x00);

    } else if (command_byte == 0x04) {
      byte_count = 0;
    }
  }
}

void requestEvent() {
  Wire.write(tx_data[byte_count]);
  if (byte_count == 7) {
    rotary_encoder_value = 0;
  }
  byte_count = (byte_count + 1) % 8;
}

void loop() {
  static unsigned long hid_send_timer = 0;
  static int count = 0;

  rotaryEncoder.updateRotation();
  rotaryEncoder.updateClick();
  int direction = rotaryEncoder.getDirection();
  rotary_encoder_value += direction;

  tx_data[0] = rotaryEncoder.isPressingDown() ? 0x01 : 0x00;
  tx_data[1] = (uint8_t)rotary_encoder_value;

  if (millis() - hid_send_timer >= 20) {
      hid_send_timer = millis();
      constexpr uint16_t bufsize = 8;
      uint8_t buf[bufsize] = {};
      buf[0] = rotaryEncoder.isPressingDown() ? 0x01 : 0x00;
      buf[1] = (uint8_t)rotary_encoder_value;
      
      if (usb_hid.ready() && connected_usb && memcmp(buf, prev_report, in_report_length) != 0) {
        usb_hid.sendReport(0, buf, bufsize);
        rotary_encoder_value = 0;
        memcpy(prev_report, buf, in_report_length);
      }
  }

  if (!connected()) {
    // Display sample text
    clcd.set_cursor(1, 0);
    clcd.printf("[%4d] %s", rotary_encoder_value,
                rotaryEncoder.isPressingDown() ? "PRESS" : "     ");
    if (rotaryEncoder.isPressingDown()) { 
      rotary_encoder_value = 0;
    }
  }
}


// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  // not used in this example
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  memset(buffer, 0x00, reqlen);
  buffer[0] = rotaryEncoder.isPressingDown() ? 0x01 : 0x00;
  buffer[1] = (uint8_t)rotaryEncoder.getDirection();

  return 8;
}


/** SET_REPORT structure
Repeat the following 2-bytes
- Byte 0: 0x00=Skip, 0x01=Data byte, 0x02=Command byte, 0x03=SetBacklight, 0xaa=Bootloader(0xaa,0xff)
- Byte 1: Command byte / Data byte / Backlight value
*/


// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  // This example doesn't use multiple report and report ID
  (void) report_id;
  (void) report_type;

  if (!connected_usb) {
    clcd.clear();
  }
  connected_usb = true;

  for (uint16_t i = 0; i < bufsize / 2; i++) {
    const uint8_t b0 = buffer[i * 2 + 0];
    const uint8_t b1 = buffer[i * 2 + 1];
    
    if (b0 == 0x00) {
      continue;

    } else if (b0 == 0x01) {
      clcd.send_data(b1);

    } else if (b0 == 0x02) {
      clcd.send_command(b1);

    } else if (b0 == 0x03) {
      clcd.set_backlight(b1);
    
    } else if (b0 == 0xaa && b1 == 0xff) {
      __disable_irq();
      TinyUSBDevice.detach();
      delay(10);
      uint32_t bootloader_address = *((uint32_t*)0x1fff8000 + 4);
      void (*reset_bootloader)(void) = bootloader_address;
      reset_bootloader();
      while (true) {}
    }
  }
}

void sprint_u32_hex(uint32_t val, char* buf) {
  constexpr char chs[16] = "0123456789abcdef";
  for (int i = 0; i < 8; i++) {
    const uint8_t v = (val >> ((7 - i) * 4)) & 0x0f;
    const char ch = chs[v];
    *buf++ = ch;
  }
}

/** Write a unique ID to 24-bytes buffer */
void get_unique_id(char* buf) {
  const uint32_t U_ID_BASE_ADDRESS = 0x1FFFF7E8;
  const uint32_t unique_id_1 = *(volatile uint32_t*)(U_ID_BASE_ADDRESS + 0);
  const uint32_t unique_id_2 = *(volatile uint32_t*)(U_ID_BASE_ADDRESS + 4);
  const uint32_t unique_id_3 = *(volatile uint32_t*)(U_ID_BASE_ADDRESS + 8);

  sprint_u32_hex(unique_id_1, buf + 0);
  sprint_u32_hex(unique_id_2, buf + 8);
  sprint_u32_hex(unique_id_3, buf + 16);
}
