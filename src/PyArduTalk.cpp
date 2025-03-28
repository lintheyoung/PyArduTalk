#include "PyArduTalk.h"

// 在PyArduTalk构造函数的初始化列表中添加
PyArduTalk::PyArduTalk(HardwareSerial& serialPort)
    : Serial_sw(serialPort), currentState(WAIT_HEADER), dataLength(0), originalLength(0),
      dataType(0), crcIndex(0), dataIndex(0), crcReceived(0), crcCalculated(0),
      lastStateChangeTime(0), syncBufferIndex(0), syncBufferLength(0),
      intCallback(nullptr), floatCallback(nullptr), stringCallback(nullptr), jsonCallback(nullptr),
      requestCallback(nullptr), echoCallback(nullptr), gyroCallback(nullptr) {  // 添加 gyroCallback 初始化
    // 初始化其他成员变量
    memset(syncBuffer, 0, SYNC_BUFFER_SIZE);
}

void PyArduTalk::sendGyro(float yaw, float roll, float pitch) {
    // 将浮点数乘以100并转换为int16_t，以保留两位小数
    int16_t yawInt = (int16_t)(yaw * 100);
    int16_t rollInt = (int16_t)(roll * 100);
    int16_t pitchInt = (int16_t)(pitch * 100);
    
    // 将三个int16_t值打包成字节数组
    byte gyroBytes[6]; // 3个int16_t，每个占2字节
    
    // 按大端序转换为字节数组
    gyroBytes[0] = (yawInt >> 8) & 0xFF;
    gyroBytes[1] = yawInt & 0xFF;
    gyroBytes[2] = (rollInt >> 8) & 0xFF;
    gyroBytes[3] = rollInt & 0xFF;
    gyroBytes[4] = (pitchInt >> 8) & 0xFF;
    gyroBytes[5] = pitchInt & 0xFF;
    
    // 计算数据包长度：类型(1字节) + 数据(6字节)
    byte length = 1 + 6;
    
    // 构建完整帧
    byte frame[1 + 1 + 1 + 6 + 2 + 1]; // 帧头+长度+类型+数据+CRC(2字节)+帧尾
    int idx = 0;
    
    frame[idx++] = FRAME_HEADER;
    frame[idx++] = length;
    frame[idx++] = TYPE_GYRO;
    
    // 复制陀螺仪数据
    memcpy(&frame[idx], gyroBytes, 6);
    idx += 6;
    
    // 计算CRC（包括类型字节和数据）
    byte crc_input[7]; // 类型(1字节) + 数据(6字节)
    crc_input[0] = TYPE_GYRO;
    memcpy(&crc_input[1], gyroBytes, 6);
    
    uint16_t crc = calculateCRC16(crc_input, 7);
    frame[idx++] = highByte(crc);
    frame[idx++] = lowByte(crc);
    
    frame[idx++] = FRAME_FOOTER;
    
    // 发送帧
    Serial_sw.write(frame, idx);
}

// 添加设置陀螺仪回调的方法实现
void PyArduTalk::onGyroReceived(GyroCallback callback) {
    gyroCallback = callback;
}

