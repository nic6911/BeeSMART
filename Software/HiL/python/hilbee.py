import paho.mqtt.client as mqtt
import serial
import time

# Set the MQTT broker address and topic names
MQTT_BROKER = "localhost"
ACTUATOR_TOPIC = "actuator"
WEIGHT_TOPIC = "weight"

# Set the serial port configuration
SERIAL_PORT = 'COM95'  # Replace with your serial port name
SERIAL_BAUDRATE = 250000  # Replace with your baudrate

# Define the MQTT client
client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with result code " + str(rc))
    # Subscribe to the weight topic upon successful connection
    client.subscribe(WEIGHT_TOPIC)

def on_message(client, userdata, msg):
    # This function will be called when a message is received on the WEIGHT_TOPIC
    weight_value = msg.payload.decode('utf-8')
    #print("Received weight value:", weight_value)
    # Write the weight value to the serial port
    ser.write((weight_value + ',').encode('utf-8'))

client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_BROKER, 1883, 60)

# Define the serial port and start reading input data
ser = serial.Serial(SERIAL_PORT, SERIAL_BAUDRATE)
ser.flushInput()

# Start the MQTT client loop in the background
client.loop_start()

try:
    while True:
        #time.sleep(1)
        # Read the actuator value from serial input
        actuator_value = ser.readline().strip()
        # Convert the string value to a float
        actuator_value = float(actuator_value)

        # Limit the value between 0 and 100
        if actuator_value < 0:
            actuator_value = 0
        elif actuator_value > 100:
            actuator_value = 100     
        #print("Actuator value:", actuator_value)

        # Publish the actuator value to the MQTT topic
        client.publish(ACTUATOR_TOPIC, actuator_value)

except KeyboardInterrupt:
    pass

# Stop the MQTT client loop and disconnect
client.loop_stop()
client.disconnect()
