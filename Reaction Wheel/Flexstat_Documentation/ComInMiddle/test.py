import serial
import time
import threading

# Define ports
com_pc_side = 'COM1'
com_stm32_side = 'COM4'

# Serial config
baud_rate = 921600
timeout = 0.1

# Global serial handles
ser_pc = None
ser_stm32 = None

def send_custom_bytes():
    print("Type: send [pc|stm32] [hex_bytes]  — e.g., send stm32 01ffbe")
    while True:
        try:
            user_input = input("> ").strip()
            if not user_input.lower().startswith("send "):
                continue
            parts = user_input.split()
            if len(parts) != 3:
                print("Invalid format. Usage: send [pc|stm32] [hex_bytes]")
                continue
            target, hex_str = parts[1], parts[2]
            data = bytes.fromhex(hex_str)
            if target.lower() == "pc" and ser_pc:
                ser_pc.write(data)
                print(f"[You → PC]     HEX: {hex_str}")
            elif target.lower() == "stm32" and ser_stm32:
                ser_stm32.write(data)
                print(f"[You → STM32]  HEX: {hex_str}")
            else:
                print("Unknown target. Use 'pc' or 'stm32'.")
        except Exception as e:
            print(f"Error sending bytes: {e}")

try:
    # Open ports
    ser_pc = serial.Serial(port=com_pc_side, baudrate=baud_rate, timeout=timeout)
    ser_stm32 = serial.Serial(port=com_stm32_side, baudrate=baud_rate, timeout=timeout)

    # Flush any stale data
    ser_pc.reset_input_buffer()
    ser_stm32.reset_input_buffer()

    # Start thread for custom input
    threading.Thread(target=send_custom_bytes, daemon=True).start()

    # Open log file
    with open('log.txt', 'a+', buffering=1, encoding='utf-8') as log:
        print("Bridge started: Logging " + com_pc_side + " <--> " + com_stm32_side)

        while True:
            # PC → STM32
            if ser_pc.in_waiting:
                data = ser_pc.read(ser_pc.in_waiting)
                ser_stm32.write(data)
                hex_str = data.hex()
                ascii_str = data.decode(errors='ignore')
                log.write(f"[PC → STM32] {time.ctime()} | HEX: {hex_str} | TXT: {ascii_str}\n")
                print(f"[PC → STM32] HEX: {hex_str} | TXT: {ascii_str}")

            # STM32 → PC
            if ser_stm32.in_waiting:
                data = ser_stm32.read(ser_stm32.in_waiting)
                ser_pc.write(data)
                hex_str = data.hex()
                ascii_str = data.decode(errors='ignore')
                log.write(f"[STM32 → PC] {time.ctime()} | HEX: {hex_str} | TXT: {ascii_str}\n")
                print(f"[STM32 → PC] HEX: {hex_str} | TXT: {ascii_str}")

except KeyboardInterrupt:
    print("\nBridge stopped by user.")
except Exception as e:
    print(f"\nError: {e}")
finally:
    if ser_pc and ser_pc.is_open:
        ser_pc.close()
    if ser_stm32 and ser_stm32.is_open:
        ser_stm32.close()