// 添加设置请求回调的方法实现
void PyArduTalk::onRequestReceived(RequestCallback callback) {
    requestCallback = callback;
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

// 将字节添加到同步缓冲区的辅助方法
void PyArduTalk::addToSyncBuffer(byte value) {
    syncBuffer[syncBufferIndex] = value;
    syncBufferIndex = (syncBufferIndex + 1) % SYNC_BUFFER_SIZE;
    
    if (syncBufferLength < SYNC_BUFFER_SIZE) {
        syncBufferLength++;
    }
}

// 在同步缓冲区中从指定位置寻找帧头
bool PyArduTalk::findFrameHeader(int startPos) {
    for (int i = 0; i < syncBufferLength - startPos; i++) {
        int pos = (startPos + i) % SYNC_BUFFER_SIZE;
        if (syncBuffer[pos] == FRAME_HEADER) {
            // 帧头需要至少跟随一个长度字节
            int lengthPos = (pos + 1) % SYNC_BUFFER_SIZE;
            if (i + 1 < syncBufferLength) {
                byte length = syncBuffer[lengthPos];
                // 验证长度是否合理
                if (length >= 2 && length <= 200) {  // 设置一个合理的最大值
                    return true;
                }
            }
        }
    }
    return false;
}

// 尝试重新同步通信
bool PyArduTalk::attemptResync() {
    Serial.println(F("尝试重新同步..."));
    
    // 在同步缓冲区中寻找有效帧头
    if (findFrameHeader(1)) {  // 从当前位置之后开始查找
        Serial.println(F("找到新的帧头，重新同步成功"));
        resetStateMachine();
        return true;
    }
    
    Serial.println(F("未找到新的帧头，重置状态机"));
    resetStateMachine();
    return false;
}

void PyArduTalk::receiveData(byte incomingByte) {
    // 记录状态改变时间
    State previousState = currentState;
    
    // 将每个接收到的字节添加到同步缓冲区
    addToSyncBuffer(incomingByte);
    
    // 对每个状态实现智能检测和处理
    switch (currentState) {
        case WAIT_HEADER:
            if (incomingByte == FRAME_HEADER) {
                currentState = READ_LENGTH;
                Serial.println(F("检测到帧头"));
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
                Serial.print(F("帧长度: "));
                Serial.println(dataLength);
            } else {
                Serial.print(F("无效帧长度: "));
                Serial.println(dataLength);
                attemptResync();
            }
            break;
            
        case READ_TYPE:
            // 随时检查是否收到新帧头
            if (incomingByte == FRAME_HEADER) {
                Serial.println(F("在READ_TYPE状态下收到帧头，重新同步"));
                currentState = READ_LENGTH;
                break;
            }
            
            dataType = incomingByte;
            crcBuffer[crcIndex++] = dataType;
            dataLength -= 1;
            currentState = READ_DATA;
            Serial.print(F("数据类型: 0x"));
            Serial.println(dataType, HEX);
            break;
            
        case READ_DATA:
            // 随时检查是否收到新帧头
            if (incomingByte == FRAME_HEADER && dataIndex == 0) {
                Serial.println(F("在READ_DATA开始时收到帧头，重新同步"));
                currentState = READ_LENGTH;
                break;
            }
            
            if (dataIndex < sizeof(dataBuffer)) {
                dataBuffer[dataIndex++] = incomingByte;
                if (crcIndex < sizeof(crcBuffer)) {
                    crcBuffer[crcIndex++] = incomingByte;
                }
                dataLength -= 1;
                
                if (dataLength == 0) {
                    currentState = READ_CRC_HIGH;
                }
            } else {
                Serial.println(F("数据缓冲区溢出"));
                attemptResync();
            }
            break;
            
        case READ_CRC_HIGH:
            // 随时检查是否收到新帧头
            if (incomingByte == FRAME_HEADER) {
                Serial.println(F("在READ_CRC_HIGH状态下收到帧头，重新同步"));
                currentState = READ_LENGTH;
                break;
            }
            
            crcReceived = incomingByte << 8;
            currentState = READ_CRC_LOW;
            break;
            
        case READ_CRC_LOW:
            // 随时检查是否收到新帧头
            if (incomingByte == FRAME_HEADER) {
                Serial.println(F("在READ_CRC_LOW状态下收到帧头，重新同步"));
                currentState = READ_LENGTH;
                break;
            }
            
            crcReceived |= incomingByte;
            crcCalculated = calculateCRC16(crcBuffer, originalLength);
            
            if (crcReceived == crcCalculated) {
                currentState = WAIT_FOOTER;
                Serial.println(F("CRC校验通过"));
            } else {
                Serial.print(F("CRC校验失败: 接收 0x"));
                Serial.print(crcReceived, HEX);
                Serial.print(F(", 计算 0x"));
                Serial.println(crcCalculated, HEX);
                attemptResync();
            }
            break;
            
        case WAIT_FOOTER:
            if (incomingByte == FRAME_FOOTER) {
                Serial.println(F("接收到完整帧"));
                processFrame();
            } else {
                Serial.print(F("帧尾错误: 0x"));
                Serial.println(incomingByte, HEX);
                attemptResync();
            }
            resetStateMachine();
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

        case TYPE_REQUEST:
            if ((originalLength - 1) == 1) { // 1字节表示请求的数据类型
                byte requestedType = dataBuffer[0];
                Serial.print(F("收到数据请求，类型: 0x"));
                Serial.println(requestedType, HEX);
                
                // 调用请求回调函数
                if (requestCallback) {
                    requestCallback(requestedType);
                }
            }
            break;

        case TYPE_GYRO:
            if ((originalLength - 1) == 6) { // 6字节表示三个int16_t
                // 从大端字节序转换为int16_t
                int16_t yawInt = (dataBuffer[0] << 8) | dataBuffer[1];
                int16_t rollInt = (dataBuffer[2] << 8) | dataBuffer[3];
                int16_t pitchInt = (dataBuffer[4] << 8) | dataBuffer[5];
                
                // 转换回浮点数
                float yaw = yawInt / 100.0f;
                float roll = rollInt / 100.0f;
                float pitch = pitchInt / 100.0f;
                
                // 调用回调函数
                if (gyroCallback) {
                    gyroCallback(yaw, roll, pitch);
                }
            }
            break;

        // 处理更多类型
        default:
            // 可选：添加一个通用的回调函数用于处理未知类型的数据
            break;
    }

    // 关键修改: 只对非请求类型的消息执行回显
    if (dataType != TYPE_REQUEST) {
        echoFrame();
    }
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

    // 输出调试信息
    Serial.print("发送回显帧: ");
    for (size_t i = 0; i < frameIndex; i++) {
        Serial.printf("%02X ", frame[i]);
    }
    Serial.println();

    // 显式发送回显数据
    Serial_sw.write(frame, frameIndex);
    Serial_sw.flush(); // 确保数据被发送出去

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