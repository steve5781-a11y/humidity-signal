// protocol.h — PC(대시보드) ↔ 보드 통신 약속
//
// PC → 보드 : 한 줄 = "요청번호 명령 인수…" + 줄바꿈(LF)
//   예) 1 STATUS
//       2 PING
//       3 STOP           신호등 끄기(측정은 계속)
//       10 SET RH 60 85  낮음 기준 / 높음 기준 (%)
//       14 SET T 10 30   온도 낮음 기준 / 높음 기준 (°C)
//       11 BRIGHT 80     RGB 밝기 0–255
//       12 LIGHT 1       신호등 켜기 (0 = 끄기)
//       13 BUZZ 1        부저 경보 켜기 (0 = 끄기)
//
// 보드 → PC : JSON 한 줄
//   {"v":1,"type":"state",...}   1초마다(request=0) 또는 STATUS 응답
//   {"v":1,"type":"ack","request":10,"applied":true}
//   {"v":1,"type":"error","request":10,"code":"out_of_range"}
#pragma once
#include <stdint.h>

#define PROFILE_ID        "P07"
#define PROTOCOL_VERSION  1
const unsigned long SERIAL_BAUD = 115200UL;

// 시간 간격 (밀리초)
const unsigned long DHT_INTERVAL_MS   = 2000UL;  // DHT11은 2초보다 자주 읽지 않습니다
const unsigned long STATE_INTERVAL_MS = 1000UL;  // 상태 자동 전송 주기
const unsigned long LCD_INTERVAL_MS   = 1000UL;  // LCD 다시 그리기 주기
const unsigned long BLINK_MS          = 500UL;   // 센서 확인 상태의 파랑 깜빡임 (0.5초 켜짐·0.5초 꺼짐)

// DHT11 연속 실패 허용 횟수: 이 횟수만큼 연속 실패하면 값을 null로 바꿉니다.
// (2회 × 2초 = 약 4초. 한 번의 일시적 실패로 화면이 깜빡이지 않게 합니다)
const uint8_t DHT_FAIL_LIMIT = 2;

// 습도 기준 기본값 — 딸기 초기값 60–85 % (대시보드에서 바꿀 수 있음)
const uint8_t DEFAULT_RH_LOW  = 60;  // 이 값 미만 = 낮음(파랑)
const uint8_t DEFAULT_RH_HIGH = 85;  // 이 값 이상 = 높음(빨강)
const uint8_t RH_HYSTERESIS   = 2;   // 되돌아갈 때 필요한 여유 (%)
const uint8_t DEFAULT_BRIGHT  = 80;  // RGB 밝기 0–255

// 온도 기준 기본값 — 딸기 초기값 10–30 °C (10 °C 미만 저온, 30 °C 이상 고온 장해)
// 온도는 LED에 표시하지 않고 부저(길게)와 대시보드로만 알립니다.
const uint8_t DEFAULT_T_LOW   = 10;
const uint8_t DEFAULT_T_HIGH  = 30;
const uint8_t T_HYSTERESIS    = 1;   // °C
const uint8_t T_SET_MIN       = 0;   // DHT11 측정 범위 0–50 °C
const uint8_t T_SET_MAX       = 50;
const uint8_t T_MIN_GAP       = 2 * T_HYSTERESIS;

// 기준값 허용 범위
const uint8_t RH_SET_MIN = 1;
const uint8_t RH_SET_MAX = 99;
// 낮음 기준과 높음 기준 사이 최소 간격: 양쪽 여유(2% + 2%)가 겹치지 않도록
const uint8_t RH_MIN_GAP = 2 * RH_HYSTERESIS;

// 부저 경보: 낮음·높음 상태로 들어갈 때만 울립니다. 능동형 부저는 음 높이를 못 바꾸므로
// 횟수(낮음 2·높음 3)와 길이(습도 = 짧게, 온도 = 길게)로 구분합니다.
const uint8_t       BUZZ_BEEPS_HIGH = 3;      // 높음 = 3번
const uint8_t       BUZZ_BEEPS_LOW  = 2;      // 낮음 = 2번
const unsigned long BUZZ_ON_MS      = 60UL;   // 습도: 한 번 울리는 길이 (짧을수록 작게 들림)
const unsigned long BUZZ_PERIOD_MS  = 300UL;  // 습도: 울림 시작 간격 (on + 쉼)
const unsigned long BUZZ_LONG_ON_MS     = 350UL;  // 온도: 한 번 울리는 길이
const unsigned long BUZZ_LONG_PERIOD_MS = 700UL;  // 온도: 울림 시작 간격
const unsigned long BUZZ_GAP_MS     = 1000UL; // 습도·온도 경보가 겹칠 때 두 경보 사이 쉼
// 소리 크기 1–10: 울리는 동안 10칸 중 몇 칸만 부저를 켤지. 10 = 최대, 1 = 가장 작음
const uint8_t       BUZZ_VOLUME     = 2;

// 받은 한 줄의 최대 길이 (UNO SRAM 2KB를 아끼기 위해 작게)
const uint8_t CMD_BUFFER_SIZE = 48;
