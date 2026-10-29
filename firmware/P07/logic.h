// logic.h — 하드웨어와 상관없는 판단·해석 함수
// 보드 없이 PC에서도 시험할 수 있도록 Arduino 기능을 쓰지 않습니다.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// 습도 상태
enum Level : uint8_t {
  LEVEL_NONE = 0,  // 값 없음 (첫 측정 전 또는 센서 오류)
  LEVEL_LOW  = 1,  // 낮음 → 파랑
  LEVEL_OK   = 2,  // 적정 → 초록
  LEVEL_HIGH = 3   // 높음 → 빨강
};

// 히스테리시스 없이 바로 나누기 (첫 측정이나 기준 변경 직후)
inline Level classifyPlain(float rh, uint8_t low, uint8_t high) {
  if (rh < low)   return LEVEL_LOW;
  if (rh >= high) return LEVEL_HIGH;
  return LEVEL_OK;
}

// 이전 상태를 기억해 경계에서 색이 계속 바뀌지 않게 합니다.
//   적정 → 높음 : rh >= high          높음 → 적정 : rh <= high - hyst
//   적정 → 낮음 : rh <  low           낮음 → 적정 : rh >= low + hyst
inline Level classifyHumidity(float rh, bool valid, Level prev,
                              uint8_t low, uint8_t high, uint8_t hyst) {
  if (!valid) return LEVEL_NONE;
  switch (prev) {
    case LEVEL_HIGH:
      if (rh > (float)high - hyst) return LEVEL_HIGH;  // 아직 여유 구간 안
      return classifyPlain(rh, low, high);
    case LEVEL_LOW:
      if (rh < (float)low + hyst) return LEVEL_LOW;    // 아직 여유 구간 안
      return classifyPlain(rh, low, high);
    case LEVEL_OK:
      return classifyPlain(rh, low, high);
    default:
      return classifyPlain(rh, low, high);
  }
}

inline const char* levelName(Level lv) {  // JSON용
  switch (lv) {
    case LEVEL_LOW:  return "low";
    case LEVEL_OK:   return "ok";
    case LEVEL_HIGH: return "high";
    default:         return nullptr;      // null로 보냅니다
  }
}

// 문자열 전체가 정수일 때만 true ("12a", "", "+" 는 거절)
inline bool parseStrictLong(const char* s, long& out) {
  if (s == nullptr || *s == '\0') return false;
  char* end = nullptr;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0') return false;
  out = v;
  return true;
}

// 한 줄을 공백 기준으로 나눕니다. 줄 자체를 고쳐 쓰며, 나눈 개수를 돌려줍니다.
// maxTok보다 많으면 maxTok + 1을 돌려줘 "인수가 너무 많음"을 알 수 있게 합니다.
inline uint8_t splitTokens(char* line, char* tok[], uint8_t maxTok) {
  uint8_t n = 0;
  char* p = line;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '\r') *p++ = '\0';
    if (!*p) break;
    if (n == maxTok) return maxTok + 1;
    tok[n++] = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r') p++;
  }
  return n;
}

// 영문 명령을 대문자로 바꿔 대소문자를 가리지 않게 합니다.
inline void toUpperAscii(char* s) {
  for (; *s; ++s) if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 'a' + 'A');
}

// 습도 기준값 검사. 문제가 없으면 nullptr, 있으면 오류 코드를 돌려줍니다.
inline const char* checkRhThresholds(long low, long high,
                                     uint8_t minV, uint8_t maxV, uint8_t minGap) {
  if (low < minV || low > maxV || high < minV || high > maxV) return "out_of_range";
  if (low >= high) return "low_not_below_high";
  if (high - low < minGap) return "gap_too_small";
  return nullptr;
}
