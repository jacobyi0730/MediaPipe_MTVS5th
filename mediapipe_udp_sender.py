from mediapipe_gesture_server import run_server


if __name__ == "__main__":
    run_server("udp", "127.0.0.1", 7001, True)
