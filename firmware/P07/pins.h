// pins.h — P07 딸기 하우스 습도 신호등의 핀 배정
// 배선도(P07 페이지)의 표와 이 파일이 반드시 같아야 합니다.
// 핀을 바꾸려면 배선도부터 먼저 고친 뒤 이 파일을 고칩니다.
#pragma once
#include <stdint.h>

// RGB LED 모듈 — PWM 핀 (강의 P02와 같은 D5·D6·D9)
const uint8_t PIN_RGB_R = 5;   // 1kΩ 경유 → RGB R
const uint8_t PIN_RGB_G = 6;   // 1kΩ 경유 → RGB G
const uint8_t PIN_RGB_B = 9;   // 1kΩ 경유 → RGB B

// RGB 모듈 종류
//   공통 −(음극)형: 핀에 "−" 또는 "GND"가 적혀 있음 → false (기본, 배선도와 같음)
//   공통 +(양극)형: 핀에 "+" 또는 "VCC"가 적혀 있음 → true
//   true로 바꾸면 배선도 J12(공통 핀)를 GND 줄이 아니라 5V 줄에 연결해야 합니다.
const bool RGB_COMMON_ANODE = false;

// DHT11 온습도 모듈 데이터선
const uint8_t PIN_DHT = 4;

// LCD1602 I2C 백팩: UNO 하드웨어 I2C 고정 핀 (SDA = A4, SCL = A5)
// Wire 라이브러리가 자동으로 사용하므로 여기서는 기록만 합니다.

// 부저 모듈 신호선 (S 또는 I/O). 소리 크기는 protocol.h의 BUZZ_VOLUME으로 조절합니다.
// 부저 구동에 Timer2를 쓰므로 D3·D11의 PWM과 tone()은 함께 쓸 수 없습니다(지금은 비어 있음).
const uint8_t PIN_BUZZER = 8;

// 부저 모듈 종류
//   저신호 동작형(트랜지스터가 달린 3핀 모듈): S가 LOW일 때 울림 → true
//   고신호 동작형·2핀 부저: S가 HIGH일 때 울림 → false
//   쉬는 동안 계속 울리면 이 값을 반대로 바꿉니다.
// 이 회로는 2핀 부저(+ → D8, − → GND)라서 false입니다.
const bool BUZZER_ACTIVE_LOW = false;

// 비워 두는 핀: D0·D1(USB 시리얼), D2, D3, D7, D10–D13, A0–A3
