class GPIOSimulator:
    def __init__(self):
        self.pins = {}

    def setup(self, pin, mode):
        self.pins[pin] = False

    def output(self, pin, state):
        self.pins[pin] = bool(state)

    def input(self, pin):
        return self.pins.get(pin, False)

    def cleanup(self):
        self.pins.clear()

class OutputDevice:
    def __init__(self, pin):
        self.pin = pin
        GPIO.setup(pin, "OUT")

    def on(self):
        GPIO.output(self.pin, True)

    def off(self):
        GPIO.output(self.pin, False)

# Initialize the global GPIO simulator
GPIO = GPIOSimulator()
