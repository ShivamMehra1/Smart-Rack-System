#pragma once
#include <Arduino.h>

// This Arduino IDE sketch is wired for PN532 over SPI.
#define RFID_READER_PN532_SPI

// ESP32 DevKit V1 (classic ESP32) pin assignments.
#define PIN_I2C_SDA       21
#define PIN_I2C_SCL       22
#define LCD_DEFAULT_ADDR  0x27
#define LCD_COLS          16
#define LCD_ROWS          2

// MFRC522 and PN532 SPI use the ESP32 VSPI pins. Only one reader is connected.
#define PIN_RFID_SS       27
#define PIN_RFID_RST      26
#define PIN_RFID_SCK      18
#define PIN_RFID_MISO     19
#define PIN_RFID_MOSI     23
#define PIN_PN532_SPI_SS  PIN_RFID_SS
#define PIN_PN532_IRQ     255 // Optional; not connected when polling over I2C
#define PIN_PN532_RESET   26

// Optional feedback hardware. The buzzer is disabled by default.
#define PIN_CONFIRM_BTN   32
#define PIN_BUZZER        -1
#define PIN_LED_SUCCESS   25
#define PIN_LED_ERROR     33

// Maximum rack limits for memory safety
#define MAX_COMPARTMENTS  100
#define MAX_STUDENTS      200
#define MAX_HISTORY       100

// Local setup access point and optional internet uplink defaults.
#define DEFAULT_AP_SSID   "ECE-SMART-RACK"
#define DEFAULT_AP_PASS   "1122334455"
#define HTTP_PORT         80
#define UTC_OFFSET_SECONDS 19800 // IST; edit for your installation timezone
