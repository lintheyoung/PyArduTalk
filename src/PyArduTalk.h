#ifndef PYARDUTALK_H
#define PYARDUTALK_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>

class PyArduTalk {
public:
    // 数据类型常量
    enum DataType {
        TYPE_INT = 0x01,
        TYPE_FLOAT = 0x02,
        TYPE_STRING = 0x03,
        TYPE_JSON = 0x04,
        TYPE_REQUEST = 0x05,
        TYPE_GYRO = 0x06,  // 新增陀螺仪数据类型
        // 可以添加更多类型
    };

    // 状态机枚举
    enum State {
        WAIT_HEADER,
        READ_LENGTH,
        READ_TYPE,
        READ_DATA,
        READ_CRC_HIGH,
        READ_CRC_LOW,
        WAIT_FOOTER
    };

    // 回调函数类型定义
    typedef void (*IntCallback)(int16_t);
    typedef void (*FloatCallback)(float);
    typedef void (*StringCallback)(const String&);
    typedef void (*JsonCallback)(const StaticJsonDocument<256>&);
    typedef void (*EchoCallback)(const byte* frame, size_t length); // （可选）
    typedef void (*RequestCallback)(byte);

    // 添加处理陀螺仪数据的回调函数类型定义
    typedef void (*GyroCallback)(float yaw, float roll, float pitch);

    // 构造函数
    PyArduTalk(HardwareSerial& serialPort);

    // 初始化方法
    void begin();

    // 循环处理方法
    void loop();

    // 发送方法
    void sendInt(int16_t value);
    void sendFloat(float value);
    void sendString(const String& value);
    void sendJson(const StaticJsonDocument<256>& doc);
    // 可以添加更多发送方法

    // 设置回调函数的方法
    void onIntReceived(IntCallback callback);
    void onFloatReceived(FloatCallback callback);
    void onStringReceived(StringCallback callback);
    void onJsonReceived(JsonCallback callback);
    void onEchoFrame(EchoCallback callback); // （可选）
    // 在public部分添加设置请求回调的方法
    void onRequestReceived(RequestCallback callback);

    // 发送陀螺仪数据
    void sendGyro(float yaw, float roll, float pitch);
    // 设置接收陀螺仪数据的回调
    void onGyroReceived(GyroCallback callback);

private:
    HardwareSerial& Serial_sw;
    State currentState;
    byte dataLength;
    byte originalLength;
    byte dataType;
    byte dataBuffer[256];
    byte crcBuffer[256 + 1];
    int crcIndex;
    int dataIndex;
    uint16_t crcReceived;
    uint16_t crcCalculated;

    RequestCallback requestCallback;

    GyroCallback gyroCallback;

    // 回调函数指针
    IntCallback intCallback;
    FloatCallback floatCallback;
    StringCallback stringCallback;
    JsonCallback jsonCallback;
    EchoCallback echoCallback; // （可选）

    const byte FRAME_HEADER = 0xAA;
    const byte FRAME_FOOTER = 0x55;

    // 私有方法
    uint16_t calculateCRC16(const byte *data, size_t length);
    void processFrame();
    void echoFrame();
    void receiveData(byte incomingByte);
    void floatToBigEndian(float value, byte *buffer);
    float bigEndianToFloat(byte *buffer);

    // 超时处理变量
    unsigned long lastStateChangeTime;
    const unsigned long FRAME_TIMEOUT = 500; // 500毫秒超时
    
    // 重置状态机
    void resetStateMachine();
    bool checkTimeout();
    
    // 添加用于滑动窗口搜索的缓冲区
    static const int SYNC_BUFFER_SIZE = 32;  // 滑动窗口缓冲区大小
    byte syncBuffer[SYNC_BUFFER_SIZE];       // 滑动窗口缓冲区
    int syncBufferIndex;                     // 当前写入位置
    int syncBufferLength;                    // 缓冲区中有效数据长度
    
    // 智能重同步方法
    bool attemptResync();
    bool findFrameHeader(int startPos);
    void addToSyncBuffer(byte value);
};

#endif