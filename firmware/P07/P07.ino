// P07 · 딸기 하우스 습도 신호등
// DHT11로 습도를 재서 LCD1602에 표시하고 RGB LED 색으로 상태를 알립니다.
//   낮음(60% 미만) = 파랑 / 적정(60–85%) = 초록 / 높음(85% 이상) = 빨강
//   측정 전·센서 오류 = 파랑 깜빡임
//   낮음·높음으로 바뀌면 D8 부저가 울립니다 (높음 = 3번, 낮음 = 2번, 적정 = 안 울림)
//   습도 = 짧게, 온도(10–30 °C 밖) = 길게. 온도는 LED에 표시하지 않고 LCD 오른쪽에 TLO/THI로 표시
// 대시보드와는 USB 시리얼 115200, JSON 한 줄 규약으로 대화합니다(protocol.h).
//
// 필요한 라이브러리 (Arduino IDE → 라이브러리 매니저)
//   DHT sensor library 1.4.7 (Adafruit) + Adafruit Unified Sensor 1.1.15
//   LiquidCrystal I2C 1.1.2 (Frank de Brabander) — LCD 주소 0x27·0x3F 등을 자동으로 찾습니다
// 보드: Arduino Uno / 시리얼 모니터: 115200, 줄 끝 "새 줄"

#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include "pins.h"
#include "protocol.h"
#include "logic.h"

DHT dht(PIN_DHT, DHT11);
LiquidCrystal_I2C* lcd = nullptr;   // 주소를 찾은 뒤에 만듭니다

// ---------- 프로그램 상태 ----------
float humidity = 0, temperature = 0;  // 마지막으로 성공한 값
bool  sensorValid = false;            // false면 JSON에서 null
bool  everMeasured = false;           // 첫 측정 전인지 구분 (LCD 문구)
uint8_t dhtFailCount = 0;

Level level = LEVEL_NONE;             // 습도 상태 (LED 색)
uint8_t rhLow = DEFAULT_RH_LOW, rhHigh = DEFAULT_RH_HIGH;
Level tLevel = LEVEL_NONE;            // 온도 상태 (부저·LCD·대시보드만)
uint8_t tLow = DEFAULT_T_LOW, tHigh = DEFAULT_T_HIGH;
uint8_t bright = DEFAULT_BRIGHT;
bool lightOn = true;
bool buzzerOn = true;                 // false면 경보음을 내지 않음 (BUZZ 0)
uint8_t outR = 0, outG = 0, outB = 0;  // 마지막으로 낸 PWM 값
bool lcdReady = false;

unsigned long lastDhtMs = 0, lastStateMs = 0, lastLcdMs = 0, lastBlinkMs = 0;
bool blinkOn = true;                  // 센서 확인 상태의 파랑 깜빡임

Level alarmLevel = LEVEL_NONE;  // 마지막으로 경보를 판단한 습도 상태 (센서 오류 동안은 그대로 둠)
Level alarmTLevel = LEVEL_NONE; // 같은 방식의 온도 상태
uint8_t beepLeft = 0;           // 남은 울림 횟수
bool beepLong = false;          // true = 온도 경보(길게)
uint8_t nextBeeps = 0;          // 울리는 중에 들어온 다음 경보
bool nextLong = false;
unsigned long lastBeepMs = 0;
volatile uint8_t buzzDuty = 0;  // 0 = 조용, 1–10 = 켜는 칸 수 (타이머 인터럽트가 읽음)

char cmdBuf[CMD_BUFFER_SIZE];
uint8_t cmdLen = 0;
bool cmdOverflow = false;

// ---------- 출력 ----------
void writeChannel(uint8_t pin, uint8_t value) {
  // 공통 +형은 LOW일 때 켜지므로 값을 뒤집습니다.
  analogWrite(pin, RGB_COMMON_ANODE ? (uint8_t)(255 - value) : value);
}

void applyOutputs() {
  outR = outG = outB = 0;
  if (lightOn) {
    // 섞은 색은 모듈에서 점 두 개로 따로 보이므로 한 가지 색만 씁니다.
    if (level == LEVEL_LOW)  outB = bright;
    if (level == LEVEL_OK)   outG = bright;
    if (level == LEVEL_HIGH) outR = bright;
    // LEVEL_NONE(측정 전·센서 오류): 불이 꺼지지 않도록 파랑을 깜빡여 건조(파랑 켜짐)와 구별합니다.
    if (level == LEVEL_NONE && blinkOn) outB = bright;
  }
  writeChannel(PIN_RGB_R, outR);
  writeChannel(PIN_RGB_G, outG);
  writeChannel(PIN_RGB_B, outB);
}

