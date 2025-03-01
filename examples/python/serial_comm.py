import serial
import time
import struct
import json

class SerialComm:
    # 数据类型常量
    TYPE_INT = 0x01
    TYPE_FLOAT = 0x02
    TYPE_STRING = 0x03
    TYPE_JSON = 0x04  # 新增 JSON 类型

    FRAME_HEADER = 0xAA
    FRAME_FOOTER = 0x55

    def __init__(self, port, baudrate=115200, timeout=1):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(2)  # 等待串口稳定

    def calculate_crc16(self, data):
        crc = 0xFFFF
        for byte in data:
            crc ^= (byte << 8)
            for _ in range(8):
                if crc & 0x8000:
                    crc = ((crc << 1) ^ 0x1021) & 0xFFFF
                else:
                    crc = (crc << 1) & 0xFFFF
        return crc

    def build_frame(self, data_type, data_bytes):
        header = self.FRAME_HEADER
        footer = self.FRAME_FOOTER
        length = 1 + len(data_bytes)  # 类型 + 数据长度
        crc_input = [data_type] + list(data_bytes)
        crc = self.calculate_crc16(crc_input)
        crc_high = (crc >> 8) & 0xFF
        crc_low = crc & 0xFF
        frame = bytes([header, length, data_type] + list(data_bytes) + [crc_high, crc_low, footer])
        return frame

    def parse_frame(self, frame):
        if len(frame) < 6:
            print("帧长度不足")
            return None
        if frame[0] != self.FRAME_HEADER or frame[-1] != self.FRAME_FOOTER:
            print("帧头或帧尾错误")
            return None
        length = frame[1]
        data_type = frame[2]
        data = frame[3:3 + length - 1]
        if len(frame) < 3 + length -1 + 2 +1:
            print("帧长度与长度字节不符")
            return None
        crc_received = (frame[3 + length -1] << 8) | frame[4 + length -1]
        crc_calculated = self.calculate_crc16(frame[2:3 + length -1])
        if crc_received != crc_calculated:
            print(f"CRC校验失败: 接收 {crc_received:04X}, 计算 {crc_calculated:04X}")
            return None
        return {
            'type': data_type,
            'data': data
        }

    def send_command(self, data_type, data_bytes):
        frame = self.build_frame(data_type, data_bytes)
        self.ser.write(frame)
        print(f"发送帧: {' '.join(f'{b:02X}' for b in frame)}")

    def read_echo(self):
        # 维护一个内部缓冲区，保存所有读取的数据
        if not hasattr(self, 'buffer'):
            self.buffer = bytearray()
        
        # 读取所有可用数据到缓冲区
        while self.ser.in_waiting > 0:
            self.buffer.extend(self.ser.read(self.ser.in_waiting))
        
        # 在缓冲区中寻找并处理所有完整帧
        while len(self.buffer) > 0:
            # 寻找帧头
            header_index = self.buffer.find(bytes([self.FRAME_HEADER]))
            if header_index == -1:
                # 没有找到帧头，清空缓冲区
                self.buffer.clear()
                break
                
            # 如果帧头不在开始位置，丢弃前面的数据
            if header_index > 0:
                print(f"丢弃无效数据: {' '.join(f'{b:02X}' for b in self.buffer[:header_index])}")
                self.buffer = self.buffer[header_index:]
                
            # 检查是否至少有帧头和长度字节
            if len(self.buffer) < 2:
                # 数据不足，等待下次处理
                break
                
            # 获取length字段 - 这是类型(1字节)和数据的长度
            length = self.buffer[1]
            
            # 计算完整帧所需的字节数: 帧头(1) + 长度(1) + 类型和数据(length) + CRC(2) + 帧尾(1)
            total_frame_size = 1 + 1 + length + 2 + 1
            
            # 确保缓冲区中有完整的帧
            if len(self.buffer) < total_frame_size:
                # 数据不足，等待更多数据
                print(f"帧不完整，等待更多数据，当前: {len(self.buffer)}/{total_frame_size} 字节")
                break
                
            # 检查帧尾
            if self.buffer[total_frame_size - 1] != self.FRAME_FOOTER:
                # 帧尾错误，丢弃一个字节并继续寻找
                print(f"帧尾错误 {self.buffer[total_frame_size - 1]:02X} != {self.FRAME_FOOTER:02X}，丢弃一字节")
                self.buffer = self.buffer[1:]
                continue
                
            # 提取完整的帧
            frame = bytes(self.buffer[:total_frame_size])
            # 从缓冲区中移除已处理的帧
            self.buffer = self.buffer[total_frame_size:]
            
            # 解析帧
            print(f"尝试解析帧: {' '.join(f'{b:02X}' for b in frame)}")
            parsed = self.parse_frame(frame)
            if parsed:
                # 根据数据类型处理解析结果
                if parsed['type'] == self.TYPE_INT:
                    received_int = int.from_bytes(parsed['data'], byteorder='big', signed=True)
                    print(f"解析接收到的整数: {received_int}")
                elif parsed['type'] == self.TYPE_FLOAT:
                    received_float = struct.unpack('>f', parsed['data'])[0]
                    print(f"解析接收到的浮点数: {received_float}")
                elif parsed['type'] == self.TYPE_STRING:
                    received_str = parsed['data'].decode('utf-8')
                    print(f"解析接收到的字符串: {received_str}")
                elif parsed['type'] == self.TYPE_JSON:
                    received_json_str = parsed['data'].decode('utf-8')
                    try:
                        received_json = json.loads(received_json_str)
                        print(f"解析接收到的JSON: {received_json}")
                    except json.JSONDecodeError as e:
                        print(f"JSON解析错误: {e}")
                else:
                    print(f"接收到未知类型的数据 (类型: {parsed['type']})")
            else:
                print(f"帧解析失败，继续寻找下一帧")

    def send_int(self, int_value):
        data_bytes = int_value.to_bytes(2, byteorder='big', signed=True)
        self.send_command(self.TYPE_INT, data_bytes)

    def send_float(self, float_value):
        data_bytes = struct.pack('>f', float_value)  # 大端
        self.send_command(self.TYPE_FLOAT, data_bytes)

    def send_string(self, string_value):
        data_bytes = string_value.encode('utf-8')
        self.send_command(self.TYPE_STRING, data_bytes)

    def send_json(self, json_dict):
        json_str = json.dumps(json_dict)
        data_bytes = json_str.encode('utf-8')
        self.send_command(self.TYPE_JSON, data_bytes)

    def close(self):
        self.ser.close()
