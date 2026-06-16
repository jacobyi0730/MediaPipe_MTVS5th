# MediaPipe -> Unreal transport setup

## Python server

The shared server script is:

```powershell
python mediapipe_gesture_server.py --transport udp --host 127.0.0.1 --port 7001 --show-preview
```

Supported transports:

- `udp`
- `tcp`
- `websocket`

The legacy UDP-only entry point still works:

```powershell
python mediapipe_udp_sender.py
```

## Packet format

All transports use the same 12-byte binary packet:

```text
2 bytes  identifier   0x4D50 ("MP")
2 bytes  cmd          0x0001
4 bytes  payload_len  4
4 bytes  payload      gesture code
```

Gesture payload values:

- `0`: unknown or no hand
- `1`: fist
- `2`: scissors
- `3`: paper

## Unreal side

`ASocketPlayer` now exposes these settings:

- `TransportType`: `UDP`, `TCP`, or `WebSocket`
- `bAutoLaunchServer`: launch the Python MediaPipe server automatically
- `bShowServerPreview`: show the OpenCV preview window
- `ServerHost`
- `ServerPort`
- `PythonExecutablePath`
- `ServerScriptPath`

If `bAutoLaunchServer` is enabled, Unreal launches:

- `mediapipe_gesture_server.py`
- with the selected transport
- on the configured host and port

## Unreal editor steps

1. Open `UnrealProject/MyMediaPipeProject/MyMediaPipeProject.uproject`
2. Rebuild the C++ project when Unreal asks
3. Open `BP_SocketPlayer`
4. Set `TransportType` to the protocol you want
5. Make sure `LogUIFactory` points to `WBP_LogUI`
6. Run PIE

The widget should show:

- selected transport
- packet identifier
- command
- payload length
- gesture code
- decoded gesture name
- host and port