// ---------- 부저 ----------
// 능동형 부저는 켜면 늘 최대 크기로 울립니다. 그래서 0.1 ms마다 켜고 끄기를 정해
// 1 ms 중 BUZZ_VOLUME/10만 켜 둡니다(소리 크기 조절). Timer2를 쓰므로 tone()·D3·D11 PWM과 함께 쓰지 않습니다.
ISR(TIMER2_COMPA_vect) {
  static uint8_t phase = 0;
  if (++phase >= 10) phase = 0;
  bool on = phase < buzzDuty;
  // 저신호 동작형은 LOW일 때 울리므로 켜기·끄기를 뒤집습니다.
  digitalWrite(PIN_BUZZER, (on != BUZZER_ACTIVE_LOW) ? HIGH : LOW);
}

void buzzerBegin() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, BUZZER_ACTIVE_LOW ? HIGH : LOW);  // 조용한 상태로 시작
  noInterrupts();
  TCCR2A = _BV(WGM21);   // CTC 모드
  TCCR2B = _BV(CS21);    // 분주 8 → 2 MHz
  OCR2A  = 199;          // 2 MHz / 200 = 10 kHz (0.1 ms마다 인터럽트)
  TIMSK2 = _BV(OCIE2A);
  interrupts();
}

// 습도 경보 = 짧게, 온도 경보 = 길게. 울리는 중에 다른 경보가 오면 끝난 뒤 이어서 울립니다.
void startAlarm(Level lv, bool isTemp) {
  uint8_t n = (lv == LEVEL_HIGH) ? BUZZ_BEEPS_HIGH : BUZZ_BEEPS_LOW;
  if (beepLeft) { nextBeeps = n; nextLong = isTemp; return; }
  beepLeft = n; beepLong = isTemp;
  lastBeepMs = millis() - BUZZ_LONG_PERIOD_MS;  // 첫 울림은 바로
}

void updateBuzzer(unsigned long now) {
  if (buzzDuty && now - lastBeepMs >= (beepLong ? BUZZ_LONG_ON_MS : BUZZ_ON_MS)) buzzDuty = 0;
  if (beepLeft == 0 && nextBeeps && now - lastBeepMs >= BUZZ_GAP_MS) {
    beepLeft = nextBeeps; beepLong = nextLong; nextBeeps = 0;
    lastBeepMs = now - BUZZ_LONG_PERIOD_MS;
  }
  if (beepLeft == 0 || now - lastBeepMs < (beepLong ? BUZZ_LONG_PERIOD_MS : BUZZ_PERIOD_MS)) return;
  lastBeepMs = now;
  beepLeft--;
  buzzDuty = BUZZ_VOLUME;
}

// 낮음·높음으로 "바뀔 때"만 울립니다. 센서가 잠깐 실패했다가 같은 상태로 돌아오면 다시 울리지 않습니다.
void checkAlarm() {
  if (!sensorValid) return;
  if (level != alarmLevel) {
    if (buzzerOn && (level == LEVEL_LOW || level == LEVEL_HIGH)) startAlarm(level, false);
    alarmLevel = level;
  }
  if (tLevel != alarmTLevel) {
    if (buzzerOn && (tLevel == LEVEL_LOW || tLevel == LEVEL_HIGH)) startAlarm(tLevel, true);
    alarmTLevel = tLevel;
  }
}

void printLevelOrNull(Level lv) {
  const char* ln = levelName(lv);
  if (ln) { Serial.print('"'); Serial.print(ln); Serial.print('"'); } else Serial.print(F("null"));
}

// ---------- JSON 보내기 ----------
void printJsonNumberOrNull(float v, bool valid) {
  if (valid) Serial.print(v, 1); else Serial.print(F("null"));
}

void sendState(uint16_t request) {
  Serial.print(F("{\"v\":"));            Serial.print(PROTOCOL_VERSION);
  Serial.print(F(",\"type\":\"state\",\"profile\":\"" PROFILE_ID "\",\"request\":"));
  Serial.print(request);
  Serial.print(F(",\"ms\":"));           Serial.print(millis());
  Serial.print(F(",\"humidity_pct\":")); printJsonNumberOrNull(humidity, sensorValid);
  Serial.print(F(",\"temperature_c\":"));printJsonNumberOrNull(temperature, sensorValid);
  Serial.print(F(",\"rh_level\":"));     printLevelOrNull(level);
  Serial.print(F(",\"rh_low\":"));       Serial.print(rhLow);
  Serial.print(F(",\"rh_high\":"));      Serial.print(rhHigh);
  Serial.print(F(",\"t_level\":"));      printLevelOrNull(tLevel);
  Serial.print(F(",\"t_low\":"));        Serial.print(tLow);
  Serial.print(F(",\"t_high\":"));       Serial.print(tHigh);
  Serial.print(F(",\"light_on\":"));     Serial.print(lightOn ? F("true") : F("false"));
  Serial.print(F(",\"bright\":"));       Serial.print(bright);
  Serial.print(F(",\"rgb\":["));
  Serial.print(outR); Serial.print(','); Serial.print(outG); Serial.print(','); Serial.print(outB);
  Serial.print(F("],\"dht_fail_count\":")); Serial.print(dhtFailCount);
  Serial.print(F(",\"buzzer_on\":"));    Serial.print(buzzerOn ? F("true") : F("false"));
  Serial.print(F(",\"lcd_ready\":"));    Serial.print(lcdReady ? F("true") : F("false"));
  Serial.println('}');
}

