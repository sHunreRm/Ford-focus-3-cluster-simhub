// Ford Focus MK3 (BM5T) / bench cluster emulator
// v6.8 — база v6.3 + два фикса
//
// РУЧНИК — найдена причина: переменная handbrake не использовалась в 0x240!
//   В sendHandbrake() было статичное byte3 = 0x40, теперь с битом из переменной
//   В sendIndicators() убираем |=0x40 из 0x03A — это был ложный бит ручника
//
// ПЕРЕГРЕВ → ручник: зажимаем температуру в 0x320 на 105С
//   (как уже делали в v5.9)
//   Стрелка термометра по 0x360 пойдёт в красную зону без ограничений

#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

#define CAN_CS 15
MCP_CAN CAN(CAN_CS);

#define ABS_FORCE_ON false

int  speed_kmh      = 0;
int  rpm            = 0;
int  fuel_percent   = 50;
int  coolant_temp   = 85;
int  outside_temp_c = 20;

bool left_turn      = false;
bool right_turn     = false;
bool handbrake      = false;
bool abs_req        = false;
bool esp_active     = false;
bool tc_off         = false;
bool brake_pedal    = false;
bool check_engine   = false;
bool head_lights    = false;
bool high_beam      = false;
bool fog_light      = false;
bool low_fuel_sh    = false;

bool abs_active              = false;
bool abs_blink_state         = false;
unsigned long abs_request_since = 0;
unsigned long abs_active_since  = 0;
unsigned long abs_blink_timer   = 0;

int  prev_speed = 0;
unsigned long prev_speed_time = 0;
float deceleration = 0.0f;

String serialBuffer;
uint32_t fuel_update_count = 0;

unsigned long t10    = 0;
unsigned long t50    = 0;
unsigned long t100   = 0;
unsigned long t120   = 0;
unsigned long t300   = 0;
unsigned long t1000  = 0;
unsigned long tDiag  = 0;

bool sendCAN(uint32_t id, const uint8_t *data, uint8_t len = 8) {
  return CAN.sendMsgBuf(id, 0, len, (uint8_t*)data) == CAN_OK;
}

// 0x03A — поворотники + ESP (БЕЗ ручника!)
// Раньше был ложный бит handbrake — убрал, ручник теперь только в 0x240
void sendIndicators() {
  uint8_t byte1 = 0x83;
  if (left_turn)    byte1 |= 0x04;
  if (right_turn)   byte1 |= 0x08;
  if (esp_active)   byte1 |= 0x20;
  // УБРАНО: if (handbrake) byte1 |= 0x40;  ← это был не тот бит

  uint8_t d[8] = {0x82, byte1, 0x00, 0x02, 0x80, 0x00, 0x00, 0x00};
  sendCAN(0x03A, d);
}

void sendAirbag() {
  uint8_t d[8] = {0x66, 0x02, 0x80, 0xFF, 0xBE, 0x00, 0x80, 0x00};
  sendCAN(0x040, d);
}

