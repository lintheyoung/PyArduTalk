# main.py
from serial_comm import SerialComm
import time
import struct
import json

def main():
    # 替换 'COM7' 为您的串口名称
    serial_comm = SerialComm('COM7')

    try:
        while True:
            # 示例1: 发送整数
            int_value = 12345
            serial_comm.send_int(int_value)
            time.sleep(0.1)

            # 示例2: 发送浮点数
            float_value = -123.450
            serial_comm.send_float(float_value)
            time.sleep(0.1)

            # 示例3: 发送字符串
            str_value = "Hello, Arduino!"
            serial_comm.send_string(str_value)
            time.sleep(0.1)

            # 示例4: 发送JSON
            json_dict = {
                "command": "toggleLED",
                "value": True
            }
            serial_comm.send_json(json_dict)
            time.sleep(0.1)

            # 读取回显
            serial_comm.read_echo()

            time.sleep(1)  # 根据需要调整发送频率

    except KeyboardInterrupt:
        print("通讯终止")
    finally:
        serial_comm.close()

if __name__ == "__main__":
    main()
