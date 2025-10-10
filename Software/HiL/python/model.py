import math

class HoneyDispenserModel:
    def __init__(self,
                 bucket_diameter_cm=40.0,
                 bucket_height_cm=60.0,
                 tap_diameter_mm=50.0,
                 tap_to_scale_cm=20.0,
                 initial_fill_cm=60.0,
                 viscosity_type='medium',
                 temperature_c=20.0):
        # Geometry
        self.bucket_diameter_cm = bucket_diameter_cm
        self.bucket_height_cm = bucket_height_cm
        self.tap_diameter_mm = tap_diameter_mm
        self.tap_to_scale_cm = tap_to_scale_cm
        self.fill_height_cm = initial_fill_cm

        # Viscosity presets (Pa.s)
        self.viscosity_table = {
            'low': 12.0,
            'medium': 25.0,
            'high': 75.0
        }
        self.viscosity_type = viscosity_type
        self.temperature_c = temperature_c

        # Actuator (servo) opening (0.0 = closed, 1.0 = fully open)
        self.tap_opening = 0.0

        # Internal state
        self.total_dispensed_g = 0.0
        self.last_flow_g_per_s = 0.0
        self.last_delay_s = 0.0
        self.time_since_open = 0.0  # NEW: track time since tap opened

    def set_tap_opening(self, opening):
        """Set tap opening (0.0 to 1.0)"""
        opening = max(0.0, min(1.0, opening))
        if opening == 0.0:
            self.time_since_open = 0.0  # reset ramp-up
        self.tap_opening = opening

    def set_temperature(self, temp_c):
        self.temperature_c = temp_c

    def set_viscosity_type(self, vtype):
        if vtype in self.viscosity_table:
            self.viscosity_type = vtype

    def set_fill_height(self, fill_cm):
        self.fill_height_cm = max(0.0, min(self.bucket_height_cm, fill_cm))

    def get_viscosity(self):
        base_visc = self.viscosity_table[self.viscosity_type]
        temp_factor = 2 ** ((20.0 - self.temperature_c) / 10.0)
        return base_visc * temp_factor

    def get_bucket_head_pressure(self):
        rho = 1400
        g = 9.81
        h = self.fill_height_cm / 100.0
        return rho * g * h

    def get_tap_area(self):
        r = (self.tap_diameter_mm / 10.0) / 2.0
        return math.pi * r * r

    def get_flow_rate(self):
        r = (self.tap_diameter_mm / 1000.0) / 2.0
        deltaP = self.get_bucket_head_pressure() * (self.tap_opening ** 2)
        viscosity = self.get_viscosity()
        L = self.tap_diameter_mm / 1000.0
        Q_m3_s = (math.pi * r**4 * deltaP) / (8 * viscosity * L)

        ramp_factor = min(1.0, self.time_since_open / 2.0)
        Q_g_s = Q_m3_s * 1400 * 1000
        Q_g_s *= ramp_factor
        # Optional turbulence correction
        Re = self.get_reynolds_number(Q_g_s)
        if Re > 2000:
            Q_g_s *= 0.7
        if deltaP < 50 or self.tap_opening < 0.05:
            Q_g_s = 0.0
        if self.tap_opening == 0.0 or self.fill_height_cm <= 0.0:
            Q_g_s = 0.0

        self.last_flow_g_per_s = Q_g_s
        return Q_g_s

    def get_reynolds_number(self, flow_rate_g_s):
        rho = 1400
        r = (self.tap_diameter_mm / 1000.0) / 2.0
        area = math.pi * r**2
        flow_rate = flow_rate_g_s / 1000.0
        velocity = flow_rate / (rho * area)
        viscosity = self.get_viscosity()
        Re = (rho * velocity * 2 * r) / viscosity
        return Re

    def get_transport_delay(self):
        area = self.get_tap_area()
        area_m2 = area / 10000.0
        flow_rate = self.get_flow_rate() / 1000.0
        if area_m2 > 0 and flow_rate > 0:
            velocity = flow_rate / (1400 * area_m2)
            delay = self.tap_to_scale_cm / 100.0 / velocity
        else:
            delay = 0.0
        self.last_delay_s = delay
        return delay

    def step(self, dt):
        """Advance simulation by dt seconds, update dispensed weight"""
        if self.tap_opening > 0.0:
            self.time_since_open += dt
        else:
            self.time_since_open = 0.0

        flow = self.get_flow_rate()
        dispensed = flow * dt

        bucket_area = math.pi * (self.bucket_diameter_cm / 2.0)**2
        bucket_area_m2 = bucket_area / 10000.0
        volume_dispensed_m3 = dispensed / 1400 / 1000
        height_loss_cm = (volume_dispensed_m3 / bucket_area_m2) * 100.0
        self.fill_height_cm = max(0.0, self.fill_height_cm - height_loss_cm)
        self.total_dispensed_g += dispensed
        return dispensed

    def get_state(self):
        return {
            'fill_height_cm': self.fill_height_cm,
            'tap_opening': self.tap_opening,
            'temperature_c': self.temperature_c,
            'viscosity_type': self.viscosity_type,
            'viscosity_Pa_s': self.get_viscosity(),
            'flow_g_per_s': self.last_flow_g_per_s,
            'transport_delay_s': self.last_delay_s,
            'total_dispensed_g': self.total_dispensed_g,
            'time_since_open': self.time_since_open
        }