# 프로젝트 개요

## 1. 목적

이 프로젝트는 노트북 웹캠으로 손 제스처를 인식한 뒤, 그 결과를 Unreal Engine 프로젝트와 연동하기 위한 프로토타입이다.

현재 목표는 다음 3가지다.

1. MediaPipe 기반 손 인식 및 가위바위보 분류
2. 제스처 결과를 네트워크 패킷으로 송신
3. Unreal UI에서 선택한 통신 방식으로 결과를 수신 및 표시

## 2. 현재 범위

현재 구현 범위는 아래와 같다.

- Python 3.12 + `uv` 기반 실행 환경
- MediaPipe Tasks Vision 기반 손 랜드마크 추적
- 가위, 바위, 보 제스처 분류
- 고정 길이 12바이트 바이너리 패킷 생성
- UDP, TCP, WebSocket 전송 방식 지원
- Unreal Engine 5.7 C++ 프로젝트와 연동
- Unreal UI 텍스트 출력 및 전송 방식 선택 UI 구성 시도

## 3. 루트 디렉터리 구조

```text
MediaPipe/
├─ docs/
├─ models/
├─ UnrealProject/
│  └─ MyMediaPipeProject/
├─ hand_tracking_webcam.py
├─ mediapipe_gesture_server.py
├─ mediapipe_udp_sender.py
├─ index.html
├─ app.js
├─ styles.css
├─ pyproject.toml
├─ uv.lock
└─ UNREAL_UDP_SETUP.md
```

## 4. 주요 파일 설명

### 4.1 Python 영역

- `pyproject.toml`
  Python 의존성 및 실행 환경 정의
- `mediapipe_gesture_server.py`
  공용 MediaPipe 서버. `udp`, `tcp`, `websocket` 전송을 선택 가능
- `mediapipe_udp_sender.py`
  UDP 전용 단축 실행 진입점
- `hand_tracking_webcam.py`
  단독 웹캠 손 추적 예제

### 4.2 웹 데모 영역

- `index.html`
  로컬 브라우저 데모 진입점
- `app.js`
  브라우저 기반 손 추적 및 가위바위보 분류
- `styles.css`
  브라우저 UI 스타일

### 4.3 Unreal 영역

- `UnrealProject/MyMediaPipeProject/MyMediaPipeProject.uproject`
  Unreal 프로젝트 메타데이터
- `Source/MyMediaPipeProject/MyMediaPipeProject.Build.cs`
  Unreal 모듈 의존성 정의
- `Source/MyMediaPipeProject/Public/SocketPlayer.h`
  Unreal 쪽 통신/프로세스 제어 중심 클래스 선언
- `Source/MyMediaPipeProject/Private/SocketPlayer.cpp`
  UDP/TCP/WebSocket 처리, Python 프로세스 실행, 패킷 파싱 로직
- `Source/MyMediaPipeProject/Public/LogUI.h`
  UI 위젯 선언
- `Source/MyMediaPipeProject/Private/LogUI.cpp`
  실행 시 UI 컨트롤 생성 및 버튼 이벤트 연결

## 5. 데이터 흐름

### 5.1 목표 데이터 흐름

```text
웹캠
  -> MediaPipe 손 랜드마크 추적
  -> 제스처 분류
  -> 바이너리 패킷 생성
  -> UDP/TCP/WebSocket 전송
  -> Unreal 수신
  -> 패킷 해석
  -> UI 표시
```

### 5.2 패킷 구조

현재 패킷은 모든 전송 방식에서 공통으로 사용된다.

| 필드 | 크기 | 값 |
|---|---:|---|
| Identifier | 2 bytes | `0x4D50` |
| Cmd | 2 bytes | `0x0001` |
| Payload Length | 4 bytes | `4` |
| Payload | 4 bytes | 제스처 코드 |

제스처 코드:

- `0` : Unknown 또는 No hand
- `1` : Fist
- `2` : Scissors
- `3` : Paper

## 6. Unreal 실행 구조

현재 `ASocketPlayer`는 아래 역할을 가진다.

1. `ULogUI` 생성 및 뷰포트에 추가
2. 선택된 전송 방식에 따라 UDP, TCP, WebSocket 중 하나 초기화
3. 필요 시 Python MediaPipe 서버 프로세스 실행
4. 수신 버퍼에서 12바이트 패킷 파싱
5. 파싱 결과를 UI 텍스트로 출력

## 7. 현재 상태 요약

구현 완료:

- Python 실행 환경 구성
- MediaPipe 손 추적 및 제스처 분류
- UDP 패킷 기반 Unreal 연동
- TCP/WebSocket 확장 구조 추가
- Unreal 쪽 선택형 전송 로직 추가

진행 중 또는 검증 필요:

- Unreal UI에서 실제 전송 방식 선택 후 실행 플로우 검증
- TCP 연결 재시도 안정성 검증
- WebSocket 연결 및 수신 검증
- `WBP_LogUI`와 코드 기반 UI 구성 충돌 여부 검증
