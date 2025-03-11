# serial_comm_request_test.py
from serial_comm import SerialComm
import time

def main():
    # 替换 'COM10' 为您的串口名称
    serial_comm = SerialComm('COM10')

    try:
        while True:
            # 请求浮点数据
            print("\n发起浮点数请求...")
            response = serial_comm.request_float()
            
            if response is not None:
                print(f"收到Arduino返回的浮点数: {response}")
            else:
                print("没有收到响应")
                
            time.sleep(0.004)  # 每2秒请求一次

    except KeyboardInterrupt:
        print("通讯终止")
    finally:
        serial_comm.close()

if __name__ == "__main__":
    main()