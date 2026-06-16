import argparse
import math
import socket
import struct
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import cv2
import mediapipe as mp
from websockets.sync.server import ServerConnection, serve


MODEL_PATH = Path("models/hand_landmarker.task")
PACKET_IDENTIFIER = 0x4D50
CMD_GESTURE = 0x0001
PAYLOAD_SIZE = 4
WINDOW_NAME = "MediaPipe Gesture Server"
GESTURE_NAMES = {
    0: "Unknown",
    1: "Fist",
    2: "Scissors",
    3: "Paper",
}


@dataclass
class FingerState:
    extended: bool
    metric_a: float
    metric_b: float


def distance(a: Any, b: Any) -> float:
    return ((a.x - b.x) ** 2 + (a.y - b.y) ** 2) ** 0.5


def angle_between(a: Any, b: Any, c: Any) -> float:
    abx = a.x - b.x
    aby = a.y - b.y
    cbx = c.x - b.x
    cby = c.y - b.y
    dot = abx * cbx + aby * cby
    mag1 = (abx**2 + aby**2) ** 0.5
    mag2 = (cbx**2 + cby**2) ** 0.5
    if not mag1 or not mag2:
        return 0.0
    cosine = max(-1.0, min(1.0, dot / (mag1 * mag2)))
    return math.degrees(math.acos(cosine))


def palm_size(landmarks: list[Any]) -> float:
    return max(distance(landmarks[0], landmarks[5]), distance(landmarks[0], landmarks[17]), 0.001)


def finger_state(
    landmarks: list[Any],
    mcp_index: int,
    pip_index: int,
    dip_index: int,
    tip_index: int,
) -> FingerState:
    pip_angle = angle_between(landmarks[mcp_index], landmarks[pip_index], landmarks[dip_index])
    dip_angle = angle_between(landmarks[pip_index], landmarks[dip_index], landmarks[tip_index])
    tip_reach = distance(landmarks[tip_index], landmarks[0])
    pip_reach = distance(landmarks[pip_index], landmarks[0])
    return FingerState(
        extended=pip_angle > 160.0 and dip_angle > 150.0 and tip_reach > pip_reach * 1.12,
        metric_a=pip_angle,
        metric_b=dip_angle,
    )


def thumb_state(landmarks: list[Any], hand_palm_size: float) -> FingerState:
    mcp_angle = angle_between(landmarks[1], landmarks[2], landmarks[3])
    ip_angle = angle_between(landmarks[2], landmarks[3], landmarks[4])
    spread = distance(landmarks[4], landmarks[5]) / hand_palm_size
    reach = distance(landmarks[4], landmarks[0]) / hand_palm_size
    return FingerState(
        extended=ip_angle > 145.0 and mcp_angle > 135.0 and spread > 0.55 and reach > 1.1,
        metric_a=spread,
        metric_b=reach,
    )


def classify_gesture(landmarks: list[Any]) -> int:
    hand_palm_size = palm_size(landmarks)
    thumb = thumb_state(landmarks, hand_palm_size)
    index = finger_state(landmarks, 5, 6, 7, 8)
    middle = finger_state(landmarks, 9, 10, 11, 12)
    ring = finger_state(landmarks, 13, 14, 15, 16)
    pinky = finger_state(landmarks, 17, 18, 19, 20)
    long_count = sum(1 for finger in (index, middle, ring, pinky) if finger.extended)

    if long_count == 0 and thumb.metric_b < 1.25:
        return 1
    if index.extended and middle.extended and not ring.extended and not pinky.extended:
        return 2
    if long_count == 4 and (thumb.extended or thumb.metric_a > 0.45):
        return 3
    return 0


def build_packet(gesture_code: int) -> bytes:
    return struct.pack("!HHII", PACKET_IDENTIFIER, CMD_GESTURE, PAYLOAD_SIZE, gesture_code)


