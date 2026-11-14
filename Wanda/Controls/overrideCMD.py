import time
import threading

# prints message with current time before each line for logging
from datetime import datetime
from pytz import timezone
est = timezone('US/Eastern')
def print_log(message:str):
    lines = message.split('\n')
    for line in lines:
        print(f"[{datetime.now(tz=est).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]}] {line}")

class SequenceAborted(Exception):
    pass

class OverrideManager:
    
    def __init__(self, controller_instance):
        self.controller = controller_instance
        # self.fire_enabled = False
        self.active_thread = None

    def process_command(self, switch_id, state):
        cmd = str(switch_id).lower()
        if cmd in self.OVERRIDDEN_CMDS:
            if self.active_thread and self.active_thread.is_alive():
                return True
            self.active_thread = threading.Thread(target=self.OVERRIDDEN_CMDS[cmd], args=(self, state))
            self.active_thread.start()
            return True
        return False   

    def set_relay_at(self, start, at_ms, pi, relay, state):
        while(time.perf_counter() - start) * 1000 < at_ms:
            if self.controller.abort:
                raise SequenceAborted("Abort requested")
        if self.controller.abort:
            raise SequenceAborted("Abort requested")
        return self.controller.set_relay(pi, relay, state)

    # TEST SEQUENCE
    def run_fire(self, state):
        igniter_t_ms = 0
        ox_open_t_ms = 250
        fuel_open_t_ms = 260
        ox_close_t_ms = 3000
        fuel_close_t_ms = 3005
        igniter_off_t_ms = 3005

        fire_start = time.perf_counter()

        try:
        # ! The functions below must be in firing order 
            self.set_relay_at(fire_start, igniter_t_ms, "wanda1", 8, True) # FIRE / IGNITER ON
            self.set_relay_at(fire_start, ox_open_t_ms, "wanda2", 2, True) # OX MAIN OPEN
            self.set_relay_at(fire_start, fuel_open_t_ms, "wanda2", 1, True) # FUEL MAIN OPEN
            self.set_relay_at(fire_start, ox_close_t_ms, "wanda2", 2, False) # OX MAIN CLOSE
            self.set_relay_at(fire_start, fuel_close_t_ms, "wanda2", 1, False) # FUEL MAIN CLOSE
            self.set_relay_at(fire_start, igniter_off_t_ms, "wanda1", 8, False) # IGNITER OFF
        except SequenceAborted:
            print_log("Sequence aborted")


    def run_fire_key(self, state):
        if state:
            self.controller.set_relay("wanda1", 6, True);
            self.controller.set_relay("wanda1", 7, True);
        else:
            self.controller.set_relay("wanda1", 6, False);
            self.controller.set_relay("wanda1", 7, False);
            self.controller.set_relay("wanda2", 1, False);
            self.controller.set_relay("wanda2", 2, False);
            self.controller.set_relay("wanda1", 8, False);

    OVERRIDDEN_CMDS = {
        "fire": run_fire,
        "fire key": run_fire_key,
    }
