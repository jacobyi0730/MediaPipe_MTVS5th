# MediaPipe -> Unreal UDP

## Python sender

Run this from `C:\Projects\MediaPipe`:

```powershell
.venv\Scripts\activate
python mediapipe_udp_sender.py
```

It sends UDP binary packets to `127.0.0.1:7001` with this layout:

```text
2 bytes  identifier   0x4D50 ("MP")
2 bytes  cmd          0x0001
4 bytes  payload_len  4
4 bytes  payload      gesture code
```

Gesture payload values:

- `1`: fist
- `2`: scissors
- `3`: paper
- `0`: unknown or no hand

## Unreal side

The Unreal pawn `ASocketPlayer` now:

- binds UDP on port `7001`
- receives binary UDP packets every tick
- validates `identifier`, `cmd`, and `payload_len`
- parses the 4-byte gesture payload
- writes the decoded gesture into `WBP_LogUI` through `TextLog`

## Unreal editor steps

1. Open `UnrealProject/MyMediaPipeProject/MyMediaPipeProject.uproject`
2. Rebuild the C++ project when Unreal asks
3. Make sure `BP_SocketPlayer` uses `ASocketPlayer`
4. Make sure `LogUIFactory` points to `WBP_LogUI`
5. Run PIE
6. Start `mediapipe_udp_sender.py`

If everything is connected, the widget should show:

- packet identifier
- command
- payload length
- gesture code
- gesture name
- UDP port
