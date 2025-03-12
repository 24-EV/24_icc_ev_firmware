#include <SPI.h>
#include <mcp_canbus.h>
#include "RTClib.h"

#define SPI_CS_PIN 9
MCP_CAN CAN(SPI_CS_PIN);  // Set CS pin

RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

// 변수 구조체 설정
struct SensorData {
  int RPM;
  int MOTOR_CUR;
  int BATT_PERCENT;
  int THROTTLE_SIG;
  int CTRL_TEMP;
  int SPEED;
  int BATT_VOLT;
};
SensorData data;

// 상수 설정
float wheeldiameter = 0.5;  // 휠 직경(m)
//float efficiency = 1.0;       // 효율 (부하가 걸렸을 때 모터 효율은 1로 가정)
float efficiency = 0.863;// * 0.94 * 0.98;  // (무부하의 rpm을 받아왔을 때 = 0.91 * 0.863 * 0.94 * 0.98)
float gear_ratio = 3.75;     // 기어비
float pi = 3.141592653589793; // 파이

// CAN 데이터 수신 상태 변수
bool id_1_received = false;
bool id_2_received = false;
unsigned long last_id_1_can_time = 0;  // 마지막으로 받은 ID 1의 CAN 메시지 시간
unsigned long last_id_2_can_time = 0;  // 마지막으로 받은 ID 2의 CAN 메시지 시간

const unsigned long message_delay = 100;  // 동일 ID 처리 간격

void setup() {
  Serial.begin(115200);   // 시리얼 모니터를 위한 시리얼 초기화
  Serial2.begin(115200);  // ESP32와의 UART 통신을 위한 시리얼 초기화

  while (!Serial) {
    ;  // 시리얼 포트 연결을 기다림
  }
  Serial.println("Serial communication initialized.");

  // CAN 버스 초기화
  if (CAN_OK == CAN.begin(CAN_500KBPS)) {
    Serial.println("CAN BUS OK!");
  } else {
    Serial.println("CAN BUS FAIL! Check wiring and connections.");
    while (1);
  }
}

void loop() {
  // CAN 메시지를 수신했는지 확인
  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    unsigned char len = 0;
    unsigned char rxBuf[8];  // CAN으로 받은 데이터를 저장할 버퍼

    CAN.readMsgBuf(&len, rxBuf);  // 데이터를 읽어와 버퍼에 저장
    unsigned long canId = CAN.getCanId();

    // ID 설정
    unsigned long id_1 = 0x0CF11E05;
    unsigned long id_2 = 0x0CF11F05;

    unsigned long current_time = millis();

    // ID 1 수신 처리
    if (canId == id_1 && current_time - last_id_1_can_time > message_delay) {
      data.RPM = (rxBuf[1] * 256) + rxBuf[0];  // RPM 값 수신

      // 모터 효율과 기어비를 반영하여 RPM을 Powertrain RPM으로 변환
      float rpm_powertrain = data.RPM * (sqrt(efficiency) / gear_ratio);

      // 속도 계산 (m/s)
      float velocity_mps = rpm_powertrain * (2 * pi * (wheeldiameter / 2) / 60);

      // 속도를 km/h로 변환
      data.SPEED = velocity_mps * 3.6;

      // 기타 데이터 처리
      data.MOTOR_CUR = ((rxBuf[3] * 256) + rxBuf[2]) / 10;
      data.BATT_VOLT = ((rxBuf[5] * 256) + rxBuf[4]) / 10;
      data.BATT_PERCENT = map(data.BATT_VOLT * 100, 4700, 5460, 0, 100);
      id_1_received = true;
      last_id_1_can_time = current_time;
    }
    // ID 2 수신 처리
    else if (canId == id_2 && current_time - last_id_2_can_time > message_delay) {
      data.THROTTLE_SIG = rxBuf[0];
      data.CTRL_TEMP = rxBuf[1] - 40;
      id_2_received = true;
      last_id_2_can_time = current_time;
    }
  }

  // ID 1과 ID 2가 모두 수신된 경우에만 데이터를 전송
  if (id_1_received && id_2_received) {
    sendDataToESP32();
    // 상태 초기화
    id_1_received = false;
    id_2_received = false;
  }

  delay(100);
}

void sendDataToESP32() {
  Serial.println("Sending data to ESP32...");

  // 데이터 전송 전 디버깅을 위해 구조체 멤버들을 개별적으로 출력
  Serial.print("RPM: ");
  Serial.println(data.RPM);
  Serial.print("MOTOR_CUR: ");
  Serial.println(data.MOTOR_CUR);
  Serial.print("BATT_VOLT: ");
  Serial.println(data.BATT_VOLT);
  Serial.print("THROTTLE_SIG: ");
  Serial.println(data.THROTTLE_SIG);
  Serial.print("CTRL_TEMP: ");
  Serial.println(data.CTRL_TEMP);
  Serial.print("SPEED (km/h): ");
  Serial.println(data.SPEED);  // km/h 단위로 속도를 출력
  Serial.print("BATT_PERCENT: ");
  Serial.println(data.BATT_PERCENT);

  // ESP32로 데이터 전송
  Serial2.write((uint8_t*)&data, sizeof(data));

  Serial.println("Data sent to ESP32");
  Serial.println("-----------------------------");
  Serial.println();  // 줄바꿈
}
