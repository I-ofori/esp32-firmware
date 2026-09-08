# Solar PV Logger — ESP32 Firmware

Reposible for reading the shield sensors and POSTing readings to the deployed backend:

```
POST https://data-logger-production.up.railway.app/api/v1/readings
```

> All configurable values live in **`config.h`**. This file lists every default,
> what it does, and how to change it once you confirm the real KiCad wiring.

---

## Connectivity: GSM-only (default) — no WiFi

This project runs **GSM-only**. Readings are sent through the **SIM800L** module;
WiFi is compiled out (`USE_WIFI = false`). Set `USE_GSM = true` (default).

### Send logic

```
Every POST_INTERVAL_SEC seconds:
  read sensors
  build JSON payload (no timestamp - the server stamps it on receipt)
  POST via SIM800L GSM  ->  GSM_HTTP_ENDPOINT
```

### Timestamps: the server owns the clock

The device sends **no `recorded_at`** field — the backend stamps each reading
with the current server time (`NOW()`) when it arrives. This means:

- No RTC chip and no GSM SNTP sync are needed on the device.
- No risk of 1970 "epoch" timestamps if a clock sync fails.
- Timestamps reflect arrival time; for a live 30-second logger this equals
  measurement time.

### GSM configuration in `config.h`

| Key | Default | Meaning |
|---|---|---|
| `USE_GSM` | `true` | Master switch for the GSM path |
| `USE_WIFI` | `false` | Set `true` only if you later add a WiFi path |
| `PIN_GSM_TX` / `PIN_GSM_RX` | `17` / `16` | ESP32 UART1 TX/RX wired to SIM800L |
| `GSM_BAUD` | `9600` | SIM800L default baud |
| `GSM_UART_NUM` | `1` | Which ESP32 hardware UART the SIM800 uses |
| `GSM_APN` | `"YOUR_APN"` | Your carrier's APN (e.g. `web.vodafone`) — **must set** |
| `GSM_USER` / `GSM_PASS` | `""` | APN username/password if required by carrier |
| `GSM_HTTP_ENDPOINT` | the Railway URL | **See HTTPS caveat below** |

### Critical: SIM800L cannot do HTTPS

The SIM800L's AT HTTP stack (`AT+HTTPPARA`/`AT+HTTPACTION`) only supports plain
`http://`. Your Railway endpoint is HTTPS-only, so **the GSM path fails if pointed
directly at `API_ENDPOINT`**. Two workable solutions:

- **Recommended: an HTTP relay.** Deploy a tiny plain-HTTP endpoint (on a VPS or
  serverless function) that accepts the POST and forwards it over HTTPS to
  `API_ENDPOINT`, preserving the `Authorization` header. Point `GSM_HTTP_ENDPOINT`
  at that relay.
- **Skip GSM TLS entirely (only if your module's firmware supports**
  `AT+HTTPPARA="SSL"` **reliably — most SIM800L do not).** In that case leave
  `GSM_HTTP_ENDPOINT` as the Railway URL and test before trusting it.

### GSM wiring notes

- SIM800L needs **5V power capable of ~2A bursts** (during register/transmit).
  It cannot be powered from the ESP32 3.3V rail. Use a regulated 5V supply plus a
  large capacitor (e.g. 470–1000 µF) near the module.
- Confirm the SIM800L TXD → ESP32 RX and RXD → ESP32 TX wiring matches
  `PIN_GSM_RX` / `PIN_GSM_TX`.
- UART1 (`GSM_UART_NUM=1`) is allocated to the SIM800L; UART0 stays on serial for
  debug logs. If your board uses UART1 differently, change `GSM_UART_NUM`.

---

## Files

| File | Purpose |
|---|---|
| `solar-pv-esp32.ino` | Main sketch (sensor reads, SIM800L GSM POST, SNTP clock sync) |
| `config.h` | **All defaults you may need to change** |

---

## Libraries (Arduino IDE Library Manager)

| Library | Used for |
|---|---|
| ArduinoJson (v7) | Building the JSON payload |
| DHT sensor library + Adafruit Unified Sensor | DHT22 temp/humidity |
| Adafruit TSL2591 | Ambient light / irradiance |
| Adafruit MAX31865 | RTD (PT100) temperature |

`WiFi`, `HTTPClient`, `WiFiClientSecure` are built into the ESP32 core.

---

## Assumed hardware defaults (see `config.h`)

These are **assumed** based on typical ESP32-DEVKITC-32E shield wiring and the
KiCad labels (`C_OUT`, `V_OUT`, `DATA`, `SDA`, `SCL`, `CS`, ...). They are
**not confirmed** from an explicit pin-to-pin net list.

