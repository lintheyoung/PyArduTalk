#include "PyArduTalk.h"

// 构造函数
PyArduTalk::PyArduTalk(HardwareSerial& serialPort)
    : Serial_sw(serialPort), currentState(WAIT_HEADER), dataLength(0), originalLength(0),
      dataType(0), crcIndex(0), dataIndex(0), crcReceived(0), crcCalculated(0),
      lastStateChangeTime(0),
      intCallback(nullptr), floatCallback(nullptr), stringCallback(nullptr), jsonCallback(nullptr),
      echoCallback(nullptr) {
    // 初始化其他成员变量
}

void PyArduTalk::begin() {
    // 假设 Serial_sw 已在外部初始化
    // 可以添加其他初始化代码（如果有）
}

void PyArduTalk::loop() {
    // 检查超时
    checkTimeout();
    
    // 读取可用数据
    while (Serial_sw.available()) {
        byte incomingByte = Serial_sw.read();
        receiveData(incomingByte);
    }
}

uint16_t PyArduTalk::calculateCRC16(const byte *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

bool PyArduTalk::checkTimeout() {
    if (currentState != WAIT_HEADER && millis() - lastStateChangeTime > FRAME_TIMEOUT) {
        Serial.println(F("Frame reception timeout, resetting state machine"));
        resetStateMachine();
        return true;
    }
    return false;
}

void PyArduTalk::resetStateMachine() {
    currentState = WAIT_HEADER;
    dataLength = 0;
    originalLength = 0;
    dataIndex = 0;
    crcIndex = 0;
}

void PyArduTalk::receiveData(byte incomingByte) {
    // 记录状态改变时间
    State previousState = currentState;
    
    switch (currentState) {
        case WAIT_HEADER:
            if (incomingByte == FRAME_HEADER) {
                currentState = READ_LENGTH;
            }
            break;
        case READ_LENGTH:
            dataLength = incomingByte;
            originalLength = incomingByte;
            if (dataLength > 0 && dataLength <= sizeof(dataBuffer) + 1) {
                crcIndex = 0;
                dataIndex = 0;
                memset(crcBuffer, 0, sizeof(crcBuffer));
                memset(dataBuffer, 0, sizeof(dataBuffer));
                currentState = READ_TYPE;
            } else {
                resetStateMachine(); // 使用新的重置函数
            }
            break;
        case READ_TYPE:
            dataType = incomingByte;
            crcBuffer[crcIndex++] = dataType;
            dataLength -= 1;
            currentState = READ_DATA;
            break;
        case READ_DATA:
            if (dataIndex < sizeof(dataBuffer)) { // 添加安全检查
                dataBuffer[dataIndex++] = incomingByte;
                if (crcIndex < sizeof(crcBuffer)) { // 添加安全检查
                    crcBuffer[crcIndex++] = incomingByte;
                }
                dataLength -= 1;
                if (dataLength == 0) {
                    currentState = READ_CRC_HIGH;
                }
            } else {
                // 缓冲区溢出保护
                resetStateMachine();
            }
            break;
        case READ_CRC_HIGH:
            crcReceived = incomingByte << 8;
            currentState = READ_CRC_LOW;
            break;
        case READ_CRC_LOW:
            crcReceived |= incomingByte;
            crcCalculated = calculateCRC16(crcBuffer, originalLength);
            if (crcReceived == crcCalculated) {
                currentState = WAIT_FOOTER;
            } else {
                resetStateMachine();
            }
            break;
        case WAIT_FOOTER:
            if (incomingByte == FRAME_FOOTER) {
                processFrame();
            }
            resetStateMachine(); // 无论如何都重置状态机，准备下一帧
            break;
    }
    
    // 如果状态改变，更新时间戳
    if (currentState != previousState) {
        lastStateChangeTime = millis();
    }
}

void PyArduTalk::processFrame() {
    switch (dataType) {
        case TYPE_INT:
            if ((originalLength - 1) == 2) { // 2 字节表示一个 int16_t
                int16_t receivedInt = (dataBuffer[0] << 8) | dataBuffer[1];
                // 调用回调函数
                if (intCallback) {
                    intCallback(receivedInt);
                }
            }
            break;

        case TYPE_FLOAT:
            if ((originalLength - 1) == 4) { // 4 字节表示一个 float
                float receivedFloat = bigEndianToFloat(dataBuffer);
                // 调用回调函数
                if (floatCallback) {
                    floatCallback(receivedFloat);
                }
            }
            break;

        case TYPE_STRING:
            {
                String receivedStr = "";
                for (int i = 0; i < (originalLength - 1); i++) {
                    receivedStr += (char)dataBuffer[i];
                }
                // 调用回调函数
                if (stringCallback) {
                    stringCallback(receivedStr);
                }
            }
            break;

        case TYPE_JSON:
            {
                String jsonStr = "";
                for (int i = 0; i < (originalLength - 1); i++) {
                    jsonStr += (char)dataBuffer[i];
                }

                // 解析JSON
                StaticJsonDocument<512> doc; // 根据JSON大小调整
                DeserializationError error = deserializeJson(doc, jsonStr);
                if (!error) {
                    // 调用回调函数
                    if (jsonCallback) {
                        jsonCallback(doc);
                    }
                }
                // 可选：如果需要处理特定的JSON内容，可以通过回调函数在外部实现
            }
            break;

        // 处理更多类型
        default:
            // 可选：添加一个通用的回调函数用于处理未知类型的数据
            break;
    }

    // 回显接收到的帧
    echoFrame();
}

void PyArduTalk::echoFrame() {
    byte frame[1 + 1 + 1 + 256 + 2 + 1]; // 最大帧大小
    int frameIndex = 0;

    // 帧头
    frame[frameIndex++] = FRAME_HEADER;

    // 使用 originalLength 来设置长度
    frame[frameIndex++] = originalLength;

    // 类型
    frame[frameIndex++] = dataType;

    // 数据
    memcpy(&frame[frameIndex], dataBuffer, originalLength - 1); // originalLength 包含类型字节
    frameIndex += (originalLength - 1);

    // CRC
    crcCalculated = calculateCRC16(crcBuffer, originalLength); // 类型 + 数据
    frame[frameIndex++] = highByte(crcCalculated);
    frame[frameIndex++] = lowByte(crcCalculated);

    // 帧尾
    frame[frameIndex++] = FRAME_FOOTER;

    // 调用回调函数（如果设置了）
    if (echoCallback) {
        echoCallback(frame, frameIndex);
    }
}

void PyArduTalk::sendInt(int16_t value) {
    byte intBytes[2];
    intBytes[0] = (value >> 8) & 0xFF;
    intBytes[1] = value & 0xFF;

    byte length = 1 + 2; // 类型 + 数据
    byte frame[1 + 1 + 1 + 2 + 2 + 1];
    int idx = 0;
    frame[idx++] = FRAME_HEADER;
    frame[idx++] = length;
    frame[idx++] = TYPE_INT;
    memcpy(&frame[idx], intBytes, 2);
    idx += 2;

    byte crc_input_int[3] = {TYPE_INT, intBytes[0], intBytes[1]};
    uint16_t crc = calculateCRC16(crc_input_int, 3);
    frame[idx++] = highByte(crc);
    frame[idx++] = lowByte(crc);

    frame[idx++] = FRAME_FOOTER;

    Serial_sw.write(frame, idx);
}

void PyArduTalk::sendFloat(float value) {
    byte floatBytes_bigEndian[4];
    floatToBigEndian(value, floatBytes_bigEndian);
    byte length = 1 + 4; // 类型 + 数据
    byte frame[1 + 1 + 1 + 4 + 2 + 1];
    int idx = 0;
    frame[idx++] = FRAME_HEADER;
    frame[idx++] = length;
    frame[idx++] = TYPE_FLOAT;
    memcpy(&frame[idx], floatBytes_bigEndian, 4);
    idx += 4;

    byte crc_input_float[5] = {TYPE_FLOAT, floatBytes_bigEndian[0], floatBytes_bigEndian[1], floatBytes_bigEndian[2], floatBytes_bigEndian[3]};
    uint16_t crc = calculateCRC16(crc_input_float, 5);
    frame[idx++] = highByte(crc);
    frame[idx++] = lowByte(crc);

    frame[idx++] = FRAME_FOOTER;

    Serial_sw.write(frame, idx);
}

void PyArduTalk::sendString(const String& value) {
    byte strBytes[256];
    int strLength = value.length();
    value.getBytes(strBytes, sizeof(strBytes), 0);

    byte length = 1 + strLength; // 类型 + 数据
    byte frame[1 + 1 + 1 + 256 + 2 + 1];
    int idx = 0;
    frame[idx++] = FRAME_HEADER;
    frame[idx++] = length;
    frame[idx++] = TYPE_STRING;
    memcpy(&frame[idx], strBytes, strLength);
    idx += strLength;

    byte crc_input_str[1 + 256];
    crc_input_str[0] = TYPE_STRING;
    memcpy(&crc_input_str[1], strBytes, strLength);
    uint16_t crc_str = calculateCRC16(crc_input_str, 1 + strLength);
    frame[idx++] = highByte(crc_str);
    frame[idx++] = lowByte(crc_str);

    frame[idx++] = FRAME_FOOTER;

    Serial_sw.write(frame, idx);
}

void PyArduTalk::sendJson(const StaticJsonDocument<256>& doc) {
    String jsonStr;
    serializeJson(doc, jsonStr);

    byte jsonBytes[256];
    int jsonLength = jsonStr.length();
    jsonStr.getBytes(jsonBytes, sizeof(jsonBytes), 0);

    byte jsonDataType = TYPE_JSON;
    byte jsonDataLength = 1 + jsonLength; // 类型 + 数据
    byte jsonFrame[1 + 1 + 1 + 256 + 2 + 1];
    int jsonIdx = 0;
    jsonFrame[jsonIdx++] = FRAME_HEADER;
    jsonFrame[jsonIdx++] = jsonDataLength;
    jsonFrame[jsonIdx++] = jsonDataType;
    memcpy(&jsonFrame[jsonIdx], jsonBytes, jsonLength);
    jsonIdx += jsonLength;

    // 计算 CRC
    byte crc_input_json[1 + 256];
    crc_input_json[0] = jsonDataType;
    memcpy(&crc_input_json[1], jsonBytes, jsonLength);
    uint16_t crc_json = calculateCRC16(crc_input_json, 1 + jsonLength);
    jsonFrame[jsonIdx++] = highByte(crc_json);
    jsonFrame[jsonIdx++] = lowByte(crc_json);

    // 帧尾
    jsonFrame[jsonIdx++] = FRAME_FOOTER;

    // 发送帧
    Serial_sw.write(jsonFrame, jsonIdx);
}

void PyArduTalk::floatToBigEndian(float value, byte *buffer) {
    byte *floatPtr = (byte*)&value;
    buffer[0] = floatPtr[3];
    buffer[1] = floatPtr[2];
    buffer[2] = floatPtr[1];
    buffer[3] = floatPtr[0];
}

float PyArduTalk::bigEndianToFloat(byte *buffer) {
    byte reversed[4] = {buffer[3], buffer[2], buffer[1], buffer[0]};
    float value;
    memcpy(&value, reversed, 4);
    return value;
}

// 设置回调函数的方法实现
void PyArduTalk::onIntReceived(IntCallback callback) {
    intCallback = callback;
}

void PyArduTalk::onFloatReceived(FloatCallback callback) {
    floatCallback = callback;
}

void PyArduTalk::onStringReceived(StringCallback callback) {
    stringCallback = callback;
}

void PyArduTalk::onJsonReceived(JsonCallback callback) {
    jsonCallback = callback;
}

void PyArduTalk::onEchoFrame(EchoCallback callback) { // （可选）
    echoCallback = callback;
}
