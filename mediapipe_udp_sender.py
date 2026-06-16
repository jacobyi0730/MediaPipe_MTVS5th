import math
import socket
import struct
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import cv2
import mediapipe as mp


MODEL_PATH = Path("models/hand_landmarker.task")
UDP_IP = "127.0.0.1"
UDP_PORT = 7001
PACKET_IDENTIFIER = 0x4D50
CMD_GESTURE = 0x0001
PAYLOAD_SIZE = 4
WINDOW_NAME = "MediaPipe UDP Sender"
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
    return struct.pack(
        "!HHII",
        PACKET_IDENTIFIER,
        CMD_GESTURE,
        PAYLOAD_SIZE,
        gesture_code,
    )


def draw_preview(frame: Any, gesture_code: int) -> None:
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
        "Press Q or ESC to quit",
        (20, frame.shape[0] - 20),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.7,
        (255, 255, 255),
        2,
        cv2.LINE_AA,
    )


def main() -> None:
    if not MODEL_PATH.exists():
        raise FileNotFoundError(f"Model file not found: {MODEL_PATH}")

    base_options = mp.tasks.BaseOptions(model_asset_path=str(MODEL_PATH))
    options = mp.tasks.vision.HandLandmarkerOptions(
        base_options=base_options,
        running_mode=mp.tasks.vision.RunningMode.VIDEO,
        num_hands=1,
        min_hand_detection_confidence=0.6,
        min_hand_presence_confidence=0.6,
        min_tracking_confidence=0.6,
    )

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    capture = cv2.VideoCapture(0)
    if not capture.isOpened():
        raise RuntimeError("Could not open the webcam.")

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
                landmarks = result.hand_landmarks[0]
                gesture_code = classify_gesture(landmarks)

            sock.sendto(build_packet(gesture_code), (UDP_IP, UDP_PORT))
            draw_preview(frame, gesture_code)
            cv2.imshow(WINDOW_NAME, frame)

            if cv2.waitKey(1) & 0xFF in (27, ord("q")):
                break

    capture.release()
    sock.close()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
