# 딸기 하우스 습도 신호등 (P07)

딸기 하우스의 습도가 관리 범위(60–85 %)를 벗어났는지 **보드의 RGB LED·부저·LCD**와 **웹 대시보드**로 알려 주는 교육용 시제품입니다.

- 사이트: https://steve5781-a11y.github.io/humidity-signal/
- 대시보드: https://steve5781-a11y.github.io/humidity-signal/dashboard.html (Chrome·Edge)

## 1. 작품명과 사용자

| 항목 | 내용 |
|---|---|
| 작품명 | 딸기 하우스 습도 신호등 |
| 사용자 (가정) | 딸기 하우스 관리자·실습생. 작업 중이라 화면을 오래 볼 수 없음 |
| 과업 | 지금 습도가 낮음·적정·높음 중 무엇인지 바로 판단하고 조치(관수·환기), 기준값 조정 |
| 사용 맥락 | 하우스(교실) 안, 보드 옆 PC 한 대, USB 전원 |

## 2. 부품과 핀

| 부품 | UNO 핀 | 역할 |
|---|---|---|
| DHT11 온습도 모듈 | D4, 5V, GND | 입력: 습도·온도 |
| RGB LED 모듈 (공통 −) | D5 R · D6 G · D9 B (각 1kΩ) | 출력: 상태 색 |
| 2핀 능동형 부저 | D8 (1kΩ 경유), GND | 출력: 상태가 바뀔 때 경보 |
| LCD1602 I2C | A4 SDA · A5 SCL, 5V, GND | 출력: 숫자·상태 글자 |

자세한 배선은 [회로도](wiring.html)를 따릅니다(단계별 그림 + 핀·구멍 표).

## 3. 실행 순서

1. [회로도](wiring.html)대로 배선합니다. USB를 뽑고 작업합니다.
2. Arduino IDE 라이브러리 매니저에서 설치: DHT sensor library 1.4.7, Adafruit Unified Sensor 1.1.15, LiquidCrystal I2C 1.1.2.
3. `firmware/P07/P07.ino`를 열고(같은 폴더의 `pins.h`·`protocol.h`·`logic.h` 함께) 보드 Arduino Uno로 업로드합니다.
4. 시리얼 모니터는 닫고, Chrome·Edge로 대시보드를 엽니다 → 「연결하기」 → 보드 포트 선택.
5. 보드 없이 화면만 볼 때는 연결 방식에서 「시뮬레이션」을 고릅니다.

## 4. 핵심 버튼 (대시보드)

| 버튼 | 하는 일 |
|---|---|
| 연결하기 / 연결 끊기 | USB(Web Serial)·브리지·시뮬레이션 연결 |
| 기준 적용 / 딸기 초기값으로 | 낮음·높음 기준(%)을 보드에 보냄 / 60·85로 되돌림 |
| 밝기 적용 | RGB 밝기 0–255 |
| 신호등 켜짐·꺼짐 | 보드 RGB 끄기 (측정은 계속) |
| 보드 부저 켜짐·꺼짐 | 보드 부저 경보 끄기 |
| 화면 알림음 | 이 PC에서만 나는 알림음 (기본 꺼짐) |
| 화면 갱신 멈춤 | 화면만 멈춤, 기록은 계속 |

## 5. 예상 결과

| 상태 | 습도 | LED | 부저 | LCD 2번째 줄 |
|---|---|---|---|---|
| 낮음 | 60 % 미만 | 파랑 | 2번 (들어갈 때만) | `LOW  (DRY)` |
| 적정 | 60–85 % | 초록 | 울리지 않음 | `OK` |
| 높음 | 85 % 이상 | 빨강 | 3번 (들어갈 때만) | `HIGH (VENT)` |
| 값 없음 | 센서 실패·측정 전 | 파랑 깜빡임 | 울리지 않음 | `CHECK SENSOR` / `MEASURING...` |

경계에서 깜빡이지 않도록 2 % 여유가 있습니다(85 %에서 빨강 → 83 % 이하에서 초록, 60 %에서 파랑 → 62 % 이상에서 초록).

## 6. 확인한 범위와 못 한 범위

| 항목 | 결과 |
|---|---|
| AVR 컴파일·업로드 | arduino-cli로 Arduino Uno에 컴파일·업로드 (2026-10-29) |
| 실물 동작 | 센서 수신(습도 52–58 %, 실패 0회), 상태별 LED 색·부저 횟수·LCD 글자, 잘못된 기준값 거절을 실물로 확인 (2026-10-29) |
| 부저 1kΩ 저항 | 회로도에 반영. 저항을 넣은 뒤 소리 크기는 **다시 확인 필요** |
| 웹 접근성 | [설계 근거](design.html)의 WCAG 2.2 표 참고. 화면 낭독기 실제 사용은 **못 함** |
| 실제 하우스 환경 | **못 함** (고습·장시간 사용) |

## 7. AI로 만들거나 바꾼 파일

Claude(AI)와 함께 만들었습니다. 모든 변경은 실물 보드로 다시 확인했습니다.

| 파일 | 내용 |
|---|---|
| `firmware/P07/*` | 건조 = 파랑, 측정 전·오류 = 파랑 깜빡임, 부저 경보(소리 크기 조절, `BUZZ` 명령), 기준 60/85 |
| `dashboard.html` | 상태 색 문구, 보드 부저 버튼·상태, 공통 메뉴 |
| `wiring.html` | 부저 + 1kΩ 배선 추가, 색 규칙 갱신 |
| `index.html`, `options.html`, `design.html`, `assets/*` | 새로 만듦 |

## 8. 다음 기능

- 바꾼 기준값을 보드 EEPROM에 저장 (지금은 다시 켜면 60/85)
- 오래 벗어나 있으면 일정 간격으로 다시 알리기
- 기록 CSV 내려받기

## 9. 통신 규약

PC → 보드: `요청번호 명령 인수` + 줄바꿈 (115200). 보드 → PC: JSON 한 줄, 1초마다 상태.

| 명령 | 동작 | 오류 코드 |
|---|---|---|
| `1 STATUS` | 상태 요청 | |
| `2 PING` | 연결 확인 | |
| `3 STOP` | 신호등 끄기 (측정은 계속) | |
| `10 SET RH 60 85` | 낮음 / 높음 기준 (%) | `out_of_range`, `low_not_below_high`, `gap_too_small` |
| `11 BRIGHT 80` | RGB 밝기 0–255 | `out_of_range` |
| `12 LIGHT 1` | 신호등 켜기 (0 = 끄기) | `out_of_range` |
| `13 BUZZ 1` | 부저 경보 켜기 (0 = 끄기) | `out_of_range` |

상태 예: `{"v":1,"type":"state","profile":"P07","request":0,"ms":10200,"humidity_pct":54.0,"temperature_c":26.6,"rh_level":"low","rh_low":60,"rh_high":85,"light_on":true,"bright":80,"rgb":[80,40,0],"dht_fail_count":0,"buzzer_on":true,"lcd_ready":true}`

PC 연결 프로그램(브리지, 선택): `bridge/bridge.py` — 다른 브라우저나 여러 탭에서 볼 때 씁니다.

```powershell
py -3.12 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r bridge/requirements.txt
.\.venv\Scripts\python.exe bridge/bridge.py --port COM3
```

## 10. 한계

교육용 시제품입니다. DHT11 오차(습도 ±5 %, 온도 ±2 °C), 빵판 배선, USB 전원 때문에 실제 하우스 관리에는 쓰지 않습니다.
