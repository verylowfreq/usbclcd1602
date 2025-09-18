/**
 MIT License, Copyright (c) 2025 Mitsumine Suzu / verylowfreq
 Please refer LICENSE file for full text.
 **/

#include "USBH_CLCD.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF
//--------------------------------------------------------------------+
#define USB_CLCD_VID 0xf055
#define USB_CLCD_PID 0x6584

// HID Command definitions
#define CMD_LCD_DATA_TRANSFER 0x01
#define CMD_LCD_COMMAND_TRANSFER 0x02
#define CMD_BACKLIGHT_CONTROL 0x03

// LCD Command definitions
#define LCD_CMD_CLEAR_DISPLAY 0x01
#define LCD_CMD_SET_DDRAM_ADDR 0x80


bool USBH_CLCD::begin(void) {
  // This is for API consistency, no hardware is touched here.
  return true;
}

void USBH_CLCD::update(void) {
  if (available() && (millis() - _last_poll_ms >= 20)) {
    _last_poll_ms = millis();
    tuh_hid_receive_report(_dev_addr, _instance);
  }
}

bool USBH_CLCD::available(void) {
  if (_dev_addr == 0) {
    return false;
  }
  for (unsigned long start = millis(); millis() - start < 100;) {
    if (tuh_ready(_dev_addr)) {
      return true;
    } else {
      _usb_host.task();
      yield();
    }
  }
  return false;
}

void USBH_CLCD::clear(void) {
  send_command(CMD_LCD_COMMAND_TRANSFER, LCD_CMD_CLEAR_DISPLAY);
  delay(5); // clear display command takes longer
}

void USBH_CLCD::setCursor(uint8_t row, uint8_t col) {
  row = row % 2;
  col = col % 16;
  uint8_t address = 0x40 * row + col;
  send_command(CMD_LCD_COMMAND_TRANSFER, LCD_CMD_SET_DDRAM_ADDR | address);
}

void USBH_CLCD::backlight(bool on) {
  send_command(CMD_BACKLIGHT_CONTROL, on ? 0x01 : 0x00);
}

size_t USBH_CLCD::write(uint8_t c) {
  if (!available()) {
    Serial.println("not available() in write()");
    return 0;
  }
  uint8_t report[2] = {CMD_LCD_DATA_TRANSFER, c};
  send_packet(report, 2);
  return 1;
}

size_t USBH_CLCD::write(const uint8_t* buffer, size_t len) {

  size_t sent_len = 0;
  while (sent_len < len) {
    size_t copy_len = len < 32 ? len : 32;
    uint8_t buf[64] = {};
    for (size_t i = 0; i < copy_len; i++) {
      buf[i * 2 + 0] = CMD_LCD_DATA_TRANSFER;
      buf[i * 2 + 1] = buffer[i];
    }
    send_packet(buf, sizeof(buf));
    buffer += copy_len;
    sent_len += copy_len;
  }
  return sent_len;
}

bool USBH_CLCD::isPressed(void) {
  return _is_pressed;
}

int8_t USBH_CLCD::readRotation(void) {
  int8_t rotation = _rotation_accumulator;
  _rotation_accumulator = 0;
  return rotation;
}

void USBH_CLCD::send_packet(const uint8_t* packet, uint16_t len) {
  if (!available()) return;

  _is_sending = true;
  
  // For single commands, pad to a full report
  if (len < 64) {
    uint8_t report[64] = {0};
    memcpy(report, packet, len);
    tuh_hid_send_report(_dev_addr, _instance, 0, report, sizeof(report));
  } else {
    tuh_hid_send_report(_dev_addr, _instance, 0, packet, len);
  }
  for (unsigned long start = millis(); millis() - start < 100 && _is_sending;) {
    _usb_host.task();
    yield();
  }
}

void USBH_CLCD::send_command(uint8_t command, uint8_t data) {
  uint8_t report[2] = {command, data};
  send_packet(report, 2);
}


//--------------------------------------------------------------------+
// Callbacks
//--------------------------------------------------------------------+

bool USBH_CLCD::mount_cb(uint8_t dev_addr) {
  uint16_t vid, pid;
  if (tuh_vid_pid_get(dev_addr, &vid, &pid) && vid == USB_CLCD_VID && pid == USB_CLCD_PID) {
    _dev_addr = dev_addr;
    // Assuming the first HID interface is the one we want
    _instance = 0; 
    
    // Report reception is now handled by update()
    return true;
  }
  return false;
}

void USBH_CLCD::umount_cb(uint8_t dev_addr) {
  if (dev_addr == _dev_addr) {
    _dev_addr = 0;
    _instance = 0;
    _is_pressed = false;
    _rotation_accumulator = 0;
  }
}

void USBH_CLCD::report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  if (dev_addr == _dev_addr && instance == _instance && len >= 2) {
    _is_pressed = (report[0] == 0x01);
    _rotation_accumulator += (int8_t)report[1];
    
    // Next report is requested by update() polling
  }
}


void USBH_CLCD::report_sent_cb(uint8_t dev_addr, uint8_t idx, uint8_t const* report, uint16_t len) {
  if (dev_addr == _dev_addr && _instance == idx) {
    _is_sending = false;
  }
}
