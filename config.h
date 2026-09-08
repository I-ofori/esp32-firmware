#pragma once
#include <Arduino.h>

// =============================================================================
//  CONFIGURATION FILE — all defaults are here
//  Change values below when you confirm the real wiring from the KiCad
//  schematic. See README.md for the full list of defaults and notes.
// =============================================================================

// ---- Connectivity mode ------------------------------------------------------
// USE_WIFI = false, USE_GSM = true -> GSM-only (data sent by SIM800L)
// USE_WIFI = true,  USE_GSM = true -> WiFi preferred, GSM fallback
#define USE_WIFI  false
#define USE_GSM   true

// ---- WiFi (only used if USE_WIFI = true) ------------------------------------
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// ---- Backend ----------------------------------------------------------------
#define API_ENDPOINT   "https://data-logger-production.up.railway.app/api/v1/readings"
#define API_DEVICE_KEY "solar-pv-device-key-2026"   // must match Railway API_DEVICE_KEY
#define DEVICE_ID      "PVLOGGER_001"               // must exist in the devices table

// ---- GSM HTTP target --------------------------------------------------------
// IMPORTANT: SIM800L AT HTTP commands do NOT support TLS/HTTPS. Railway only
// serves HTTPS, so the GSM path must post to a plain-HTTP relay that forwards
// to API_ENDPOINT. Set GSM_HTTP_ENDPOINT to your relay (e.g. on a cheap VPS or
// a serverless HTTP->HTTPS proxy). Leave as API_ENDPOINT only if your SIM800
// firmware privides working AT+HTTPPARA SSL support (rare/unreliable).
#define GSM_HTTP_ENDPOINT  "http://altaria.proxy.rlwy.net:45035/api/v1/readings"
#define RELAY_SECRET       "348ba97f7ef341ca8f75b5bf4d54a0a4c54a73ab7ede926b"
// ---- Timing -----------------------------------------------------------------
#define POST_INTERVAL_SEC  30   // seconds between POSTs (backend limit: 30 req/min/IP)
#define WIFI_RETRY_MS      5000

// ---- TLS --------------------------------------------------------------------
// USE_CA_CERT = 0 -> setInsecure() (skips cert verification, easiest for testing)
// USE_CA_CERT = 1 -> verify server with CA_CERT below (production, recommended)
#define USE_CA_CERT  0
#ifndef CA_CERT
#define CA_CERT "-----BEGIN CERTIFICATE-----\nPASTE_REAL_CERT_HERE\n-----END CERTIFICATE-----\n"
#endif

// ---- Analog pins (assumed ADC, 0-3.3V) --------------------------------------
#define PIN_ADC_CURRENT   34   // ACS712  output C_OUT
#define PIN_ADC_VOLTAGE   35   // Voltage sensor output V_OUT

// ---- DHT22 -------------------------------------------------------------
#define PIN_DHT_DATA      4    // DHT22 digital DATA pin

// ---- MAX31865 (RTD / PT100 via SPI) -----------------------------------
#define PIN_RTD_CS        5
#define PIN_RTD_SDI       23
#define PIN_RTD_SDO       19
#define PIN_RTD_SCK       18

// ---- I2C bus (TSL2591, RTC ZC261500, AT24C256, LCD) ------------------
#define I2C_SDA           21
#define I2C_SCL           22

// ---- SIM800L (GSM) -------------------------------------------------------
// Used for sending readings and (in GSM-only mode) for time sync.
#define PIN_GSM_TX        17   // ESP32 TX -> SIM800L RXD
#define PIN_GSM_RX        16   // ESP32 RX <- SIM800L TXD
#define GSM_BAUD          9600
#define GSM_UART_NUM      1    // ESP32 hardware UART1 for the SIM800L
#define GSM_APN           "internet"     // e.g. "web.vodafone", "internet"
#define GSM_USER          ""
#define GSM_PASS          ""

// ---- SIM800 SNTP (unused) -------------------------------------------------
// Time is stamped by the backend on receipt, so the device needs no clock.
// (Kept for reference only; remove freely.)

// ---- Feature toggles ---------------------------------------------------------
#define USE_DHT22          true
#define USE_TSL2591        true
#define USE_MAX31865       true
#define USE_RTC            false

// ---- Calibration ------------------------------------------------------------
// ACS712 (5A module): outputs Vcc/2 at 0A, sensitivity 185 mV/A (5A version).
// If your shield powers it at 5V, midpoint = 2.5V.
#define ACS712_SENSITIVITY_V_PER_A   0.185
#define ACS712_MIDPOINT_VOLTS        2.5

// Generic 25V voltage-sensor module: Vout = Vin * ratio (common ratio 0.2).
// Adjust to match your shield's divider.
#define VOLTAGE_SENSOR_RATIO         0.2

// TSL2591: returns lux. Approximate sunlight conversion to W/m2.
// 1 W/m2 ~ 120 lux (full spectrum). Not a calibrated pyranometer.
#define LUX_PER_W_M2                 120.0

// MAX31865 PT100 reference resistor and nominal RTD resistance
#define RTD_RREF_OHMS                430.0
#define RTD_RNOM_OHMS                100.0