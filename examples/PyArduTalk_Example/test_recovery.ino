#include <PyArduTalk.h>
#include <ArduinoJson.h>

// 创建 PyArduTalk 对象，使用 UART1 (GPIO 20 TX, GPIO 21 RX)
PyArduTalk pyArduTalk(Serial1);

// 接收计数器，用于验证恢复能力
int intReceivedCount = 0;
int floatReceivedCount = 0;
int stringReceivedCount = 0;
int jsonReceivedCount = 0;

// 回调函数定义
void handleInt(int16_t value) {
    Serial.print("回调 - 接收到整数: ");
    Serial.println(value);
    intReceivedCount++;
    
    // 输出统计信息
    printStats();
}

void handleFloat(float value) {
    Serial.print("回调 - 接收到浮点数: ");
    Serial.println(value);
    floatReceivedCount++;
    
    // 输出统计信息
    printStats();
}

void handleString(const String& value) {
    Serial.print("回调 - 接收到字符串: ");
    Serial.println(value);
    stringReceivedCount++;
    
    // 输出统计信息
    printStats();
}

void handleJson(const StaticJsonDocument<256>& doc) {
    Serial.print("回调 - 接收到JSON数据: ");
    serializeJson(doc, Serial);
    Serial.println();
    jsonReceivedCount++;
    
    // 输出统计信息
    printStats();
}

// 输出统计信息
void printStats() {
    Serial.println("\n--- 接收统计 ---");
    Serial.print("整数帧: ");
    Serial.println(intReceivedCount);
    Serial.print("浮点数帧: ");
    Serial.println(floatReceivedCount);
    Serial.print("字符串帧: ");
    Serial.println(stringReceivedCount);
    Serial.print("JSON帧: ");
    Serial.println(jsonReceivedCount);
    Serial.println("---------------\n");
}

// 回调函数用于处理回显帧
void handleEchoFrame(const byte* frame, size_t length) {
    Serial.print("回显帧: ");
    for (size_t i = 0; i < length; i++) {
        Serial.printf("%02X ", frame[i]);
    }
    Serial.println();
}

void setup() {
    // 初始化串口用于调试
    Serial.begin(115200);
    while (!Serial) {
        ; // 等待 Serial 连接
    }
    
    delay(1000); // 确保 Serial 开启
    Serial.println("\n\n===== PyArduTalk 帧恢复测试 =====\n");

    // 初始化 Serial1（UART1）
    Serial1.begin(115200, SERIAL_8N1, 21, 20);

    // 初始化 PyArduTalk
    pyArduTalk.begin();

    // 设置回调函数
    pyArduTalk.onIntReceived(handleInt);
    pyArduTalk.onFloatReceived(handleFloat);
    pyArduTalk.onStringReceived(handleString);
    pyArduTalk.onJsonReceived(handleJson);
    pyArduTalk.onEchoFrame(handleEchoFrame);
    
    Serial.println("Arduino准备完毕，等待测试数据...");
}

void loop() {
    // 处理接收的数据
    pyArduTalk.loop();
    
    // 周期性发送数据，便于测试双向通信
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 10000) { // 每10秒发送一次
        lastSendTime = millis();
        
        Serial.println("\n发送测试数据...");
        
        // 发送整数
        pyArduTalk.sendInt(42);
        
        // 发送浮点数
        pyArduTalk.sendFloat(3.14159);
        
        // 发送字符串
        pyArduTalk.sendString("Arduino Test");
        
        // 发送JSON
        StaticJsonDocument<256> doc;
        doc["device"] = "Arduino";
        doc["uptime"] = millis() / 1000;
        doc["received"] = intReceivedCount + floatReceivedCount + 
                          stringReceivedCount + jsonReceivedCount;
        pyArduTalk.sendJson(doc);
    }
}