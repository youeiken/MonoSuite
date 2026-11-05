import datetime
import time
import sys
import subprocess

# Auto-ensure pyserial is accessible inside the PlatformIO environment
try:
    import serial
    import serial.tools.list_ports
except ImportError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pyserial"])
    import serial
    import serial.tools.list_ports

Import("env")

def send_time(source, target, env):
    print("\n[time-sync] Waiting for ESP32-S3 USB CDC port to reconnect...")
    
    # 1. Give the chip a second to finish flashing and start its reboot cycle
    time.sleep(1.5)
    
    # 2. Dynamically fetch the intended upload port
    target_port = env.subst("$UPLOAD_PORT")
    ser = None
    
    # 3. Retry loop: Probing the serial connection for up to 5 seconds
    start_time = time.time()
    while time.time() - start_time < 5.0:
        try:
            # Check if the port is active in the operating system
            available_ports = [p.device for p in serial.tools.list_ports.comports()]
            if target_port in available_ports or not target_port:
                # If target_port wasn't explicitly defined, grab the first available USB port
                actual_port = target_port if target_port in available_ports else available_ports[0]
                
                ser = serial.Serial(actual_port, 115200, timeout=2)
                print(f"[time-sync] Successfully connected to {actual_port}")
                break
        except Exception:
            pass
        time.sleep(0.5)

    if not ser:
        print(f"[time-sync] Skipped: Could not connect to serial port {target_port}")
        return

    try:
        # 4. Give the ESP32 firmware bootloader a moment to hit setup()
        time.sleep(1.0)
        
        # Format current UTC time strictly matching your 'parseIsoUtc' parser
        # now = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ")
        now = datetime.datetime.now(datetime.UTC).strftime("%Y-%m-%dT%H:%M:%SZ")
        payload = f"TIME={now}\n"
        
        ser.write(payload.encode('utf-8'))
        ser.flush()
        print(f"[time-sync] Sent data: {payload.strip()}")
        
        # Optional: Read back any immediate debugging confirmations from the ESP32
        time.sleep(0.2)
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            print(f"[time-sync] ESP32 Output:\n{response.strip()}")
            
    except Exception as e:
        print(f"[time-sync] Error transmitting data: {e}")
    finally:
        ser.close()

env.AddPostAction("upload", send_time)
