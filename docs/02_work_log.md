# 작업 내역

## 1. 환경 구성

### 1.1 Python 설치 및 가상환경

수행 내용:

- `uv` 설치 상태 확인
- Python 3.12 설치 상태 확인
- 프로젝트 루트에 `.venv` 생성

결과:

- Python 3.12 가상환경 사용 가능 상태 확보

### 1.2 Python 패키지 구성

추가 의존성:

- `mediapipe`
- `opencv-python`
- `websockets`

관련 파일:

- `pyproject.toml`
- `uv.lock`

## 2. MediaPipe 단독 예제 작성

### 2.1 웹캠 손 추적 예제

수행 내용:

- `hand_tracking_webcam.py` 작성
- MediaPipe Tasks Vision 기반 손 추적 적용
- 랜드마크 오버레이 및 좌우 손 텍스트 표시

결과:

- 단독 웹캠 손 추적 예제 실행 가능

### 2.2 브라우저 데모 작성

수행 내용:

- `index.html`, `app.js`, `styles.css` 작성
- 브라우저 카메라 입력 기반 손 추적 구현
- 가위바위보 분류 로직 추가

결과:

- 로컬 브라우저 기반 데모 페이지 구성

## 3. 제스처 분류 로직 개선

초기 방식:

- 손가락 끝 좌표의 단순 방향 비교

문제:

- 손등/손바닥 방향이 바뀌면 분류 안정성 저하

개선 내용:

- 손가락 관절 각도
- 손목 대비 손가락 거리 비율
- 엄지 spread/reach 기준

결과:

- 주먹, 가위, 보 분류 정확도 개선 시도
- 다만 손 회전/가림 상황에서는 여전히 추가 튜닝 필요

## 4. Unreal 연동 1차: UDP

### 4.1 Unreal C++ 수신기 추가

수행 내용:

- `ASocketPlayer`에 UDP 소켓 수신 로직 추가
- `ULogUI`에 텍스트 갱신 기능 추가
- 패킷 내용 UI 표시

관련 파일:

- `SocketPlayer.h`
- `SocketPlayer.cpp`
- `LogUI.h`
- `LogUI.cpp`
- `MyMediaPipeProject.Build.cs`

### 4.2 패킷 방식 변경

초기 방식:

- JSON 문자열 전송

변경 방식:

- 12바이트 고정 길이 바이너리 패킷

변경 이유:

- Unreal 수신 파싱 단순화
- 프로토콜 확장 여지 확보

## 5. Unreal 연동 2차: 다중 전송 방식

### 5.1 Python 서버 구조 변경

수행 내용:

- `mediapipe_gesture_server.py` 작성
- `udp`, `tcp`, `websocket` 선택형 전송 지원
- `mediapipe_udp_sender.py`는 UDP 전용 실행 래퍼로 축소

결과:

- 공용 서버 진입점 확보

### 5.2 Unreal 다중 전송 방식 구조 추가

수행 내용:

- `EMediaPipeTransportType` enum 추가
- UDP/TCP/WebSocket별 초기화 및 종료 로직 분리
- 패킷 버퍼 파싱 공통화
- Python 서버 자동 실행 로직 추가

결과:

- Unreal 쪽에서 전송 방식 선택을 수용할 수 있는 구조 형성

## 6. Unreal UI 선택 흐름 작업

수행 내용:

- `ULogUI`에 콤보박스 추가
- `Start Selected Transport` 버튼 추가
- 버튼 클릭 시 `StartMediaPipeWithTransport(...)` 호출
- `bAutoLaunchServer` 기본값 `false`로 수정

현재 이슈:

- 실제 런타임에서 선택 UI가 기대대로 노출되는지 검증 필요
- 기존 `WBP_LogUI` 블루프린트와 코드 기반 UI 생성 방식이 충돌할 가능성 존재

## 7. 설정 파일 및 정리 작업

수행 내용:

- 루트 `.gitignore` 추가
- Unreal 프로젝트 `.gitignore` 추가
- `UNREAL_UDP_SETUP.md` 작성 및 갱신
- `docs/` 폴더 문서화 시작

## 8. 현재 작업 상태

완료된 항목:

- Python 환경
- MediaPipe 손 인식
- 제스처 분류
- 바이너리 패킷 설계
- Unreal 수신 구조
- 다중 전송 방식 확장 코드

남은 항목:

- Unreal 런타임 UI 선택 흐름 최종 검증
- TCP/WebSocket 실제 통신 확인
- 코드 안정화
- 문서 기반 Word 산출물 정리
