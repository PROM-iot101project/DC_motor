/*
    이 코드는 Node-RED에서 "motor" on/off 명령을 받아서 L293D 모터를 제어하고,
    수동 버튼(토글)도 가능하도록 한 예제입니다.

    ▪ Node-RED publish 예시:
      { "d": { "motor": "on" } } → 모터 ON
      { "d": { "motor": "off" } } → 모터 OFF

    ▪ 수동 버튼(PUSH_BUTTON)을 누르면 manualOverride 토글 (on/off)
    ▪ publishData()를 통해 현재 상태를 MQTT로 publish
*/

/*
    **핵심 설명**
    - MQTT에서 motor 키 값("on"/"off")를 받아 relayOn 상태 제어
    - loop에서 manualOverride(버튼) 또는 relayOn 값으로 모터 제어
    - 수동 버튼 입력 시 manualOverride 토글
    - 주기적으로 또는 상태 변화 시 publishData() 호출
*/

/*
    **주요 포인트**
    - handleUserCommand에서 d["motor"] 처리
    - loop에서 IN1, IN2, ENA 핀 제어
    - manualOverride가 최우선, 그 다음 relayOn 기준으로 모터 상태 결정
    - 상태 변화 시 switchChanged = true → 즉시 publish
*/

#include <Arduino.h>
#include <IO7F32.h>
#include <WiFi.h>

// 핀 설정
const int IN1 = 27;               // L293D IN1 → ESP32 GPIO 27
const int IN2 = 26;               // L293D IN2 → ESP32 GPIO 26
const int ENA = 25;               // L293D ENA → ESP32 GPIO 25
const int PUSH_BUTTON = 33;       // 수동 토글 버튼 → GPIO 33

// 상태 변수
bool relayOn = false;             // 서버 명령으로 켜는 상태
bool manualOverride = false;      // 수동 버튼으로 강제 ON
bool switchChanged = false;       // 상태 변화 발생 시 publish 트리거

bool buttonOldState = false;      // 버튼 상태(이전 값 저장)

char* ssid_pfix = (char*)"IOT_DC_Motor";   // WiFi SSID 접두사
unsigned long lastPublishMillis = -pubInterval;  // 마지막 publish 시각

// 상태 publish 함수
void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    data["relay"] = relayOn ? "on" : "off";
    data["manual_override"] = manualOverride ? "on" : "off";

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);

    Serial.printf("[Publish] Relay: %s, ManualOverride: %s\n",
                  relayOn ? "on" : "off",
                  manualOverride ? "on" : "off");
}

// 서버에서 온 명령 처리 (motor 키 기준으로 처리)
void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];
    Serial.println(topic);

    if (d.containsKey("motor")) {
        const char* motorState = d["motor"];

        if (strcmp(motorState, "on") == 0) {
            if (!relayOn) {
                relayOn = true;
                switchChanged = true;
                Serial.println("[Server] Motor ON");
            }
        } else if (strcmp(motorState, "off") == 0) {
            if (relayOn) {
                relayOn = false;
                switchChanged = true;
                Serial.println("[Server] Motor OFF");
            }
        }

        lastPublishMillis = -pubInterval;  // 즉시 publish
    }
}

// 초기 설정
void setup() {
    Serial.begin(115200);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(ENA, OUTPUT);
    digitalWrite(ENA, LOW);           // 초기 모터 OFF

    pinMode(PUSH_BUTTON, INPUT_PULLUP);  // 버튼 PULLUP 사용

    initDevice();
    JsonObject meta = cfg["meta"];

    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
    lastPublishMillis = -pubInterval;

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    userCommand = handleUserCommand;

    set_iot_server();
    iot_connect();
}

// 메인 루프
void loop() {
    // 1️⃣ 버튼 처리
    int buttonState = digitalRead(PUSH_BUTTON);

    if (buttonOldState == HIGH && buttonState == LOW) {
        manualOverride = !manualOverride;
        switchChanged = true;
        Serial.printf("[Button] ManualOverride toggled to %s\n",
                      manualOverride ? "on" : "off");
        lastPublishMillis = -pubInterval;
        delay(200);  // 디바운스
    }
    buttonOldState = buttonState;

    // 2️⃣ 모터 제어
    if (manualOverride) {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(ENA, HIGH);
    } else if (relayOn) {
        digitalWrite(IN1, HIGH);
        digitalWrite(IN2, LOW);
        digitalWrite(ENA, HIGH);
    } else {
        digitalWrite(ENA, LOW);
    }

    // 3️⃣ client.loop() 처리
    static unsigned long lastClientLoopTime = 0;
    if (millis() - lastClientLoopTime > 50) {
        client.loop();
        lastClientLoopTime = millis();
    }

    // 4️⃣ 상태 publish 처리
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }

    if (switchChanged) {
        publishData();
        switchChanged = false;
    }
}