void sendAck(uint16_t request) {
  Serial.print(F("{\"v\":1,\"type\":\"ack\",\"request\":"));
  Serial.print(request);
  Serial.println(F(",\"applied\":true}"));
}

void sendError(uint16_t request, const char* code) {
  Serial.print(F("{\"v\":1,\"type\":\"error\",\"request\":"));
  Serial.print(request);
  Serial.print(F(",\"code\":\""));
  Serial.print(code);
  Serial.println(F("\"}"));
}

// ---------- LCD ----------
// I2C 백팩(PCF8574 계열)은 보통 0x27 또는 0x3F입니다. 0x20–0x3F를 차례로 확인합니다.
uint8_t findLcdAddress() {
  Wire.begin();
  const uint8_t first[] = {0x27, 0x3F};
  for (uint8_t a : first) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) return a; }
  for (uint8_t a = 0x20; a <= 0x3F; a++) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) return a; }
  return 0;  // 못 찾음 → lcd_ready=false
}

void drawLcd() {
  if (!lcdReady) return;
  char line[17];
  if (sensorValid) {
    snprintf(line, sizeof(line), "RH %3d%%  T %3dC ",
             (int)lround(humidity), (int)lround(temperature));
  } else {
    snprintf(line, sizeof(line), "RH  --%%  T  --C ");
  }
  lcd->setCursor(0, 0); lcd->print(line);

  const char* text;
  switch (level) {
    case LEVEL_LOW:  text = "LOW  (DRY)";  break;
    case LEVEL_OK:   text = "OK";          break;
    case LEVEL_HIGH: text = "HIGH (VENT)"; break;
    default:         text = everMeasured ? "CHECK SENSOR" : "MEASURING..."; break;
  }
  // 왼쪽 12칸 = 습도 상태, 오른쪽 4칸 = 신호등을 껐을 때 " OFF", 아니면 온도가 벗어났을 때 " TLO"/" THI"
  const char* right = !lightOn ? " OFF" : tLevel == LEVEL_LOW ? " TLO" : tLevel == LEVEL_HIGH ? " THI" : "    ";
  snprintf(line, sizeof(line), "%-12s%s", text, right);
  lcd->setCursor(0, 1); lcd->print(line);
}

// ---------- 센서 ----------
void readSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    if (dhtFailCount < 255) dhtFailCount++;
    if (dhtFailCount >= DHT_FAIL_LIMIT) sensorValid = false;  // 0으로 바꾸지 않고 null 처리
  } else {
    humidity = h; temperature = t;
    sensorValid = true; everMeasured = true;
    dhtFailCount = 0;
  }
  level = classifyHumidity(humidity, sensorValid, level, rhLow, rhHigh, RH_HYSTERESIS);
  // 온도도 같은 규칙(기준 + 여유 구간)으로 나눕니다.
  tLevel = classifyHumidity(temperature, sensorValid, tLevel, tLow, tHigh, T_HYSTERESIS);
  applyOutputs();
  checkAlarm();
}

