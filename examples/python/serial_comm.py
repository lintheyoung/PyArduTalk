import serial
import time
import struct
import json

class SerialComm:
    # 数据类型常量
    TYPE_INT = 0x01
    TYPE_FLOAT = 0x02
    TYPE_STRING = 0x03
    TYPE_JSON = 0x04  # JSON 类型
    TYPE_REQUEST = 0x05  # 新增请求类型

    FRAME_HEADER = 0xAA
    FRAME_FOOTER = 0x55

    def __init__(self, port, baudrate=115200, timeout=1):
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(2)  # 等待串口稳定
        self.buffer = bytearray()  # 初始化缓冲区

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
        # 基本格式检查
        if len(frame) < 6:  # 最小有效帧大小
            print("帧长度不足")
            return None
        
        if frame[0] != self.FRAME_HEADER or frame[-1] != self.FRAME_FOOTER:
            print("帧头或帧尾错误")
            return None
        
        length = frame[1]
        expected_frame_size = 1 + 1 + length + 2 + 1  # 帧头+长度+数据+CRC+帧尾
        
        if len(frame) != expected_frame_size:
            print(f"帧长度不匹配: 预期 {expected_frame_size}，实际 {len(frame)}")
            return None
        
        data_type = frame[2]
        data = frame[3:3 + length - 1]  # 减1是因为length包含了类型字节
        
        # CRC校验
        crc_high = frame[-3]
        crc_low = frame[-2]
        crc_received = (crc_high << 8) | crc_low
        
        # 计算CRC
        crc_data = frame[2:3 + length - 1]  # 类型 + 数据
        crc_calculated = self.calculate_crc16(crc_data)
        
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
        # 读取所有可用数据到缓冲区
        while self.ser.in_waiting > 0:
            self.buffer.extend(self.ser.read(self.ser.in_waiting))
        
        # 使用滑动窗口算法查找并处理所有可能的帧
        results = []
        processed_index = 0
        buffer_length = len(self.buffer)
        
        while processed_index < buffer_length:
            # 寻找帧头，从当前处理位置开始
            header_index = self.buffer.find(bytes([self.FRAME_HEADER]), processed_index)
            
            # 没有找到帧头，保留最后一部分数据（可能是不完整的帧起始）
            if header_index == -1:
                if buffer_length > 16:  # 保留最后16字节以防不完整帧头
                    self.buffer = self.buffer[-16:]
                break
            
            # 如果在处理位置之后找到了帧头
            if header_index > processed_index:
                print(f"跳过无效数据: {' '.join(f'{b:02X}' for b in self.buffer[processed_index:header_index])}")
                processed_index = header_index
            
            # 检查缓冲区中是否至少有帧头和长度字节
            if header_index + 1 >= buffer_length:
                # 不够完整的帧，保留这部分等待更多数据
                self.buffer = self.buffer[header_index:]
                break
            
            length = self.buffer[header_index + 1]
            
            # 检查长度是否合理 (增加健壮性检查)
            if length < 2 or length > 200:  # 设置一个合理的最大长度
                print(f"检测到无效长度 {length}，在位置 {header_index}，跳过此帧头")
                processed_index = header_index + 1  # 跳过此帧头，继续查找
                continue
            
            # 计算所需的总帧长度: 帧头(1) + 长度(1) + 类型和数据(length) + CRC(2) + 帧尾(1)
            total_frame_size = 1 + 1 + length + 2 + 1
            
            # 检查是否有足够的数据形成完整帧
            if header_index + total_frame_size > buffer_length:
                # 缓冲区中数据不足，保留此部分等待更多数据
                self.buffer = self.buffer[header_index:]
                break
            
            # 验证帧尾
            expected_footer_index = header_index + total_frame_size - 1
            if self.buffer[expected_footer_index] != self.FRAME_FOOTER:
                print(f"帧尾错误: 期望 {self.FRAME_FOOTER:02X}，实际 {self.buffer[expected_footer_index]:02X}")
                
                # 智能重同步: 在当前帧范围内查找下一个可能的帧头
                next_header_index = self.buffer.find(bytes([self.FRAME_HEADER]), header_index + 1, header_index + total_frame_size)
                if next_header_index != -1:
                    # 在当前帧内找到另一个帧头，从那里继续
                    print(f"在当前帧内找到新帧头，位置: {next_header_index - header_index} 字节处")
                    processed_index = next_header_index
                else:
                    # 否则跳过当前帧头，继续搜索
                    processed_index = header_index + 1
                continue
            
            # 提取帧并解析
            frame = bytes(self.buffer[header_index:header_index + total_frame_size])
            print(f"尝试解析帧: {' '.join(f'{b:02X}' for b in frame)}")
            
            parsed = self.parse_frame(frame)
            if parsed:
                # 解析成功，处理数据
                data_type = parsed['type']
                data = parsed['data']
                
                result = None
                if data_type == self.TYPE_INT:
                    result = int.from_bytes(data, byteorder='big', signed=True)
                    print(f"解析接收到的整数: {result}")
                elif data_type == self.TYPE_FLOAT:
                    result = struct.unpack('>f', data)[0]
                    print(f"解析接收到的浮点数: {result}")
                elif data_type == self.TYPE_STRING:
                    result = data.decode('utf-8')
                    print(f"解析接收到的字符串: {result}")
                elif data_type == self.TYPE_JSON:
                    try:
                        result = data.decode('utf-8')
                        result = json.loads(result)
                        print(f"解析接收到的JSON: {result}")
                    except json.JSONDecodeError as e:
                        print(f"JSON解析错误: {e}")
                else:
                    print(f"接收到未知类型的数据 (类型: {data_type})")
                
                if result is not None:
                    results.append(result)
                
                # 继续处理下一帧
                processed_index = header_index + total_frame_size
            else:
                # 帧解析失败(如CRC错误)，使用滑动窗口寻找新帧头
                next_header_index = self.buffer.find(bytes([self.FRAME_HEADER]), header_index + 1, header_index + total_frame_size)
                if next_header_index != -1:
                    # 找到新的帧头
                    print(f"CRC错误后找到新帧头，位置: {next_header_index}")
                    processed_index = next_header_index
                else:
                    # 没找到，跳过当前帧继续处理
                    processed_index = header_index + 1
        
        # 更新缓冲区，移除已处理部分
        if processed_index > 0:
            self.buffer = self.buffer[processed_index:]
        
        # 返回处理结果
        if results:
            return results[-1]  # 返回最后一个有效结果，保持与原函数相同的返回类型
        return None

    def request_float(self):
        """发送请求获取浮点数的命令"""
        print("请求浮点数数据...")
        # 创建请求浮点数据的数据包，使用TYPE_FLOAT作为请求的数据类型
        data_bytes = bytes([self.TYPE_FLOAT])
        self.send_command(self.TYPE_REQUEST, data_bytes)
        
        # 等待响应并返回
        return self.read_response()
    
    def request_int(self):
        """发送请求获取整数的命令"""
        print("请求整数数据...")
        data_bytes = bytes([self.TYPE_INT])
        self.send_command(self.TYPE_REQUEST, data_bytes)
        return self.read_response()

    def request_string(self):
        """发送请求获取字符串的命令"""
        print("请求字符串数据...")
        data_bytes = bytes([self.TYPE_STRING])
        self.send_command(self.TYPE_REQUEST, data_bytes)
        return self.read_response()

    def request_json(self):
        """发送请求获取JSON的命令"""
        print("请求JSON数据...")
        data_bytes = bytes([self.TYPE_JSON])
        self.send_command(self.TYPE_REQUEST, data_bytes)
        return self.read_response()

    def read_response(self, timeout=2.0):
        """读取响应数据，带超时机制"""
        start_time = time.time()
        while (time.time() - start_time) < timeout:
            response = self.read_echo()
            if response is not None:
                return response
            time.sleep(0.1)  # 小延迟，避免过度消耗CPU
        
        print("等待响应超时")
        return None

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