import paho.mqtt.client as mqtt
import serial
import time
import glob
import sys

# Set the MQTT broker address and topic names
MQTT_BROKER = "localhost"
ACTUATOR_TOPIC = "actuator"
WEIGHT_TOPIC = "weight"
STATE_TOPIC = "state"

# Set the serial port configuration
def get_serial_port():
    # Require a port as a command-line argument
    if len(sys.argv) > 1:
        return sys.argv[1]
    print("Error: No serial port provided. Exiting.")
    sys.exit(1)

SERIAL_BAUDRATE = 250000  # Replace with your baudrate

# Define the MQTT client
client = mqtt.Client()

ser = None

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with result code " + str(rc))
    client.subscribe(WEIGHT_TOPIC)

def on_message(client, userdata, msg):
    weight_value = msg.payload.decode('utf-8')
    print("Received weight value:", weight_value)
    try:
        if ser and ser.is_open:
            ser.write((weight_value + ',').encode('utf-8'))
    except Exception as e:
        print(f"Serial write error: {e}")

client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_BROKER, 1883, 60)

# Start the MQTT client loop in the background
client.loop_start()

def connect_serial():
    while True:
        port = get_serial_port()
        if port:
            try:
                s = serial.Serial(port, SERIAL_BAUDRATE, timeout=1)
                s.flushInput()
                print(f"Connected to serial port: {port}")
                return s
            except Exception as e:
                print(f"Failed to connect to serial port {port}: {e}")
        else:
            print("No serial port found. Retrying in 3 seconds...")
        time.sleep(3)
def main():
    global ser
    try:
        while True:
            if ser is None or not ser.is_open:
                ser = connect_serial()
            try:
                inputvalue = ser.readline().strip()
                actuator_value = float(inputvalue)
                if actuator_value < 0:
                    actuator_value = 0
                elif actuator_value > 100:
                    actuator_value = 100
                print("Actuator value:", actuator_value)
                client.publish(ACTUATOR_TOPIC, actuator_value)
            except Exception as e:
                print(f"Serial error: {e}. Attempting to reconnect...")
                try:
                    ser.close()
                except Exception:
                    pass
                ser = None
                time.sleep(2)
    except KeyboardInterrupt:
        print("Exiting hilbee.")
        client.loop_stop()
        client.disconnect()
        if ser:
            try:
                ser.close()
            except Exception:
                pass

if __name__ == "__main__":
    main()

try:
    while True:
        if ser is None or not ser.is_open:
            ser = connect_serial()
        try:
            inputvalue = ser.readline().strip()
            actuator_value = float(inputvalue)
            if actuator_value < 0:
                actuator_value = 0
            elif actuator_value > 100:
                actuator_value = 100
            print("Actuator value:", actuator_value)
            client.publish(ACTUATOR_TOPIC, actuator_value)
        except Exception as e:
            print(f"Serial error: {e}. Attempting to reconnect...")
            try:
                ser.close()
            except Exception:
                pass
            ser = None
            time.sleep(2)
except KeyboardInterrupt:
    print("Exiting hilbee.")
    client.loop_stop()
    client.disconnect()
    if ser:
        try:
            ser.close()
        except Exception:
            pass