void sendSeatbelt() {
  uint8_t d[8] = {0xD0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  sendCAN(0x060, d);
}

void sendABS_ESC() {
  uint8_t byte1 = 0x98;
  uint8_t byte2 = 0x04;
  uint8_t byte4 = 0x10;

  if (ABS_FORCE_ON || (abs_active && abs_blink_state)) {
    byte2 = 0x14;
    byte4 = 0x40;
  }

  uint8_t d[8] = {0x00, byte1, byte2, 0x80, byte4, 0xF4, 0xE8, 0x10};
  sendCAN(0x070, d);
}

void sendWakeRunning() {
  uint8_t b1 = (head_lights ? 0x80 : 0x00) | 0x03;
  uint8_t d[8] = {0x77, b1, 0x07, 0x3F, 0xD9, 0x06, 0x03, 0x81};
  sendCAN(0x080, d);
}

void sendSpeedRPM() {
  int safe_rpm   = max(0, rpm);
  int safe_speed = max(0, speed_kmh);

  uint16_t rpm_raw = (uint16_t)(safe_rpm / 2);
  uint16_t spd_raw = (uint16_t)(safe_speed * 95);

  uint8_t d[8] = {
    0xC0, 0x03, 0x0A, 0x00,
    (uint8_t)((rpm_raw >> 8) | 0xC0),
    (uint8_t)(rpm_raw & 0xFF),
    (uint8_t)(spd_raw >> 8),
    (uint8_t)(spd_raw & 0xFF)
  };
  sendCAN(0x110, d);
}

void sendOutsideTemp() {
  int t = constrain(outside_temp_c, -40, 63);
  uint16_t raw = (uint16_t)(512 + (t * 4));
  uint8_t d[8] = {
    0x00, 0x00, 0x00,
    (uint8_t)(raw >> 8),
    (uint8_t)(raw & 0xFF),
    0x80, 0x00, 0x01
  };
  sendCAN(0x1A4, d);
}

void sendHillStart() {
  uint8_t d[8] = {0x01, 0x50, 0x27, 0x00, 0x00, 0x00, 0x80, 0x00};
  sendCAN(0x1B0, d);
}

void sendLights() {
  uint8_t b0 = 0x00;
  if (head_lights)    b0 = 0x80;
  else if (fog_light) b0 = 0x20;
  uint8_t b1 = high_beam ? 0x80 : 0x00;
  uint8_t d[8] = {b0, b1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  sendCAN(0x1B4, d);
}

void sendImmobiliser() {
  uint8_t d[8] = {0x42, 0x80, 0x38, 0x00, 0x80, 0x00, 0x80, 0x00};
  sendCAN(0x1E0, d);
}

// 0x240 — ручник (ИСПРАВЛЕНО!)
// Формула из bigunclemax: byte3 = 0x40 | (brakeApplied << 7)
//   ВЫКЛ: byte3 = 0x40
//   ВКЛ:  byte3 = 0xC0 (бит 7 = 0x80 + базовый 0x40)
// Раньше я не использовал переменную handbrake вообще — баг
void sendHandbrake() {
  uint8_t b3 = handbrake ? 0xC0 : 0x40;
  uint8_t d[8] = {0x00, 0x02, 0x00, b3, 0x00, 0x00, 0x00, 0x00};
  sendCAN(0x240, d);
}

void sendEngineOil() {
  uint8_t d[8] = {0x20, 0xD5, 0x18, 0x04, 0x1C, 0x11, 0x00, 0x00};
  sendCAN(0x250, d);
}

void sendBrakeFluidWasher() {
  uint8_t d[8] = {0x98, 0x00, 0x01, 0x00, 0x04, 0x03, 0x92, 0x37};
  sendCAN(0x290, d);
}

void sendEngineStatus() {
  uint8_t d[8] = {0x06, 0x00, 0x00, 0x00, 0x3E, 0x4F, 0x80, 0xE5};
  sendCAN(0x2A0, d);
}

void sendAlternator() {
  uint8_t d[8] = {0x00, 0x00, 0x00, 0x21, 0xC0, 0x00, 0x00, 0x00};
  sendCAN(0x300, d);
}

void sendABS_213() {
  uint8_t byte0 = 0x00;
  uint8_t byte1 = 0x00;

  if (ABS_FORCE_ON || (abs_active && abs_blink_state)) {
    byte0 = 0xFF;
    byte1 = 0xFF;
  }

  uint8_t d[8] = {byte0, byte1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  sendCAN(0x213, d);
}

// 0x320 — топливо + температура
// ИЗМЕНЕНИЕ: температура для 0x320 ограничена 105С
// Стрелка термометра идёт по 0x360 без ограничений
// Это должно убрать ложное зажигание ручника при перегреве
void sendFuel() {
  int safe_fuel = constrain(fuel_percent, 0, 100);

  // Температура для 0x320 ОГРАНИЧЕНА 105С
  int safe_temp_for_320 = constrain(coolant_temp, -40, 105);

  uint16_t fuel_val = 7936 - (safe_fuel * 53 / 10);
  uint8_t fuel_h = ((fuel_val >> 8) & 0xFF) | 0x10;
  uint8_t fuel_l = fuel_val & 0xFF;

  uint16_t temp_val = (safe_temp_for_320 + 40) * 10;
  uint8_t temp_h = (temp_val >> 8) & 0xFF;
  uint8_t temp_l = temp_val & 0xFF;

  uint8_t b4 = 0x10;
  uint8_t b5 = 0x01;
  if (fuel_percent <= 10) {
    b4 = 0x01;
    b5 = 0x00;
  } else if (fuel_percent <= 12) {
    b4 = 0x00;
    b5 = 0x00;
  } else if (low_fuel_sh && fuel_percent <= 15) {
    b4 = 0x01;
    b5 = 0x00;
  }

  uint8_t d[8] = {
    temp_h, temp_l,
    fuel_h, fuel_l,
    b4, b5, 0x00, 0x00
  };
  sendCAN(0x320, d);
}

// 0x360 — стрелка термометра, БЕЗ ограничений (полная температура)
void sendEngineTemp() {
  int safe_temp = constrain(coolant_temp, -60, 195);
  uint8_t temp  = (uint8_t)(safe_temp + 60);
  uint8_t d[8]  = {0xE0, 0x00, 0x38, 0x40, 0x00, 0xE0, 0x69, temp};
  sendCAN(0x360, d);
}

void sendWheel_4B0() {
  int safe_speed = max(0, speed_kmh);
  uint16_t wheel = (uint16_t)(safe_speed * 96);
  uint8_t hi = (wheel >> 8) & 0xFF;
  uint8_t lo = wheel & 0xFF;
  uint8_t d[8] = {hi, lo, hi, lo, hi, lo, hi, lo};
  sendCAN(0x4B0, d);
}

void updateDeceleration() {
  unsigned long now = millis();
  if (prev_speed_time == 0) {
    prev_speed_time = now;
    prev_speed = speed_kmh;
    return;
  }

  unsigned long dt_ms = now - prev_speed_time;
  if (dt_ms < 100) return;

  int dv = prev_speed - speed_kmh;
  deceleration = (float)dv * 1000.0f / (float)dt_ms;

  prev_speed = speed_kmh;
  prev_speed_time = now;
}

void updateAbsLogic() {
  unsigned long now = millis();

  bool abs_should_fire = abs_req && brake_pedal && speed_kmh > 30 && deceleration > 8.0f;

  if (abs_should_fire) {
    if (abs_request_since == 0) abs_request_since = now;
    if (!abs_active && (now - abs_request_since) > 150) {
      abs_active = true;
      abs_active_since = now;
    }
  } else {
    abs_request_since = 0;
    if (abs_active && (now - abs_active_since) > 200) {
      abs_active = false;
      abs_blink_state = false;
    }
  }

  if (abs_active && (now - abs_blink_timer > 180)) {
    abs_blink_timer = now;
    abs_blink_state = !abs_blink_state;
  }
}

void startupBurst() {
  for (int i = 0; i < 60; i++) {
    sendIndicators();
    sendAirbag();
    sendSeatbelt();
    sendABS_ESC();
    sendWakeRunning();
    sendSpeedRPM();
    sendOutsideTemp();
    sendEngineOil();
    sendWheel_4B0();
    if ((i % 2) == 0) { sendHillStart(); sendImmobiliser(); sendLights(); }
    if ((i % 3) == 0) { sendHandbrake(); }
    if ((i % 6) == 0) { sendBrakeFluidWasher(); sendEngineStatus(); sendABS_213(); }
    if ((i % 8) == 0) { sendAlternator(); sendFuel(); sendEngineTemp(); }
    delay(20);
    yield();
  }
}

void parseSimHub() {
  serialBuffer.trim();
  if (serialBuffer.length() < 4) return;

  int pos = 0;
  while (pos < (int)serialBuffer.length()) {
    int colon = serialBuffer.indexOf(':', pos);
    if (colon < 0) break;

    String key = serialBuffer.substring(pos, colon);
    key.trim();
    key.toUpperCase();

    int semi = serialBuffer.indexOf(';', colon + 1);
    if (semi < 0) semi = serialBuffer.length();

    String v = serialBuffer.substring(colon + 1, semi);
    v.trim();

    float val_f = v.toFloat();
    int   val   = (int)val_f;

    if      (key == "FUEL") {
      int new_fuel = (val_f <= 1.5f) ? constrain((int)(val_f*100+0.5f),0,100) : constrain(val,0,100);
      if (new_fuel != fuel_percent) {
        fuel_update_count++;
        fuel_percent = new_fuel;
      }
    }
    else if (key == "SPD" || key == "SPEED")      { speed_kmh     = max(0, val); }
    else if (key == "RPM")                        { rpm            = max(0, val); }
    else if (key == "COOLANT" || key == "WATERT" || key == "WATERTEMP") { coolant_temp = val; }
    else if (key == "OUTTEMP" || key == "AMB" || key == "AMBIENT")      { outside_temp_c = val; }
    else if (key == "L")                          { left_turn     = (val != 0); }
    else if (key == "R")                          { right_turn    = (val != 0); }
    else if (key == "HB" || key == "HANDBRAKE")   { handbrake     = (val != 0); }
    else if (key == "ABS")                        { abs_req       = (val != 0); }
    else if (key == "BRAKE")                      { brake_pedal   = (val != 0); }
    else if (key == "ESP" || key == "TCS")        { esp_active    = (val != 0); }
    else if (key == "TCOFF" || key == "ESPOFF")   { tc_off        = (val != 0); }
    else if (key == "CEL" || key == "CHECKENG")   { check_engine  = (val != 0); }
    else if (key == "LIGHTS")                     { head_lights   = (val != 0); }
    else if (key == "HIBEAM")                     { high_beam     = (val != 0); }
    else if (key == "FOG")                        { fog_light     = (val != 0); }
    else if (key == "LOWFUEL")                    { low_fuel_sh   = (val != 0); }

    pos = semi + 1;
  }
}

void printDiag() {
  Serial.print("[DIAG] FUEL=");        Serial.print(fuel_percent);
  Serial.print(" CLT=");                Serial.print(coolant_temp);
  Serial.print(" HB=");                 Serial.print(handbrake);
  Serial.print(" | SPD=");              Serial.print(speed_kmh);
  Serial.print(" decel=");              Serial.print(deceleration, 1);
  Serial.print(" ABS_act=");            Serial.print(abs_active);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  serialBuffer.reserve(256);
  SPI.begin();

  if (CAN.begin(MCP_ANY, CAN_125KBPS, MCP_8MHZ) != CAN_OK) {
    Serial.println("CAN FAIL");
    while (1) { delay(100); yield(); }
  }
  CAN.setMode(MCP_NORMAL);

  Serial.println("=== Focus MK3 bench v6.8 ===");
  Serial.println("Base: v6.3");
  Serial.println("FIX 1: handbrake variable now used in 0x240");
  Serial.println("FIX 2: temp in 0x320 capped at 105C (no false handbrake)");
  Serial.println("CAN OK");

  delay(300);
  startupBurst();

  unsigned long now = millis();
  t10 = t50 = t100 = t120 = t300 = t1000 = tDiag = now;
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') { parseSimHub(); serialBuffer = ""; }
    else if (c != '\r') serialBuffer += c;
  }

  updateDeceleration();
  updateAbsLogic();

  unsigned long now = millis();

  if (now - t10 >= 10) {
    t10 = now;
    sendFuel();
  }

  if (now - t50 >= 50) {
    t50 = now;
    sendIndicators();
    sendAirbag();
    sendSeatbelt();
    sendABS_ESC();
    sendWakeRunning();
    sendSpeedRPM();
    sendOutsideTemp();
    sendEngineOil();
    sendWheel_4B0();
  }

  if (now - t100 >= 100) {
    t100 = now;
    sendHillStart();
    sendImmobiliser();
    sendLights();
    sendABS_213();
  }

  if (now - t120 >= 120) { t120 = now; sendHandbrake(); }
  if (now - t300 >= 300) { t300 = now; sendBrakeFluidWasher(); sendEngineStatus(); sendEngineTemp(); sendAlternator(); }
  if (now - t1000 >= 1000){ t1000= now; }
  if (now - tDiag >= 1000){ tDiag = now; printDiag(); }
}