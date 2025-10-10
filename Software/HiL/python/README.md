
# Honey Dispenser Model Documentation

## Overview

This repository contains a physics-based simulation of a honey dispensing system, designed for Hardware-in-the-Loop (HiL) testing. The model simulates a bucket of honey with a servo-controlled tap, and calculates the flow rate, transport delay, and dispensed weight based on actuator input and physical parameters. The system is integrated with MQTT for real-time control and state monitoring.

---

## `model.py` — HoneyDispenserModel

### Purpose
A class-based model that simulates the dispensing of honey from a bucket through a tap, considering geometry, viscosity, temperature, actuator position, and transport delay.

### Parameters
- **Bucket diameter** ($d_{bucket}$, cm)
- **Bucket height** ($h_{bucket}$, cm)
- **Tap diameter** ($d_{tap}$, mm)
- **Tap-to-scale distance** ($L_{tap-scale}$, cm)
- **Initial fill height** ($h_{fill}$, cm)
- **Viscosity type** (low, medium, high)
- **Temperature** ($T$, °C)

### Key Equations & Reasoning

#### 1. **Viscosity Adjustment**
Viscosity ($\eta$) is temperature-dependent:
$$
\eta = \eta_0 \cdot 2^{\frac{20 - T}{10}}
$$
Where $\eta_0$ is the base viscosity for the selected honey type.

#### 2. **Bucket Head Pressure**
Pressure at the tap due to honey column:
$$
P = \rho \cdot g \cdot h_{fill}
$$
Where:
- $\rho$ = honey density ($\approx 1400\,kg/m^3$)
- $g$ = gravity ($9.81\,m/s^2$)
- $h_{fill}$ = current honey height (m)

#### 3. **Tap Area**
$$
A_{tap} = \pi \left(\frac{d_{tap}}{2 \times 10}\right)^2
$$
Where $d_{tap}$ is in mm, converted to cm.

#### 4. **Flow Rate (Hagen-Poiseuille Law, simplified)**
$$
Q = \frac{\pi r^4 \Delta P}{8 \eta L}
$$
Where:
- $r$ = tap radius (m)
- $\Delta P$ = pressure difference (Pa)
- $\eta$ = viscosity (Pa·s)
- $L$ = tap length ($\approx d_{tap}$, m)

Converted to grams/second:
$$
Q_{g/s} = Q_{m^3/s} \cdot \rho \cdot 1000
$$

#### 5. **Transport Delay**
Time for honey to reach the scale:
$$
\text{delay} = \frac{L_{tap-scale}}{v}
$$
Where $v$ is average velocity:
$$
v = \frac{Q_{kg/s}}{\rho \cdot A_{tap}}
$$

#### 6. **Bucket Fill Update**
After each simulation step:
$$
\Delta h_{fill} = \frac{Q_{g/s} \cdot dt}{\rho \cdot A_{bucket} \cdot 10}
$$
Where $A_{bucket}$ is the bucket cross-sectional area (cm²).

### Usage Example
```python
from model import HoneyDispenserModel
model = HoneyDispenserModel()
model.set_tap_opening(1.0)  # fully open
dispensed = model.step(1.0) # simulate 1 second
state = model.get_state()
```

---


## HiL System Integration (`model_handler.py` & `hilbee.py`)

### Purpose
`model_handler.py` launches the honey dispenser simulation, exposes a PyQt5 UI for manual and automatic jar placement, and manages MQTT communication. It also starts `hilbee.py` as a subprocess for serial/MQTT integration.

### Features
- PyQt5 UI for manual jar placement ("Place jar" and "Remove jar" buttons)
- Dropdown menu to select available COM ports
- "Connect" button to start the system with the selected port (turns to "Disconnect" when running)
- "Disconnect" button stops the system and terminates `hilbee.py`
- Slider for enabling/disabling automatic jar placement
- Auto jar placement: When enabled, waits 10 seconds after the tap is closed, removes the jar, waits 2 seconds, places the jar, and repeats until the bucket is nearly empty (`fill_height_cm < 2`)
- Listens for actuator values on MQTT (`actuator` topic)
- Publishes model state as JSON to the `state` topic
- Publishes total dispensed weight for legacy compatibility (`weight` topic)
- Starts and terminates `hilbee.py` automatically with the selected port

### System Architecture
```
[Serial/Hardware] <-> [hilbee.py] <-> [MQTT Broker] <-> [model_handler.py + PyQt5 UI] <-> [HoneyDispenserModel]
```
- **Actuator input**: Received via MQTT or serial, mapped to tap opening (0-100%)
- **Model simulation**: Calculates flow, delay, and dispensed weight
- **State output**: All model states published as JSON on MQTT for downstream use

### Example State Output (MQTT `state` topic)
```json
{
  "fill_height_cm": 59.8,
  "tap_opening": 1.0,
  "temperature_c": 30.0,
  "viscosity_type": "low",
  "viscosity_Pa_s": 1.0,
  "flow_g_per_s": 120.5,
  "transport_delay_s": 0.8,
  "total_dispensed_g": 240.0
}
```

### Usage
1. Install dependencies from `requirements.txt` (see below for PyQt5 and other requirements).
2. Activate your Python virtual environment.
3. Run `model_handler.py`.
4. In the PyQt5 UI, select the desired COM port from the dropdown and press "Connect" to start the system (this will start `hilbee.py` with the selected port).
5. Use the UI to manually place/remove the jar, or enable auto jar placement.
6. Press "Disconnect" to stop the system and terminate `hilbee.py`.
7. The simulation will run until the bucket is nearly empty.

#### Requirements
- Python 3.x
- PyQt5
- paho-mqtt
- pyserial
- keyboard

Install with:
```sh
pip install -r requirements.txt
```

---


## Reasoning & Extensibility
- The model is designed for realism and configurability, allowing HiL testing with different honey types, temperatures, and hardware setups.
- All physical parameters are exposed and can be tuned for your scenario.
- MQTT integration enables seamless connection to real or simulated hardware and control systems.
- The PyQt5 UI makes it easy to control and automate jar placement for testing and demonstration purposes.

---

## References
- [Hagen-Poiseuille Law](https://en.wikipedia.org/wiki/Hagen%E2%80%93Poiseuille_equation)
- [Honey Viscosity Data](https://www.sciencedirect.com/science/article/pii/S0023643819303932)

---

## License
MIT
