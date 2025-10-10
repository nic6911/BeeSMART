import paho.mqtt.client as mqtt
import threading
# PyQt5 for cross-platform UI
from PyQt5 import QtWidgets, QtCore
import sys
# Model logic
from model import HoneyDispenserModel
import time
import json
# For launching hilbee.py as a subprocess
import subprocess
import serial.tools.list_ports

# Define global variables
# Global variables for actuator and weight state
actuator_value = 0.0
weight_value = 0.0
weight_offset = 0.0

# Set the MQTT broker address and topic names
# MQTT broker address and topic names
MQTT_BROKER = "localhost"
ACTUATOR_TOPIC = "actuator"
WEIGHT_TOPIC = "weight"
STATE_TOPIC = "state"

# Define the MQTT client and connect to the broker
# Create MQTT client
client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    """Callback for MQTT connection"""
    print("Connected to MQTT broker with result code " + str(rc))
    client.subscribe(ACTUATOR_TOPIC)

def on_message(client, userdata, msg):
    """Callback for receiving actuator value from MQTT"""
    global actuator_value
    try:
        actuator_value = float(msg.payload.decode('utf-8'))
    except ValueError:
        print("Received invalid actuator value")


"""
PyQt5 UI for weight offset control
"""
class JarControlWindow(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("HiLBee")
        self.setGeometry(100, 100, 300, 150)
        layout = QtWidgets.QVBoxLayout()

        self.label = QtWidgets.QLabel(f"Place or remove jar")
        layout.addWidget(self.label)

        self.btn_place = QtWidgets.QPushButton("Place jar")
        self.btn_place.clicked.connect(self.place_jar)
        layout.addWidget(self.btn_place)

        self.btn_remove = QtWidgets.QPushButton("Remove jar")
        self.btn_remove.clicked.connect(self.remove_jar)
        layout.addWidget(self.btn_remove)



        # Add COM port dropdown and Start button
        self.com_ports = [port.device for port in serial.tools.list_ports.comports()]
        self.com_dropdown = QtWidgets.QComboBox()
        self.com_dropdown.addItems(self.com_ports)
        layout.addWidget(QtWidgets.QLabel("Select COM port:"))
        layout.addWidget(self.com_dropdown)

        self.start_button = QtWidgets.QPushButton("Connect")
        self.start_button.clicked.connect(self.toggle_connection)
        layout.addWidget(self.start_button)

        # Add auto jar placement slider
        self.auto_jar_slider = QtWidgets.QCheckBox("Auto jar placement")
        self.auto_jar_slider.stateChanged.connect(self.toggle_auto_jar)
        layout.addWidget(self.auto_jar_slider)

        self.setLayout(layout)

        self.auto_jar_thread = None
        self.auto_jar_running = False
        self.system_started = False

    def toggle_connection(self):
        global hilbee_proc
        if not self.system_started:
            selected_port = self.com_dropdown.currentText()
            if not selected_port:
                QtWidgets.QMessageBox.warning(self, "Error", "No COM port selected.")
                return
            hilbee_proc = subprocess.Popen([sys.executable, "hilbee.py", selected_port])
            self.system_started = True
            self.start_button.setText("Disconnect")
            self.start_button.setStyleSheet("background-color: #2196F3; color: white;")
        else:
            # Disconnect: terminate hilbee.py
            if hilbee_proc and hilbee_proc.poll() is None:
                hilbee_proc.terminate()
                hilbee_proc.wait()
            self.system_started = False
            self.start_button.setText("Connect")
            self.start_button.setStyleSheet("")

    def toggle_auto_jar(self, state):
        if state == QtCore.Qt.Checked:
            self.auto_jar_running = True
            self.auto_jar_thread = threading.Thread(target=self.auto_jar_loop, daemon=True)
            self.auto_jar_thread.start()
        else:
            self.auto_jar_running = False
            self.auto_jar_thread = None

    def auto_jar_loop(self):
        tap_closed_time = None
        while self.auto_jar_running:
            # Poll tap opening value
            if model.tap_opening == 0.0:
                if tap_closed_time is None:
                    tap_closed_time = time.time()
                elif time.time() - tap_closed_time >= 10:
                    if not self.auto_jar_running:
                        break
                    self.remove_jar()
                    time.sleep(2)
                    if not self.auto_jar_running:
                        break
                    self.place_jar()
                    tap_closed_time = None  # Reset after jar placement
            else:
                tap_closed_time = None
            # Stop if fill_height_cm < 5
            if model.fill_height_cm < 5.0:
                self.auto_jar_running = False
                self.auto_jar_slider.setChecked(False)
                break
            time.sleep(0.2)

    def place_jar(self):
        """Set weight offset to 50 when jar is placed"""
        global weight_offset
        weight_offset = 50.0
        self.label.setText(f"Jar is on the scale ({weight_offset}g)")
        print("Weight offset set to 50 (Place jar)")

    def remove_jar(self):
        """Reset weight offset and dispensed weight when jar is removed"""
        global weight_offset
        weight_offset = 0.0
        model.total_dispensed_g = 0.0
        self.label.setText(f"No jar on the scale")
        print("Weight offset set to 0 and dispensed reset (Remove jar)")


# Model loop to run in a background thread
"""
Background thread: runs the model simulation loop and publishes state to MQTT
"""
def model_loop(stop_event):
    global weight_value
    dt = 0.1  # seconds
    while not stop_event.is_set():
        model.set_tap_opening(actuator_value / 100.0)
        model.step(dt)
        weight_value = model.total_dispensed_g
        client.publish(WEIGHT_TOPIC, weight_value + weight_offset)
        state = model.get_state()
        client.publish(STATE_TOPIC, json.dumps(state))
        time.sleep(dt)

"""
Main entry point: starts MQTT, model, UI, and hilbee.py subprocess
Handles clean shutdown on UI close or Ctrl+C
"""
if __name__ == "__main__":
    stop_event = threading.Event()
    hilbee_proc = subprocess.Popen([sys.executable, "hilbee.py"])
    try:
        # MQTT setup
        client.on_connect = on_connect
        client.on_message = on_message
        client.connect(MQTT_BROKER, 1883, 60)
        client.loop_start()

        # Model setup
        model = HoneyDispenserModel()

        # Start model loop in background
        model_thread = threading.Thread(target=model_loop, args=(stop_event,), daemon=True)
        model_thread.start()

        app = QtWidgets.QApplication(sys.argv)
        window = JarControlWindow()
        window.show()

        # Connect UI close event to full shutdown
        def on_close():
            print("UI closed. Exiting application.")
            stop_event.set()
            model_thread.join()
            client.loop_stop()
            client.disconnect()
            if hilbee_proc.poll() is None:
                hilbee_proc.terminate()
                hilbee_proc.wait()
            sys.exit(0)
        app.aboutToQuit.connect(on_close)
        app.exec_()
    except KeyboardInterrupt:
        print("Exiting model handler.")
        stop_event.set()
        model_thread.join()
        client.loop_stop()
        client.disconnect()
        if hilbee_proc.poll() is None:
            hilbee_proc.terminate()
            hilbee_proc.wait()
        sys.exit(0)


