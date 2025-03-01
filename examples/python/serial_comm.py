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
        while self.ser.in_waiting:
            echo = self.ser.read_until(bytes([self.FRAME_FOOTER]))
            if echo:
                echo = echo
                print("接收到回显:", ' '.join(f'{b:02X}' for b in echo))
                parsed = self.parse_frame(echo)
                if parsed:
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
                        print("接收到未知类型的数据")

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
