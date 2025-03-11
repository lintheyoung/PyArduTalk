# request_response_test.py
from serial_comm import SerialComm
import time

def main():
    # 替换为您的串口名称
    serial_comm = SerialComm('COM10')  # Windows示例
    # serial_comm = SerialComm('/dev/ttyUSB0')  # Linux示例
    # serial_comm = SerialComm('/dev/tty.usbserial-XXXXXXX')  # Mac示例

    try:
        # 测试请求-响应模式
        print("\n===== 请求-响应测试 =====")
        
        # 请求整数数据
        print("\n1. 请求整数数据")
        int_response = serial_comm.request_int()
        print(f"收到整数响应: {int_response}")
        time.sleep(1)
        
        # 请求浮点数数据
        print("\n2. 请求浮点数数据")
        float_response = serial_comm.request_float()
        print(f"收到浮点数响应: {float_response}")
        time.sleep(1)
        
        # 请求字符串数据
        print("\n3. 请求字符串数据")
        string_response = serial_comm.request_string()
        print(f"收到字符串响应: {string_response}")
        time.sleep(1)
        
        # 请求JSON数据
        print("\n4. 请求JSON数据")
        json_response = serial_comm.request_json()
        print(f"收到JSON响应: {json_response}")
        
        print("\n请求-响应测试完成!")

    except KeyboardInterrupt:
        print("测试终止")
    except Exception as e:
        print(f"发生错误: {e}")
    finally:
        serial_comm.close()

if __name__ == "__main__":
    main()