def draw_preview(frame: Any, gesture_code: int, transport: str, host: str, port: int) -> None:
    cv2.putText(
        frame,
        f"Gesture: {GESTURE_NAMES.get(gesture_code, 'Unknown')}",
        (20, 40),
        cv2.FONT_HERSHEY_SIMPLEX,
        1.0,
        (0, 255, 255),
        2,
        cv2.LINE_AA,
    )
    cv2.putText(
        frame,
        f"Transport: {transport.upper()} {host}:{port}",
        (20, 80),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.75,
        (0, 200, 255),
        2,
        cv2.LINE_AA,
    )
    cv2.putText(
        frame,
        "Press Q or ESC to quit",
        (20, frame.shape[0] - 20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (255, 255, 255),
        2,
        cv2.LINE_AA,
    )


class UdpBroadcaster:
    def __init__(self, host: str, port: int) -> None:
        self.target = (host, port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send(self, packet: bytes) -> None:
        self.sock.sendto(packet, self.target)

    def close(self) -> None:
        self.sock.close()


class TcpServerBroadcaster:
    def __init__(self, host: str, port: int) -> None:
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((host, port))
        self.server.listen(1)
        self.server.settimeout(0.01)
        self.client: socket.socket | None = None
        self.client_addr: tuple[str, int] | None = None

    def _accept_if_needed(self) -> None:
        if self.client is not None:
            return
        try:
            client, client_addr = self.server.accept()
            client.setblocking(False)
            self.client = client
            self.client_addr = client_addr
            print(f"TCP client connected: {client_addr}")
        except TimeoutError:
            return
        except OSError:
            return

    def send(self, packet: bytes) -> None:
        self._accept_if_needed()
        if self.client is None:
            return
        try:
            self.client.sendall(packet)
        except OSError:
            print(f"TCP client disconnected: {self.client_addr}")
            self.client.close()
            self.client = None
            self.client_addr = None

    def close(self) -> None:
        if self.client is not None:
            self.client.close()
            self.client = None
        self.server.close()


class WebSocketBroadcaster:
    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self.lock = threading.Lock()
        self.connections: set[ServerConnection] = set()
        self.stop_event = threading.Event()
        self.server = serve(self._handle_client, host, port)
        self.thread = threading.Thread(target=self._run_server, daemon=True)
        self.thread.start()

    def _run_server(self) -> None:
        with self.server:
            self.stop_event.wait()

    def _handle_client(self, websocket: ServerConnection) -> None:
        with self.lock:
            self.connections.add(websocket)
        print("WebSocket client connected")
        try:
            for _ in websocket:
                pass
        finally:
            with self.lock:
                self.connections.discard(websocket)
            print("WebSocket client disconnected")

    def send(self, packet: bytes) -> None:
        with self.lock:
            connections = list(self.connections)
        for connection in connections:
            try:
                connection.send(packet)
            except Exception:
                with self.lock:
                    self.connections.discard(connection)

    def close(self) -> None:
        self.stop_event.set()
        with self.lock:
            for connection in list(self.connections):
                try:
                    connection.close()
                except Exception:
                    pass
            self.connections.clear()
        self.thread.join(timeout=1.0)


def create_broadcaster(transport: str, host: str, port: int):
    if transport == "udp":
        return UdpBroadcaster(host, port)
    if transport == "tcp":
        return TcpServerBroadcaster(host, port)
    if transport == "websocket":
        return WebSocketBroadcaster(host, port)
    raise ValueError(f"Unsupported transport: {transport}")


def run_server(transport: str, host: str, port: int, show_preview: bool) -> None:
    if not MODEL_PATH.exists():
        raise FileNotFoundError(f"Model file not found: {MODEL_PATH}")

    broadcaster = create_broadcaster(transport, host, port)
    base_options = mp.tasks.BaseOptions(model_asset_path=str(MODEL_PATH))
    options = mp.tasks.vision.HandLandmarkerOptions(
        base_options=base_options,
        running_mode=mp.tasks.vision.RunningMode.VIDEO,
        num_hands=1,
        min_hand_detection_confidence=0.6,
        min_hand_presence_confidence=0.6,
        min_tracking_confidence=0.6,
    )
    capture = cv2.VideoCapture(0)
    if not capture.isOpened():
        broadcaster.close()
        raise RuntimeError("Could not open the webcam.")

    print(f"MediaPipe server running: {transport.upper()} {host}:{port}")

    try:
        with mp.tasks.vision.HandLandmarker.create_from_options(options) as landmarker:
            while True:
                success, frame = capture.read()
                if not success:
                    break

                frame = cv2.flip(frame, 1)
                rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)
                timestamp_ms = int(time.time() * 1000)
                result = landmarker.detect_for_video(mp_image, timestamp_ms)

                gesture_code = 0
                if result.hand_landmarks:
                    gesture_code = classify_gesture(result.hand_landmarks[0])

                broadcaster.send(build_packet(gesture_code))

                if show_preview:
                    draw_preview(frame, gesture_code, transport, host, port)
                    cv2.imshow(WINDOW_NAME, frame)
                    if cv2.waitKey(1) & 0xFF in (27, ord("q")):
                        break
    finally:
        capture.release()
        broadcaster.close()
        cv2.destroyAllWindows()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--transport", choices=("udp", "tcp", "websocket"), default="udp")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7001)
    parser.add_argument("--show-preview", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    run_server(args.transport, args.host, args.port, args.show_preview)


if __name__ == "__main__":
    main()
