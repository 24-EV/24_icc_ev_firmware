#include <WiFi.h>
#include <RingBuf.h>
#include <ArduinoJson.h>
#include <SocketIOclient.h>

// SON_ID
#define SON_ID "nitepp04"
#define SON_PWD "00000000"

// MY_ID
#define MY_ID "kimchunsick"
#define MY_PWD "01234567890"

// Hotspot AP configurations
const char ssid[] = MY_ID;
const char password[] = MY_PWD;

// 서버 정보
const char server[] = "43.203.166.209";  // 소켓IO 서버 IP
const char url[] = "/socket.io/?EIO=4";  // 소켓IO 서버 URL
const int port = 2004;                   // 소켓IO 서버 포트

char log_payload[256];  // JSON 메시지를 저장할 버퍼
//이렇게도 변경해 테스트 시도 필요
//char log_payload[42] = "[\"tlog\",{\"log\":\"";

// Socket.IO 클라이언트
SocketIOclient socketIO;

// RingBuffer 생성
RingBuf<char, 1024> tx_buf;

unsigned long lastReceiveTime = 0;          // 마지막 수신 시간 기록
const unsigned long timeoutDuration = 100;  // 100ms 타임아웃
char buffer[14];                            // 14바이트 데이터를 저장할 버퍼
int bufferIndex = 0;                        // 버퍼 인덱스

bool server_conn = false;  // 서버 연결 상태를 추적할 플래그

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // UART2 설정 (RX: GPIO 16, TX: GPIO 17)
  Serial.println("Serial2 initialized.");

  // Wi-Fi 연결
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wi-Fi 연결 대기
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi 연결 완료");

  // Socket.IO 서버 연결
  socketIO.begin(server, port, url);
  socketIO.onEvent(socketIOEvent);
  Serial.println("Socket.IO 연결 시도 중...");
}

void loop() {
  // Wi-Fi 연결 확인
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi 연결 끊김, 재연결 시도 중...");
    WiFi.reconnect();
    delay(500);
    return;
  }

  // Socket.IO 루프 유지
  socketIO.loop();

  if (Serial2.available() > 0) {
    unsigned long currentTime = millis();

    // 수신 시작 시점 기록
    if (lastReceiveTime == 0) {
      lastReceiveTime = currentTime;
    }

    // 수신된 데이터 처리
    while (Serial2.available() > 0 && currentTime - lastReceiveTime < timeoutDuration) {
      char received = Serial2.read();
      buffer[bufferIndex++] = received;

      // 14바이트가 모두 수신된 경우
      if (bufferIndex == 14) {
        processReceivedData();  // 수신된 데이터를 처리
        bufferIndex = 0;        // 인덱스 초기화
        lastReceiveTime = 0;    // 타임아웃 초기화
        break;
      }

      // 현재 시간 업데이트
      currentTime = millis();
    }

    // 타임아웃이 발생한 경우
    if (currentTime - lastReceiveTime >= timeoutDuration) {
      Serial.println("Timeout occurred, discarding data.");
      bufferIndex = 0;      // 데이터 무효화
      lastReceiveTime = 0;  // 타임아웃 초기화
    }
  }
}

void processReceivedData() {

  // 데이터를 RingBuf에 저장
  for (int x = 0; x < 14; x++) {
    tx_buf.lockedPushOverwrite(buffer[x]);
  }

  // 디버깅: RingBuf에 저장된 데이터를 출력
  if (!tx_buf.isEmpty()) {
    char pop;
    char buf[14];

    for (int x = 0; x < 14; x++) {
      if (tx_buf.lockedPop(pop)) {
        buf[x] = pop;
      } else {
        Serial.print("[empty]");
        break;
      }
    }

    // 데이터 전송부
    sprintf(log_payload,
            "[\"tlog\", {\"data\":[\"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\", \"%02x\"]}]",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
            buf[8], buf[9], buf[10], buf[11], buf[12], buf[13]);

    // 서버에 연결되어 있는 상태라면 주기적으로 메시지 전송
    if (server_conn) {
      socketIO.sendEVENT(log_payload);  // JSON 메시지 전송
      Serial.print("메시지 전송 완료: ");
      Serial.println(log_payload);  // 전송된 메시지를 시리얼 모니터에 출력(디버깅)
    }
    Serial.println();
  }
}

// Socket.IO 이벤트 처리 함수
void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case sIOtype_CONNECT:
      Serial.println("Socket.IO 연결 성공");
      server_conn = true;  // 서버 연결 상태 플래그를 true로 설정
      socketIO.send(sIOtype_CONNECT, "/");
      break;

    case sIOtype_DISCONNECT:
      Serial.println("Socket.IO 연결 해제");
      server_conn = false;  // 서버 연결 상태 플래그를 false로 설정
      break;

    case sIOtype_EVENT:
      Serial.print("이벤트 수신: ");
      Serial.println((char *)payload);
      break;

    case sIOtype_ERROR:
      Serial.println("Socket.IO 오류 발생");
      break;

    default:
      break;
  }
}