// ---------- 명령 처리 ----------
void handleLine(char* line) {
  char* tok[6];
  uint8_t n = splitTokens(line, tok, 5);
  if (n == 0) return;                       // 빈 줄은 무시
  long reqL;
  if (!parseStrictLong(tok[0], reqL) || reqL < 1 || reqL > 65535) {
    sendError(0, "bad_request"); return;
  }
  uint16_t req = (uint16_t)reqL;
  if (n > 5 || n < 2) { sendError(req, "bad_args"); return; }
  toUpperAscii(tok[1]);
  const char* cmd = tok[1];
  uint8_t argc = n - 2;
  long a = 0, b = 0;

  if (strcmp(cmd, "STATUS") == 0) {
    if (argc != 0) { sendError(req, "bad_args"); return; }
    sendState(req);
  } else if (strcmp(cmd, "PING") == 0) {
    if (argc != 0) { sendError(req, "bad_args"); return; }
    sendAck(req);
  } else if (strcmp(cmd, "STOP") == 0) {
    if (argc != 0) { sendError(req, "bad_args"); return; }
    lightOn = false; applyOutputs(); drawLcd(); sendAck(req);
  } else if (strcmp(cmd, "LIGHT") == 0) {
    if (argc != 1 || !parseStrictLong(tok[2], a)) { sendError(req, "bad_args"); return; }
    if (a != 0 && a != 1) { sendError(req, "out_of_range"); return; }
    lightOn = (a == 1); applyOutputs(); drawLcd(); sendAck(req);
  } else if (strcmp(cmd, "BRIGHT") == 0) {
    if (argc != 1 || !parseStrictLong(tok[2], a)) { sendError(req, "bad_args"); return; }
    if (a < 0 || a > 255) { sendError(req, "out_of_range"); return; }
    bright = (uint8_t)a; applyOutputs(); sendAck(req);
  } else if (strcmp(cmd, "BUZZ") == 0) {
    if (argc != 1 || !parseStrictLong(tok[2], a)) { sendError(req, "bad_args"); return; }
    if (a != 0 && a != 1) { sendError(req, "out_of_range"); return; }
    buzzerOn = (a == 1);
    if (!buzzerOn) { beepLeft = 0; nextBeeps = 0; buzzDuty = 0; }  // 울리는 중이면 바로 멈춤
    sendAck(req);
  } else if (strcmp(cmd, "SET") == 0) {
    if (argc != 3) { sendError(req, "bad_args"); return; }
    toUpperAscii(tok[2]);
    const bool isT = strcmp(tok[2], "T") == 0;
    if (!isT && strcmp(tok[2], "RH") != 0) { sendError(req, "unknown_setting"); return; }
    if (!parseStrictLong(tok[3], a) || !parseStrictLong(tok[4], b)) { sendError(req, "bad_args"); return; }
    const char* err = isT ? checkRhThresholds(a, b, T_SET_MIN, T_SET_MAX, T_MIN_GAP)
                          : checkRhThresholds(a, b, RH_SET_MIN, RH_SET_MAX, RH_MIN_GAP);
    if (err) { sendError(req, err); return; }
    // 기준이 바뀌면 여유 구간 없이 새로 나눕니다.
    if (isT) {
      tLow = (uint8_t)a; tHigh = (uint8_t)b;
      tLevel = classifyHumidity(temperature, sensorValid, LEVEL_NONE, tLow, tHigh, T_HYSTERESIS);
    } else {
      rhLow = (uint8_t)a; rhHigh = (uint8_t)b;
      level = classifyHumidity(humidity, sensorValid, LEVEL_NONE, rhLow, rhHigh, RH_HYSTERESIS);
    }
    applyOutputs(); drawLcd(); sendAck(req);
  } else {
    sendError(req, "unknown_command");
  }
}

void pollSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (cmdOverflow) sendError(0, "line_too_long");
      else { cmdBuf[cmdLen] = '\0'; handleLine(cmdBuf); }
      cmdLen = 0; cmdOverflow = false;
    } else if (cmdLen < CMD_BUFFER_SIZE - 1) {
      cmdBuf[cmdLen++] = c;
    } else {
      cmdOverflow = true;  // 줄이 끝날 때까지 버림
    }
  }
}

// ---------- 시작과 반복 ----------
void setup() {
  // 1) 출력부터 끈 상태로 만듭니다.
  pinMode(PIN_RGB_R, OUTPUT); pinMode(PIN_RGB_G, OUTPUT); pinMode(PIN_RGB_B, OUTPUT);
  buzzerBegin();
  applyOutputs();  // level = NONE → 파랑 깜빡임 (측정 중)

  Serial.begin(SERIAL_BAUD);
  dht.begin();

  // 2) LCD: 실패해도 나머지는 계속 동작하고 lcd_ready=false로 알립니다.
  uint8_t addr = findLcdAddress();
  if (addr) {
    lcd = new LiquidCrystal_I2C(addr, 16, 2);
    lcd->init();
    lcd->backlight();
    lcd->clear();
    lcdReady = true;
    drawLcd();
  }

  // DHT11은 전원 후 약 1초 뒤부터 읽을 수 있으므로, 첫 측정은 2초가 아니라 1초 뒤에 합니다.
  lastDhtMs = millis() - (DHT_INTERVAL_MS - 1000UL);
}

void loop() {
  unsigned long now = millis();   // 빼기로 비교해 약 49일 뒤 넘침에도 안전합니다
  pollSerial();

  if (now - lastDhtMs >= DHT_INTERVAL_MS) { lastDhtMs = now; readSensor(); }
  if (now - lastLcdMs >= LCD_INTERVAL_MS) { lastLcdMs = now; drawLcd(); }
  if (now - lastStateMs >= STATE_INTERVAL_MS) { lastStateMs = now; sendState(0); }
  if (now - lastBlinkMs >= BLINK_MS) {
    lastBlinkMs = now;
    blinkOn = !blinkOn;
    if (level == LEVEL_NONE) applyOutputs();
  }
  updateBuzzer(now);
}
