// 25년도 컨트롤러에 맞게 코드 수정함.
// 기존은 24년도 컨트롤러 코드. esp32 모듈이 없어서 정상 작동하는지 테스트 못 해봤습니다.

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
const char server[] = "43.203.166.209"; // 소켓IO 서버 IP
const char url[] = "/socket.io/?EIO=4"; // 소켓IO 서버 URL
const int port = 2004;                  // 소켓IO 서버 포트

char log_payload[512]; // JSON 메시지를 저장할 버퍼

SocketIOclient socketIO;
RingBuf<char, 1024> tx_buf;

unsigned long lastReceiveTime = 0;
const unsigned long timeoutDuration = 100;

char buffer[44]; // 44바이트 수신 버퍼
int bufferIndex = 0;
bool server_conn = false;

void setup()
{
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 16, 17);
    Serial.println("Serial2 initialized.");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi 연결 완료");

    socketIO.begin(server, port, url);
    socketIO.onEvent(socketIOEvent);
    Serial.println("Socket.IO 연결 시도 중...");
}

void loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Wi-Fi 연결 끊김, 재연결 시도 중...");
        WiFi.reconnect();
        delay(500);
        return;
    }

    socketIO.loop();

    if (Serial2.available() > 0)
    {
        unsigned long currentTime = millis();

        if (lastReceiveTime == 0)
        {
            lastReceiveTime = currentTime;
        }

        while (Serial2.available() > 0 && currentTime - lastReceiveTime < timeoutDuration)
        {
            char received = Serial2.read();
            buffer[bufferIndex++] = received;

            if (bufferIndex == 44)
            {
                processReceivedData();
                bufferIndex = 0;
                lastReceiveTime = 0;
                break;
            }

            currentTime = millis();
        }

        if (currentTime - lastReceiveTime >= timeoutDuration)
        {
            Serial.println("Timeout occurred, discarding data.");
            bufferIndex = 0;
            lastReceiveTime = 0;
        }
    }
}

void processReceivedData()
{
    for (int x = 0; x < 44; x++)
    {
        tx_buf.lockedPushOverwrite(buffer[x]);
    }

    if (!tx_buf.isEmpty())
    {
        char pop;
        char buf[44];

        for (int x = 0; x < 44; x++)
        {
            if (tx_buf.lockedPop(pop))
            {
                buf[x] = pop;
            }
            else
            {
                Serial.print("[empty]");
                break;
            }
        }

// 2바이트씩 병합 (Little Endian 기준)
#define GET_16BIT(i) ((uint16_t)((uint8_t)buf[i] | ((uint8_t)buf[i + 1] << 8)))

        uint16_t motor_temp_L = GET_16BIT(0);
        uint16_t controller_temp_L = GET_16BIT(2);
        uint16_t current_L = GET_16BIT(4);
        uint16_t voltage_L = GET_16BIT(6);
        uint16_t power_L = GET_16BIT(8);
        uint16_t rpm_L = GET_16BIT(10);
        uint16_t torque_L = GET_16BIT(12);
        uint16_t torque_cmd_L = GET_16BIT(14);

        uint16_t motor_temp_R = GET_16BIT(16);
        uint16_t controller_temp_R = GET_16BIT(18);
        uint16_t current_R = GET_16BIT(20);
        uint16_t voltage_R = GET_16BIT(22);
        uint16_t power_R = GET_16BIT(24);
        uint16_t rpm_R = GET_16BIT(26);
        uint16_t torque_R = GET_16BIT(28);
        uint16_t torque_cmd_R = GET_16BIT(30);

        uint16_t adc_signal = GET_16BIT(32);
        uint16_t speed = GET_16BIT(34);
        uint16_t yaw_rate = GET_16BIT(36);
        uint16_t steering_angle = GET_16BIT(38);
        uint16_t batt_percent = GET_16BIT(40);
        uint16_t total_power = GET_16BIT(42);

        // JSON 메시지 생성
        snprintf(log_payload, sizeof(log_payload),
                 "[\"tlog\", {"
                 "\"Motor_temp_L\":%u, \"Controller_temp_L\":%u, \"Current_L\":%u, \"Voltage_L\":%u, \"Power_L\":%u, \"RPM_L\":%u, \"Torque_L\":%u, \"Torque_cmd_L\":%u, "
                 "\"Motor_temp_R\":%u, \"Controller_temp_R\":%u, \"Current_R\":%u, \"Voltage_R\":%u, \"Power_R\":%u, \"RPM_R\":%u, \"Torque_R\":%u, \"Torque_cmd_R\":%u, "
                 "\"ADC_Signal\":%u, \"Speed\":%u, \"Yaw_rate\":%u, \"Steering_angle\":%u, \"Batt_percent\":%u, \"Total_power\":%u"
                 "}]",
                 motor_temp_L, controller_temp_L, current_L, voltage_L, power_L, rpm_L, torque_L, torque_cmd_L,
                 motor_temp_R, controller_temp_R, current_R, voltage_R, power_R, rpm_R, torque_R, torque_cmd_R,
                 adc_signal, speed, yaw_rate, steering_angle, batt_percent, total_power);

        if (server_conn)
        {
            socketIO.sendEVENT(log_payload);
            Serial.print("메시지 전송 완료: ");
            Serial.println(log_payload);
        }
    }
}

void socketIOEvent(socketIOmessageType_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case sIOtype_CONNECT:
        Serial.println("Socket.IO 연결 성공");
        server_conn = true;
        socketIO.send(sIOtype_CONNECT, "/");
        break;

    case sIOtype_DISCONNECT:
        Serial.println("Socket.IO 연결 해제");
        server_conn = false;
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