| Signal | Schematic label | GPIO default | Notes |
|---|---|---|---|
| ACS712 current analog out | `C_OUT` | **34** | ADC (0–3.3V) |
| Voltage divider analog out | `V_OUT` | **35** | ADC (0–3.3V) |
| DHT22 data | `DATA` | **4** | Digital |
| MAX31865 chip select | `CS` | **5** | SPI |
| MAX31865 serial data in | `SDI` | **23** | SPI MOSI |
| MAX31865 serial data out | `SDO` | **19** | SPI MISO |
| MAX31865 clock | `SCK`/`SCLK` | **18** | SPI |
| I2C data (TSL2591, RTC, EEPROM, LCD) | `SDA` | **21** | I2C |
| I2C clock | `SCL` | **22** | I2C |
| SIM800L TX (to ESP32 RX) | `GPS_RX` | **16** | Not used yet |
| SIM800L RX (from ESP32 TX) | `GPS_TX` | **17** | Not used yet |

**To change a pin:** edit the matching `#define PIN_...` in `config.h`.

---

## Calibration defaults

| Constant | Default | Meaning |
|---|---|---|
| `ACS712_SENSITIVITY_V_PER_A` | `0.185` | 185 mV/A on the 5A ACS712 module |
| `ACS712_MIDPOINT_VOLTS` | `2.5` | Output at 0 A = VCC/2 (2.5V @ 5V supply) |
| `VOLTAGE_SENSOR_RATIO` | `0.2` | Common 25V module: Vout = Vin × 0.2 |
| `LUX_PER_W_M2` | `120.0` | Approx solar conversion: 1 W/m² ≈ 120 lux |
| `RTD_RREF_OHMS` / `RTD_RNOM_OHMS` | `430` / `100` | MAX31865 reference & PT100 nominal |

**Important caveats for accuracy:**
- **Voltage**: read the actual divider resistors on the shield and set
  `VOLTAGE_SENSOR_RATIO` to the real ratio (`R2 / (R1 + R2)`), or calculate
  `Vin = Vadc / ratio`. A basic 25V module uses 30k/7.5k → ratio 0.2.
- **Current**: confirm the ACS712 variant (5A: 185mV/A, 20A: 100mV/A, 30A:
  66mV/A) and its supply voltage (`ACS712_MIDPOINT_VOLTS` = VCC/2).
- **Irradiance**: the TSL2591 is an *ambient light* sensor, not a pyranometer.
  The lux→W/m² factor is approximate. Swap `LUX_PER_W_M2` to match a
  calibration against a real reference if available.

---

## Backend contract (must match the API exactly)

- **Endpoint:** `POST /api/v1/readings`
- **Auth header:** `Authorization: Bearer <API_DEVICE_KEY>`
- **Content-Type:** `application/json`
- **Payload (required fields):**

```json
{
  "device_id": "PVLOGGER_001",
  "voltage": 24.1,
  "current": 3.5,
  "temperature": 32.4,
  "irradiance": 850.0,
  "humidity": 45.0,
  "recorded_at": "2026-09-07T08:43:00.000Z"
}
```

- `recorded_at` must be ISO 8601 **with timezone offset** (the sketch sends
  UTC with `.000Z`). Never send a naive timestamp without offset.
- `power` is optional — the backend computes `power = voltage × current`.
- `device_id` must already exist in the Supabase `devices` table (FK constraint).
  `PVLOGGER_001` is pre-inserted by `schema.sql`.

### Response codes to expect

| Code | Meaning | Sketch log |
|---|---|---|
| 201 | Stored successfully | `Reading stored OK` |
| 400 | Bad payload or device not registered | prints response body |
| 401 | Wrong device key | `API_DEVICE_KEY mismatch` |
| 409 | Duplicate `device_id` + same `recorded_at` | `duplicate` |
| 429 | Exceeded rate limit (30 req/min/IP) | `rate limited` |

---

## What to change when you get the real schematic info

1. **Pin numbers** — replace the `#define PIN_...` values in `config.h` with the
   actual ESP32 GPIO numbers traced in KiCad.
2. **ADC calibration** — set `VOLTAGE_SENSOR_RATIO` and
   `ACS712_*` constants to the real shield values.
3. **Credentials** — `API_DEVICE_KEY` must match the Railway env var
   `API_DEVICE_KEY`; set `GSM_APN` and (if needed) `GSM_USER`/`GSM_PASS` to your
   carrier's values.
4. **GSM endpoint** — point `GSM_HTTP_ENDPOINT` at your HTTP→HTTPS relay
   (required, since SIM800L cannot do TLS itself).
5. **TLS (if WiFi is ever enabled)** — set `USE_WIFI = true`, then
   `USE_CA_CERT = 1` and paste Railway's real CA certificate into `CA_CERT`.
   For testing, `setInsecure()` is fine.

---

## Optional modules on the shield (not wired into the code)

| Part | Present on shield | Used? |
|---|---|---|
| ZC261500 RTC (J10) | Yes | No — NTP used instead. Enable with `USE_RTC true` |
| AT24C256 EEPROM (J8) | Yes | No |
| LCD (J4) | Yes | No |
| SIM800L GSM (U5) | Yes | **Yes — primary GSM path** (see Connectivity section). Requires an HTTP→HTTPS relay |
| HW-669 regulator (J9) | Yes | Power only |