/*

    * 미세먼지 센서에 On 이라고 보내는 부분이 없어서 아래와 같다고 가정하고 작성했습니다. 중간에 if문을 추가했어요!

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");
    data["dust"] = ewaDust;

    // ***PM2.5가 35일 때를 기준으로 on/off 상태 추가***
    if (ewaDust >= 35) {      // 실제로 35부터 주의단계라고 하네용
        data["dust_state"] = "on";
    } else {
        data["dust_state"] = "off";
    }

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
    Serial.printf("[Publish] EWA Dust: %.2f µg/m³, State: %s\n", ewaDust, data["dust_state"]);
}
*/

/*

    * 사용 부품 : L293D 모터 드라이버, ywrobot ezmotor r300 (DC 모터), 따로 전원공급 필요! (건전지 4개 정도)
    * 근데 문제는, 제가 L293D를 실제로 갖고 있지 않아서... 잘 작동하는지 확인은 못했습니다...ㅠㅠ
    * Io7 Lamp 기반으로 수정했습니다! 수정한 부분은 주석으로 표시했습니다.

*/


#include <Arduino.h>             
#include <IO7F32.h>              
#include <WiFi.h>                


// 수정한 부분
const int IN1 = 22;              // L293D IN1 을 esp32 GPIO 22번에 연결
const int IN2 = 23;              // L293D IN2 를 esp32 GPIO 23번에 연결
const int ENA = 5;               // L293D ENA 를 esp32 GPIO 5번에 연결


char* ssid_pfix = (char*)"IOT_DC_Motor";             // WiFi SSID 설정용 접두사
unsigned long lastPublishMillis = -pubInterval;      // 마지막 publish 시간 초기화 (간격 제어용 변수)

// 수정한 부분
// 모터 상태를 MQTT로 publish
void publishData() {
    StaticJsonDocument<512> root;                    // JSON 문서 생성
    JsonObject data = root.createNestedObject("d");  // "d" 객체 생성 (IoT 프로토콜 표준)

    // 수정한 부분
    data["motor"] = digitalRead(ENA) == HIGH ? "on" : "off";  // 현재 EMA 핀 상태를 기준으로 모터 상태를 "on" 또는 "off"로 설정

    serializeJson(root, msgBuffer);                  // JSON 문서를 문자열로 직렬화
    client.publish(evtTopic, msgBuffer);             // MQTT로 publish
}

// 수정한 부분
// 수신된 MQTT 메시지를 처리하는 함수
void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];      // 수신된 JSON에서 "d" 객체를 가져옴
    Serial.println(topic);            // 디버그 출력

    // 수정한 부분
    // dust_state 가 35 이상일 때 모터를 켜고, 그렇지 않으면 끔
    if (d.containsKey("dust_state")) {
        if (strstr(d["dust_state"], "on")) {
            digitalWrite(IN1, HIGH);          // 모터 방향 설정
            digitalWrite(IN2, LOW);
            digitalWrite(ENA, HIGH);          // 모터 켜기
        } else if (strstr(d["dust_state"], "off")) {
            digitalWrite(ENA, LOW);           // 모터 끄기
        }
        lastPublishMillis = -pubInterval;     // 모터 상태가 변경되었으므로, 다음 publish 시점까지 기다리지 않고 즉시 publish. 초기화.
    }
}

// 초기 설정 함수
void setup() {
    Serial.begin(115200);             // 시리얼 통신 시작 (초기화)

    // 수정한 부분
    pinMode(IN1, OUTPUT);             // 모터 방향 핀 설정
    pinMode(IN2, OUTPUT);
    pinMode(ENA, OUTPUT);             
    digitalWrite(ENA, LOW);           // 초기에는 모터 OFF

    initDevice();                     // Io7F32 라이브러리 초기화 및 설정 로드
    JsonObject meta = cfg["meta"];    // 설정에서 메타데이터 가져오기

    // publish 주기 설정 (없으면 0으로 설정)
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
    lastPublishMillis = -pubInterval;

    // WiFi 연결 설정
    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");            // WiFi 연결 대기 중에 점 출력력
    }

    Serial.printf("\nIP address : "); // 연결 성공 시 IP 출력
    Serial.println(WiFi.localIP());

    userCommand = handleUserCommand;  

    set_iot_server();                 // MQTT 서버 설정 및 연결
    iot_connect();                    
}

// 메인 루프 함수
void loop() {
    if (!client.connected()) {
        iot_connect();                // 연결 끊겼으면 MQTT 서버에 재연결
    }
    client.loop();                    // MQTT client loop (수신 대기)

    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();              
        lastPublishMillis = millis();
    }
}
