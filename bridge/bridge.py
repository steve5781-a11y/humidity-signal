"""P07 습도 신호등 · PC 연결 프로그램(브리지)

보드의 USB 시리얼(COM 포트)을 이 프로그램 하나가 맡고,
대시보드는 이 프로그램의 WebSocket(ws://127.0.0.1:8765)으로 접속합니다.
여러 브라우저 탭·기기가 같은 보드를 볼 수 있지만, 이 PC 밖에서는 접속할 수 없습니다.

사용법 (Windows PowerShell 예)
  py -3.12 -m venv .venv
  .\\.venv\\Scripts\\python.exe -m pip install -r bridge/requirements.txt
  .\\.venv\\Scripts\\python.exe bridge/bridge.py --list          # 포트 목록 보기
  .\\.venv\\Scripts\\python.exe bridge/bridge.py --port COM5     # 실행

주의: Arduino 시리얼 모니터와 이 프로그램은 같은 포트를 동시에 열 수 없습니다.
"""
from __future__ import annotations

import argparse
import asyncio
import json
import re
import threading
import time

import serial
import serial.tools.list_ports
import websockets

# 보드 명령 형식: "요청번호 명령 인수…" (protocol.h와 같은 규칙, 47자 이하)
COMMAND_RE = re.compile(r"^\d{1,5} [A-Za-z]+( [A-Za-z0-9-]+){0,3}$")
MAX_COMMAND_LEN = 47
RESET_WAIT_S = 2.0      # 포트를 열면 UNO가 재시작하므로 기다립니다
RETRY_WAIT_S = 3.0


class Bridge:
    def __init__(self, port: str, baud: int) -> None:
        self.port_name = port
        self.baud = baud
        self.ser: serial.Serial | None = None
        self.clients: set = set()
        self.loop: asyncio.AbstractEventLoop | None = None
        self.status = "starting"
        self.write_lock = threading.Lock()

    # ---------- 브라우저로 보내기 ----------
    def publish(self, text: str) -> None:
        """시리얼 스레드에서 호출해도 안전하게 모든 접속자에게 보냅니다."""
        if self.loop is not None:
            self.loop.call_soon_threadsafe(asyncio.ensure_future, self._broadcast(text))

    async def _broadcast(self, text: str) -> None:
        dead = []
        for ws in list(self.clients):
            try:
                await ws.send(text)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.clients.discard(ws)

    def bridge_event(self, status: str, detail: str = "") -> str:
        self.status = status
        return json.dumps({"v": 1, "type": "bridge", "status": status,
                           "port": self.port_name, "detail": detail}, ensure_ascii=False)

    # ---------- 시리얼 읽기 스레드 ----------
    def serial_worker(self) -> None:
        while True:
            try:
                self.publish(self.bridge_event("serial_opening"))
                ser = serial.Serial(self.port_name, self.baud, timeout=0.5)
                time.sleep(RESET_WAIT_S)
                ser.reset_input_buffer()
                self.ser = ser
                self.publish(self.bridge_event("serial_open"))
                print(f"[bridge] {self.port_name} 열림 ({self.baud})")
                buf = b""
                while True:
                    chunk = ser.read(256)
                    if not chunk:
                        continue
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        text = line.decode("utf-8", errors="replace").strip()
                        if text:
                            self.publish(text)
                    if len(buf) > 2048:          # 줄바꿈 없는 잡음은 버립니다
                        buf = b""
            except (serial.SerialException, OSError) as exc:
                self.ser = None
                msg = str(exc)
                print(f"[bridge] 시리얼 오류: {msg} — {RETRY_WAIT_S:.0f}초 뒤 다시 시도")
                self.publish(self.bridge_event("serial_error", msg))
                time.sleep(RETRY_WAIT_S)

    # ---------- 브라우저에서 받기 ----------
    async def handle_client(self, ws) -> None:
        self.clients.add(ws)
        await ws.send(json.dumps({"v": 1, "type": "bridge", "status": self.status,
                                  "port": self.port_name, "detail": ""}))
        try:
            async for message in ws:
                line = str(message).strip()
                if len(line) > MAX_COMMAND_LEN or not COMMAND_RE.match(line):
                    await ws.send(json.dumps({"v": 1, "type": "bridge", "status": "rejected",
                                              "port": self.port_name, "detail": line[:60]}))
                    continue
                ser = self.ser
                if ser is None:
                    await ws.send(self.bridge_event("serial_not_open"))
                    continue
                with self.write_lock:
                    ser.write((line + "\n").encode("ascii"))
                # 출력 명령은 자동 재전송하지 않습니다. 결과는 보드의 ack/error로 확인합니다.
        finally:
            self.clients.discard(ws)


async def main_async(args: argparse.Namespace) -> None:
    bridge = Bridge(args.port, args.baud)
    bridge.loop = asyncio.get_running_loop()
    threading.Thread(target=bridge.serial_worker, daemon=True).start()
    async with websockets.serve(bridge.handle_client, "127.0.0.1", args.ws_port):
        print(f"[bridge] 대시보드 접속 주소: ws://127.0.0.1:{args.ws_port}")
        await asyncio.Future()


def main() -> None:
    parser = argparse.ArgumentParser(description="P07 시리얼 ↔ WebSocket 브리지")
    parser.add_argument("--port", help="보드 포트 (예: COM5, /dev/ttyACM0)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--ws-port", type=int, default=8765)
    parser.add_argument("--list", action="store_true", help="연결된 포트 목록만 보기")
    args = parser.parse_args()
    if args.list or not args.port:
        ports = list(serial.tools.list_ports.comports())
        if not ports:
            print("연결된 포트가 없습니다. 데이터용 USB 케이블과 드라이버를 확인하세요.")
        for p in ports:
            print(f"{p.device}\t{p.description}")
        if not args.port:
            return
    try:
        asyncio.run(main_async(args))
    except KeyboardInterrupt:
        print("\n[bridge] 종료")


if __name__ == "__main__":
    main()
