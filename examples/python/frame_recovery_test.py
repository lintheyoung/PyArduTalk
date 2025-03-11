"""
PyArduTalk 帧恢复测试脚本
用于测试改进后的通信协议在各种故障情况下的恢复能力
"""

import time
import serial
import struct
import random
import os
from serial_comm import SerialComm

class CorruptionTest:
    def __init__(self, port, baudrate=115200):
        self.serial_comm = SerialComm(port, baudrate)
        print(f"初始化测试，连接到 {port} (波特率: {baudrate})")
        time.sleep(1)  # 等待连接稳定
        
    def test_basic_communication(self):
        """测试基本通信功能是否正常"""
        print("\n=== 测试 1: 基本通信功能 ===")
        
        # 发送整数
        value = 12345
        print(f"发送整数: {value}")
        self.serial_comm.send_int(value)
        time.sleep(0.5)
        
        # 发送浮点数
        value = 123.45
        print(f"发送浮点数: {value}")
        self.serial_comm.send_float(value)
        time.sleep(0.5)
        
        # 发送字符串
        value = "Hello, Test!"
        print(f"发送字符串: {value}")
        self.serial_comm.send_string(value)
        time.sleep(0.5)
        
        # 接收回显
        response = self.serial_comm.read_echo()
        print(f"收到回显: {response}\n")
        
    def test_corrupted_frame_header(self):
        """测试帧头损坏的情况"""
        print("\n=== 测试 2: 帧头损坏恢复 ===")
        
        # 构建一个正常的整数帧
        value = 9876
        normal_frame = self.serial_comm.build_frame(
            self.serial_comm.TYPE_INT, 
            value.to_bytes(2, byteorder='big', signed=True)
        )
        
        # 故意损坏帧头
        corrupted_frame = bytearray(normal_frame)
        corrupted_frame[0] = 0xFF  # 替换帧头
        
        print(f"发送损坏的帧头: {' '.join(f'{b:02X}' for b in corrupted_frame)}")
        self.serial_comm.ser.write(corrupted_frame)
        time.sleep(0.2)
        
        # 发送正常帧
        print(f"发送正常帧: {' '.join(f'{b:02X}' for b in normal_frame)}")
        self.serial_comm.ser.write(normal_frame)
        time.sleep(0.5)
        
        # 接收回显
        response = self.serial_comm.read_echo()
        print(f"收到回显: {response}")
        
        # 判断是否恢复
        if response == 9876:
            print("✓ 恢复成功 - 能够正确识别和处理后续的正确帧\n")
        else:
            print("✗ 恢复失败 - 无法正确处理后续帧\n")
    
    def test_corrupt_frame_footer(self):
        """测试帧尾损坏的情况"""
        print("\n=== 测试 3: 帧尾损坏恢复 ===")
        
        # 构建一个正常的字符串帧
        value = "Test123"
        normal_frame = self.serial_comm.build_frame(
            self.serial_comm.TYPE_STRING, 
            value.encode('utf-8')
        )
        
        # 故意损坏帧尾
        corrupted_frame = bytearray(normal_frame)
        corrupted_frame[-1] = 0xAA  # 替换帧尾
        
        print(f"发送损坏的帧尾: {' '.join(f'{b:02X}' for b in corrupted_frame)}")
        self.serial_comm.ser.write(corrupted_frame)
        time.sleep(0.2)
        
        # 发送正常帧
        print(f"发送正常帧: {' '.join(f'{b:02X}' for b in normal_frame)}")
        self.serial_comm.ser.write(normal_frame)
        time.sleep(0.5)
        
        # 接收回显
        response = self.serial_comm.read_echo()
        print(f"收到回显: {response}")
        
        # 判断是否恢复
        if response == "Test123":
            print("✓ 恢复成功 - 能够正确识别和处理后续的正确帧\n")
        else:
            print("✗ 恢复失败 - 无法正确处理后续帧\n")
            
    def test_corrupt_crc(self):
        """测试CRC损坏的情况"""
        print("\n=== 测试 4: CRC损坏恢复 ===")
        
        # 构建一个正常的整数帧
        value = 5555
        normal_frame = self.serial_comm.build_frame(
            self.serial_comm.TYPE_INT, 
            value.to_bytes(2, byteorder='big', signed=True)
        )
        
        # 故意损坏CRC
        corrupted_frame = bytearray(normal_frame)
        corrupted_frame[-3] = (corrupted_frame[-3] + 1) % 256  # 修改CRC高字节
        
        print(f"发送CRC损坏的帧: {' '.join(f'{b:02X}' for b in corrupted_frame)}")
        self.serial_comm.ser.write(corrupted_frame)
        time.sleep(0.2)
        
        # 发送正常帧
        print(f"发送正常帧: {' '.join(f'{b:02X}' for b in normal_frame)}")
        self.serial_comm.ser.write(normal_frame)
        time.sleep(0.5)
        
        # 接收回显
        response = self.serial_comm.read_echo()
        print(f"收到回显: {response}")
        
        # 判断是否恢复
        if response == 5555:
            print("✓ 恢复成功 - 能够正确识别和处理后续的正确帧\n")
        else:
            print("✗ 恢复失败 - 无法正确处理后续帧\n")
            
    def test_garbage_between_frames(self):
        """测试帧间垃圾数据的情况"""
        print("\n=== 测试 5: 帧间垃圾数据 ===")
        
        # 构建第一个正常帧
        value1 = 1111
        frame1 = self.serial_comm.build_frame(
            self.serial_comm.TYPE_INT, 
            value1.to_bytes(2, byteorder='big', signed=True)
        )
        
        # 构建第二个正常帧
        value2 = 2222
        frame2 = self.serial_comm.build_frame(
            self.serial_comm.TYPE_INT, 
            value2.to_bytes(2, byteorder='big', signed=True)
        )
        
        # 生成随机垃圾数据
        garbage = bytearray([random.randint(0, 255) for _ in range(10)])
        
        # 发送第一帧
        print(f"发送第一帧: {' '.join(f'{b:02X}' for b in frame1)}")
        self.serial_comm.ser.write(frame1)
        time.sleep(0.2)
        
        # 发送垃圾数据
        print(f"发送垃圾数据: {' '.join(f'{b:02X}' for b in garbage)}")
        self.serial_comm.ser.write(garbage)
        time.sleep(0.2)
        
        # 发送第二帧
        print(f"发送第二帧: {' '.join(f'{b:02X}' for b in frame2)}")
        self.serial_comm.ser.write(frame2)
        time.sleep(0.5)
        
        # 读取两次，获取两个帧的响应
        response1 = self.serial_comm.read_echo()
        time.sleep(0.2)
        response2 = self.serial_comm.read_echo()
        
        print(f"第一个响应: {response1}")
        print(f"第二个响应: {response2}")
        
        # 判断是否恢复
        if response1 == 1111 and response2 == 2222:
            print("✓ 恢复成功 - 能够跳过垃圾数据并处理两个帧\n")
        else:
            print("✗ 恢复失败 - 无法正确处理帧间垃圾数据\n")
    
    def test_partial_frame(self):
        """测试不完整帧的情况"""
        print("\n=== 测试 6: 不完整帧处理 ===")
        
        # 构建一个正常帧
        value = 7777
        normal_frame = self.serial_comm.build_frame(
            self.serial_comm.TYPE_INT, 
            value.to_bytes(2, byteorder='big', signed=True)
        )
        
        # 只发送帧的前半部分
        partial_frame = normal_frame[:len(normal_frame)//2]
        print(f"发送不完整帧: {' '.join(f'{b:02X}' for b in partial_frame)}")
        self.serial_comm.ser.write(partial_frame)
        time.sleep(0.2)
        
        # 发送正常帧
        print(f"发送完整帧: {' '.join(f'{b:02X}' for b in normal_frame)}")
        self.serial_comm.ser.write(normal_frame)
        time.sleep(0.5)
        
        # 接收回显
        response = self.serial_comm.read_echo()
        print(f"收到回显: {response}")
        
        # 判断是否恢复
        if response == 7777:
            print("✓ 恢复成功 - 能够处理后续的完整帧\n")
        else:
            print("✗ 恢复失败 - 无法恢复至正常状态\n")
    
    def test_embedded_frames(self):
        """测试帧内嵌套另一个帧的情况"""
        print("\n=== 测试 7: 帧内嵌套帧 ===")
        
        # 构建第一个帧
        value1 = 8888
        frame1 = bytearray(self.serial_comm.build_frame(
            self.serial_comm.TYPE_INT, 
            value1.to_bytes(2, byteorder='big', signed=True)
        ))
        
        # 构建第二个帧
        value2 = 9999
        frame2 = self.serial_comm.build_frame(
            self.serial_comm.TYPE_INT, 
            value2.to_bytes(2, byteorder='big', signed=True)
        )
        
        # 在第一个帧的中间插入第二个帧的帧头
        insert_pos = len(frame1) // 2
        frame1.insert(insert_pos, self.serial_comm.FRAME_HEADER)
        
        print(f"发送嵌套帧头的帧: {' '.join(f'{b:02X}' for b in frame1)}")
        self.serial_comm.ser.write(frame1)
        time.sleep(0.2)
        
        # 发送正常帧
        print(f"发送正常帧: {' '.join(f'{b:02X}' for b in frame2)}")
        self.serial_comm.ser.write(frame2)
        time.sleep(0.5)
        
        # 接收回显
        response = self.serial_comm.read_echo()
        print(f"收到回显: {response}")
        
        # 判断是否恢复
        if response == 9999:
            print("✓ 恢复成功 - 能够从嵌套帧中恢复\n")
        else:
            print("✗ 恢复失败 - 无法从嵌套帧中恢复\n")
            
    def run_all_tests(self):
        """运行所有测试"""
        try:
            self.test_basic_communication()
            time.sleep(0.5)
            
            self.test_corrupted_frame_header()
            time.sleep(0.5)
            
            self.test_corrupt_frame_footer()
            time.sleep(0.5)
            
            self.test_corrupt_crc()
            time.sleep(0.5)
            
            self.test_garbage_between_frames()
            time.sleep(0.5)
            
            self.test_partial_frame()
            time.sleep(0.5)
            
            self.test_embedded_frames()
            
            print("\n所有测试完成!")
            
        finally:
            self.serial_comm.close()

if __name__ == "__main__":
    # 请替换为您的串口
    PORT = "COM10"  # Windows 示例
    # PORT = "/dev/ttyUSB0"  # Linux 示例
    # PORT = "/dev/tty.usbserial-XXXXXXX"  # Mac 示例
    
    test = CorruptionTest(PORT)
    test.run_all_tests()