#include <PyArduTalk.h>
#include <ArduinoJson.h>

// 创建 PyArduTalk 对象，使用 UART1 (GPIO 20 TX, GPIO 21 RX)
PyArduTalk pyArduTalk(Serial1);

// 回调函数定义
void handleInt(int16_t value) {
    Serial.print("回调 - 接收到整数: ");
    Serial.println(value);
}

void handleFloat(float value) {
    Serial.print("回调 - 接收到浮点数: ");
    Serial.println(value);
}

void handleString(const String& value) {
    Serial.print("回调 - 接收到字符串: ");
    Serial.println(value);
}

void handleJson(const StaticJsonDocument<256>& doc) {
    Serial.print("回调 - 接收到JSON数据: ");
    serializeJson(doc, Serial);
    Serial.println();
}

// （可选）回调函数定义，用于处理回显帧
/*
void handleEchoFrame(const byte* frame, size_t length) {
    Serial.print("回显帧: ");
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", frame[i]);
    }
    Serial.println();
}
*/

void setup() {
    // 初始化内置 LED 引脚（如果需要）
    // pinMode(LED_BUILTIN, OUTPUT);
    // digitalWrite(LED_BUILTIN, LOW);

    // 初始化串口用于调试
    Serial.begin(115200);
    while (!Serial) {
        ; // 等待串口连接
    }
    delay(1000); // 确保 Serial 开启

    // 初始化 Serial1（UART1）
    Serial1.begin(115200, SERIAL_8N1, 20, 21);

    // 初始化 PyArduTalk
    pyArduTalk.begin();

    // 设置回调函数
    pyArduTalk.onIntReceived(handleInt);
    pyArduTalk.onFloatReceived(handleFloat);
    pyArduTalk.onStringReceived(handleString);
    pyArduTalk.onJsonReceived(handleJson);

    // （可选）设置回调函数，用于处理回显帧
    // pyArduTalk.onEchoFrame(handleEchoFrame);
}

void loop() {
    pyArduTalk.loop();

    // 示例：定时发送不同类型的数据
    static unsigned long lastIntSendTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastIntSendTime > 5000) { // 每5秒发送一次整数
        lastIntSendTime = currentTime;
        int16_t exampleInt = 12345;
        pyArduTalk.sendInt(exampleInt);
    }

    static unsigned long lastFloatSendTime = 0;
    if (currentTime - lastFloatSendTime > 5000) { // 每5秒发送一次浮点数
        lastFloatSendTime = currentTime;
        float exampleFloat = -123.45;
        pyArduTalk.sendFloat(exampleFloat);
    }

    static unsigned long lastStrSendTime = 0;
    if (currentTime - lastStrSendTime > 7000) { // 每7秒发送一次字符串
        lastStrSendTime = currentTime;
        String exampleStr = "Hello from Arduino!";
        pyArduTalk.sendString(exampleStr);
    }

    static unsigned long lastJsonSendTime = 0;
    if (currentTime - lastJsonSendTime > 10000) { // 每10秒发送一次JSON数据
        lastJsonSendTime = currentTime;

        // 构建JSON对象
        StaticJsonDocument<256> doc;
        doc["sensor"] = "temperature";
        doc["value"] = 23.5;
        pyArduTalk.sendJson(doc);
    }
}
