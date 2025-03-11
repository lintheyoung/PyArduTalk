# gyro_test.py
from serial_comm import SerialComm
import time

def main():
    # 替换为您的串口名称
    serial_comm = SerialComm('COM10')  # Windows示例
    # serial_comm = SerialComm('/dev/ttyUSB0')  # Linux示例
    # serial_comm = SerialComm('/dev/tty.usbserial-XXXXXXX')  # Mac示例

    try:
        print("\n===== 陀螺仪数据测试 =====")
        
        # 请求陀螺仪数据
        print("\n请求陀螺仪数据")
        gyro_data = serial_comm.request_gyro()
        
        if gyro_data:
            print(f"收到陀螺仪数据:")
            print(f"Yaw: {gyro_data['yaw']}°")
            print(f"Roll: {gyro_data['roll']}°")
            print(f"Pitch: {gyro_data['pitch']}°")
        else:
            print("未收到有效的陀螺仪数据")
        
        print("\n陀螺仪数据测试完成!")

    except KeyboardInterrupt:
        print("测试终止")
    except Exception as e:
        print(f"发生错误: {e}")
    finally:
        serial_comm.close()

if __name__ == "__main__":
    main()