#include <PyArduTalk.h>
#include <ArduinoJson.h>

// 创建 PyArduTalk 对象，使用 UART1 (GPIO 20 TX, GPIO 21 RX)
PyArduTalk pyArduTalk(Serial1);

// 在回调函数定义部分添加
void handleRequest(byte requestType) {
    Serial.print("回调 - 收到数据请求，类型: 0x");
    Serial.println(requestType, HEX);
    
    // 根据请求类型返回相应数据
    switch (requestType) {
        case PyArduTalk::TYPE_INT:
            {
                int16_t value = 42; // 可替换为传感器实际读取值
                Serial.print("发送整数响应: ");
                Serial.println(value);
                pyArduTalk.sendInt(value);
            }
            break;
            
        case PyArduTalk::TYPE_FLOAT:
            {
                float value = 3.14159; // 可替换为传感器实际读取值
                Serial.print("发送浮点数响应: ");
                Serial.println(value);
                pyArduTalk.sendFloat(value);
            }
            break;
            
        case PyArduTalk::TYPE_STRING:
            {
                String value = "Response from ESP32";
                Serial.print("发送字符串响应: ");
                Serial.println(value);
                pyArduTalk.sendString(value);
            }
            break;
            
        case PyArduTalk::TYPE_JSON:
            {
                StaticJsonDocument<256> doc;
                doc["sensor"] = "temperature";
                doc["value"] = 25.6;
                doc["timestamp"] = millis();
                
                Serial.println("发送JSON响应");
                pyArduTalk.sendJson(doc);
            }
            break;
            
        default:
            Serial.println("未知的请求类型");
            break;
    }
}


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
    delay(1000); // 确保 Serial 开启

    // 初始化 Serial1（UART1）
    Serial1.begin(115200, SERIAL_8N1, 21, 20);

    // 初始化 PyArduTalk
    pyArduTalk.begin();

    // 设置回调函数
    pyArduTalk.onIntReceived(handleInt);
    pyArduTalk.onFloatReceived(handleFloat);
    pyArduTalk.onStringReceived(handleString);
    pyArduTalk.onJsonReceived(handleJson);

    pyArduTalk.onRequestReceived(handleRequest);

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
