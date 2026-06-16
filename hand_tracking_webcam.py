import time
import urllib.request
from pathlib import Path

import cv2
import mediapipe as mp


MODEL_DIR = Path("models")
MODEL_PATH = MODEL_DIR / "hand_landmarker.task"
MODEL_URL = (
    "https://storage.googleapis.com/mediapipe-models/"
    "hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task"
)


def ensure_model() -> Path:
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    if not MODEL_PATH.exists():
        print(f"Downloading model to {MODEL_PATH} ...")
        urllib.request.urlretrieve(MODEL_URL, MODEL_PATH)
    return MODEL_PATH


def draw_hand_results(
    frame,
    result,
    hand_connections,
    drawing_utils,
) -> None:
    if not result.hand_landmarks:
        return

    for index, hand_landmarks in enumerate(result.hand_landmarks):
        drawing_utils.draw_landmarks(
            image=frame,
            landmark_list=hand_landmarks,
            connections=hand_connections,
        )

        if index < len(result.handedness) and result.handedness[index]:
            label = result.handedness[index][0].category_name
            wrist = hand_landmarks[0]
            text_position = (
                int(wrist.x * frame.shape[1]),
                max(30, int(wrist.y * frame.shape[0]) - 10),
            )
            cv2.putText(
                frame,
                label,
                text_position,
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                (0, 255, 0),
                2,
                cv2.LINE_AA,
            )


def main() -> None:
    model_path = ensure_model()

    BaseOptions = mp.tasks.BaseOptions
    HandLandmarker = mp.tasks.vision.HandLandmarker
    HandLandmarkerOptions = mp.tasks.vision.HandLandmarkerOptions
    RunningMode = mp.tasks.vision.RunningMode
    HandLandmarksConnections = mp.tasks.vision.HandLandmarksConnections
    drawing_utils = mp.tasks.vision.drawing_utils

    options = HandLandmarkerOptions(
        base_options=BaseOptions(model_asset_path=str(model_path)),
        running_mode=RunningMode.VIDEO,
        num_hands=2,
        min_hand_detection_confidence=0.6,
        min_hand_presence_confidence=0.6,
        min_tracking_confidence=0.6,
    )

    capture = cv2.VideoCapture(0)
    if not capture.isOpened():
        raise RuntimeError("Could not open the webcam.")

    previous_time = time.time()

    with HandLandmarker.create_from_options(options) as landmarker:
        while True:
            success, frame = capture.read()
            if not success:
                print("Failed to read a frame from the webcam.")
                break

            frame = cv2.flip(frame, 1)
            rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb_frame)
            timestamp_ms = int(time.time() * 1000)

            result = landmarker.detect_for_video(mp_image, timestamp_ms)
            draw_hand_results(
                frame=frame,
                result=result,
                hand_connections=HandLandmarksConnections.HAND_CONNECTIONS,
                drawing_utils=drawing_utils,
            )

            current_time = time.time()
            fps = 1.0 / max(current_time - previous_time, 1e-6)
            previous_time = current_time

            cv2.putText(
                frame,
                f"FPS: {fps:.1f}",
                (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                (0, 255, 255),
                2,
                cv2.LINE_AA,
            )
            cv2.putText(
                frame,
                "Press Q or ESC to quit",
                (10, frame.shape[0] - 15),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7,
                (255, 255, 255),
                2,
                cv2.LINE_AA,
            )

            cv2.imshow("MediaPipe Hand Tracking", frame)
            key = cv2.waitKey(1) & 0xFF
            if key in (27, ord("q")):
                break

    capture.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
