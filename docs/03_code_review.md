# 코드리뷰

## 1. 리뷰 범위

리뷰 대상:

- `mediapipe_gesture_server.py`
- `mediapipe_udp_sender.py`
- `UnrealProject/MyMediaPipeProject/Source/MyMediaPipeProject/Public/SocketPlayer.h`
- `UnrealProject/MyMediaPipeProject/Source/MyMediaPipeProject/Private/SocketPlayer.cpp`
- `UnrealProject/MyMediaPipeProject/Source/MyMediaPipeProject/Public/LogUI.h`
- `UnrealProject/MyMediaPipeProject/Source/MyMediaPipeProject/Private/LogUI.cpp`

리뷰 기준:

- 동작 정확성
- 런타임 안정성
- 구조 적합성
- 유지보수성

## 2. 주요 findings

### Finding 1. `ULogUI`가 런타임에 위젯 트리 루트를 강제로 교체함

위치:

- `UnrealProject/MyMediaPipeProject/Source/MyMediaPipeProject/Private/LogUI.cpp`

문제:

- `EnsureRuntimeControls()`에서 `WidgetTree->RootWidget`를 코드로 교체한다.
- 기존 `WBP_LogUI` 블루프린트에 디자이너가 구성한 레이아웃이나 바인딩이 있으면 무시되거나 유실될 수 있다.
- 현재 증상인 "초기 텍스트만 보이고 선택 UI가 기대대로 동작하지 않음"도 이 구조와 연관될 가능성이 높다.

영향:

- 블루프린트 UI와 C++ UI가 충돌
- 위젯 유지보수성 저하
- UI 디버깅 난이도 상승

권고:

- 코드에서 루트를 재구성하는 방식 대신 `WBP_LogUI`에 명시적으로 `ComboBox`, `Button`, `TextBlock`를 배치하고 `BindWidget`로 연결하는 구조가 더 안전하다.

### Finding 2. `StartMediaPipeWithTransport()`가 항상 Python 서버 프로세스를 실행함

위치:

- `UnrealProject/MyMediaPipeProject/Source/MyMediaPipeProject/Private/SocketPlayer.cpp`

문제:

- 현재는 UI 버튼을 누르면 `bAutoLaunchServer`와 관계없이 `LaunchMediaPipeServer()`를 호출한다.
- 사용자가 이미 외부에서 Python 서버를 수동 실행 중이어도, Unreal이 다시 동일 포트로 새 프로세스를 띄우려고 시도한다.

영향:

- 포트 충돌 가능성
- 중복 프로세스 실행
- 디버깅 시 혼동

권고:

- `bAutoLaunchServer`와 별도로 `bLaunchServerOnButtonClick` 같은 정책 플래그를 두거나
- 이미 실행 중인 서버 프로세스/포트 상태를 먼저 검사한 후 실행하는 것이 적절하다.

### Finding 3. TCP 연결 초기화와 재연결 로직이 비결정적임

위치:

- `UnrealProject/MyMediaPipeProject/Source/MyMediaPipeProject/Private/SocketPlayer.cpp`

문제:

- 비동기/non-blocking `Connect()` 호출 결과를 단순 성공/실패로 해석하고 있다.
- 실제로는 플랫폼에 따라 "연결 진행 중"과 "실패"의 구분이 필요하다.
- `GetConnectionState()` 기반 재시도도 충분한 상태 전이를 보장하지 않는다.

영향:

- TCP가 실제 연결되었는데도 실패로 보일 수 있음
- 연결 타이밍에 따라 패킷 수신 불안정

권고:

- 연결 상태를 명시적 상태 머신으로 관리하고
- 서버 실행 후 초기 지연 시간 또는 health check를 두는 편이 안전하다.

### Finding 4. WebSocket 구현은 엔진 모듈 의존성에 민감함

위치:

- `MyMediaPipeProject.Build.cs`
- `SocketPlayer.cpp`

문제:

- 현재 엔진 환경에서는 `WebSockets`가 플러그인이 아니라 런타임 모듈이다.
- 이 차이를 잘못 다루면 쉽게 빌드 오류가 발생한다.
- 이미 한 차례 `.uproject` 플러그인 선언 문제를 겪었다.

영향:

- Unreal 버전/설치 상태에 따라 빌드 실패 가능

권고:

- 문서에 `UE 5.7 기준 WebSockets는 모듈 의존성만 추가`라고 명확히 남기고
- 버전별 차이를 별도 정리하는 것이 좋다.

### Finding 5. Python 쪽 제스처 분류는 휴리스틱 기반이라 오분류 여지가 남아 있음

위치:

- `mediapipe_gesture_server.py`

문제:

- 제스처 분류가 랜드마크 각도와 거리 비율의 휴리스틱 조합으로 구성되어 있다.
- 손 회전, 가림, 비정형 손모양에 대해 안정성이 떨어질 수 있다.

영향:

- 실제 Unreal 쪽 통신은 정상이어도 체감 품질이 낮아질 수 있음

권고:

- 분류 임계값 로그를 수집해 튜닝하거나
- MediaPipe Gesture Recognizer 기반으로 전환 검토

## 3. 긍정적인 점

### 3.1 패킷 포맷이 단순하고 명확함

- 12바이트 고정 길이 패킷은 Unreal/C++에서 파싱이 단순하다.
- 디버깅 포인트가 명확하다.

### 3.2 전송 방식별 분리 구조는 확장 가능성이 있음

- UDP/TCP/WebSocket을 enum으로 분리한 구조는 방향성이 맞다.
- 이후 시리얼라이저나 메시지 타입 확장에도 유리하다.

### 3.3 Python과 Unreal 역할 경계가 비교적 명확함

- Python은 인식과 송신
- Unreal은 수신과 표시

이 구조는 책임 분리가 분명한 편이다.

## 4. 우선순위 권고안

### 우선순위 1

- `WBP_LogUI`를 기준으로 명시적 UI 바인딩 구조로 변경

### 우선순위 2

- 선택 UI 클릭 후 실제 서버 실행/연결 상태를 단계별 로그로 표시

예:

- `Launching Python server`
- `Binding UDP socket`
- `TCP connected`
- `WebSocket connected`

### 우선순위 3

- TCP/WebSocket 재연결 상태 머신 보강

### 우선순위 4

- 제스처 분류 정확도 튜닝 또는 모델 기반 인식 전환 검토

## 5. 결론

현재 프로젝트는 프로토타입 수준의 목적에는 충분히 진척되어 있다.

특히 다음은 이미 확보되었다.

- MediaPipe 손 인식
- Unreal 연동용 패킷 구조
- 다중 전송 방식 확장 구조

다만 실사용 수준으로 가려면 아래 2가지가 가장 중요하다.

1. Unreal UI 선택 흐름의 실제 동작 보장
2. 전송 방식별 연결 안정성 확보

즉, 현재 단계는 "구조는 만들어졌고, 런타임 UX와 안정성 검증이 남은 상태"로 평가할 수 있다.
