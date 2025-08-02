import paho.mqtt.client as mqtt
import threading
import keyboard

# Define global variables
actuator_value = "0"
weight_value = 0

# Set the MQTT broker address and topic names
MQTT_BROKER = "localhost"
ACTUATOR_TOPIC = "actuator"
WEIGHT_TOPIC = "weight"

# Define the MQTT client and connect to the broker
client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with result code " + str(rc))
    # Subscribe to the actuator topic upon successful connection
    client.subscribe(ACTUATOR_TOPIC)

def on_message(client, userdata, msg):
    global actuator_value, weight_value
    # This function will be called when a message is received on the ACTUATOR_TOPIC
    actuator_value = float(msg.payload.decode('utf-8'))
    print("Received actuator value:", actuator_value)
    # Calculate the weight (twice the actuator value)
    if actuator_value >= 10:
        weight_value += actuator_value * 0.2
    print("Calculated weight value:", weight_value)
    # Publish the weight value to the MQTT topic
    client.publish(WEIGHT_TOPIC, str(int(weight_value)))

def keyboard_listener():
    global weight_value
    while True:
        if keyboard.is_pressed('n') and actuator_value <= 10:
            weight_value = 0
            print("Weight value set to 0")
        elif keyboard.is_pressed('r'):
            weight_value = 20
            print("Weight value set to 20")

# Start the MQTT loop in a separate thread to allow keyboard input handling
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_BROKER, 1883, 60)
threading.Thread(target=client.loop_forever).start()

# Start the keyboard listener in the main thread
keyboard_listener()
