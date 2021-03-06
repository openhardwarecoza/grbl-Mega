/*
  main.c - An embedded CNC Controller with rs274/ngc (g-code) support
  Part of Grbl

  Copyright (c) 2011-2016 Sungeun K. Jeon for Gnea Research LLC
  Copyright (c) 2009-2011 Simen Svale Skogsrud

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "grbl.h"


// Declare system global variable structure
system_t sys;
int32_t sys_position[N_AXIS];      // Real-time machine (aka home) position vector in steps.
int32_t sys_probe_position[N_AXIS]; // Last probe position in machine coordinates and steps.
volatile uint8_t sys_probe_state;   // Probing state value.  Used to coordinate the probing cycle with stepper ISR.
volatile uint8_t sys_rt_exec_state;   // Global realtime executor bitflag variable for state management. See EXEC bitmasks.
volatile uint8_t sys_rt_exec_alarm;   // Global realtime executor bitflag variable for setting various alarms.
volatile uint8_t sys_rt_exec_motion_override; // Global realtime executor bitflag variable for motion-based overrides.
volatile uint8_t sys_rt_exec_accessory_override; // Global realtime executor bitflag variable for spindle/coolant overrides.
volatile uint8_t sys_rt_exec_axis;   // added Global realtime executor bitflag variable for axis movement.
volatile uint8_t sys_rt_exec_position;   // added Global realtime executor bitflag variable for homing and zeroing.

volatile uint8_t main_count;
volatile uint8_t protocol_count;
volatile uint8_t step_per_click;

volatile uint8_t nX;
volatile uint8_t nY;
volatile uint8_t bX;
volatile uint8_t bY;

#ifdef DEBUG
  volatile uint8_t sys_rt_exec_debug;
#endif


int main(void)
{
  // Initialize system upon power-up.
  
  //added
  pinMode(xUpPin,   INPUT_PULLUP);
  pinMode(xDownPin, INPUT_PULLUP);
  pinMode(yUpPin,   INPUT_PULLUP);
  pinMode(yDownPin, INPUT_PULLUP);
  pinMode(zUpPin,   INPUT_PULLUP);
  pinMode(zDownPin, INPUT_PULLUP);
  pinMode(xySetPin, INPUT_PULLUP);
  pinMode(zSetPin,  INPUT_PULLUP);
  pinMode(StepSetPin, INPUT_PULLUP);
  pinMode(zHometPin,  INPUT_PULLUP);
  pinMode(xyHomePin,  INPUT_PULLUP);
  pinMode(goXyPin,    INPUT_PULLUP);
  
  pinMode(lcd_rs,  OUTPUT);
  pinMode(lcd_en,  OUTPUT);
  pinMode(lcd_d4,  OUTPUT);
  pinMode(lcd_d5,  OUTPUT);
  pinMode(lcd_d6,  OUTPUT);
  pinMode(lcd_d7,  OUTPUT);
  
  pinMode(encoderXaPin, INPUT_PULLUP);
  pinMode(encoderXbPin, INPUT_PULLUP);
  pinMode(encoderYaPin, INPUT_PULLUP);
  pinMode(encoderYbPin, INPUT_PULLUP);
  pinMode(encoderSetX0, INPUT_PULLUP);
  pinMode(encoderSetY0, INPUT_PULLUP);
  
  //end added
  serial_init();   // Setup serial baud rate and interrupts
  settings_init(); // Load Grbl settings from EEPROM
  stepper_init();  // Configure stepper pins and interrupt timers
  system_init();   // Configure pinout pins and pin-change interrupt

  memset(sys_position,0,sizeof(sys_position)); // Clear machine position.
  sei(); // Enable interrupts

  // Initialize system state.
  #ifdef FORCE_INITIALIZATION_ALARM
    // Force Grbl into an ALARM state upon a power-cycle or hard reset.
    sys.state = STATE_ALARM;
  #else
    sys.state = STATE_IDLE;
  #endif
  
  // Check for power-up and set system alarm if homing is enabled to force homing cycle
  // by setting Grbl's alarm state. Alarm locks out all g-code commands, including the
  // startup scripts, but allows access to settings and internal commands. Only a homing
  // cycle '$H' or kill alarm locks '$X' will disable the alarm.
  // NOTE: The startup script will run after successful completion of the homing cycle, but
  // not after disabling the alarm locks. Prevents motion startup blocks from crashing into
  // things uncontrollably. Very bad.
  #ifdef HOMING_INIT_LOCK
    if (bit_istrue(settings.flags,BITFLAG_HOMING_ENABLE)) { sys.state = STATE_ALARM; }
  #endif

   SetUpLCD();
   
   PrintPosLCD(000.00, 000.00, 000.00);
   PrintWcoLCD(000.00, 000.00, 000.00);
   
   main_count = 0;
   protocol_count = 0;
   
  // Grbl initialization loop upon power-up or a system abort. For the latter, all processes
  // will return to this loop to be cleanly re-initialized.
  for(;;) {

	main_count++;
	//PrintMillsLCD(0, main_count);
        
    //memset(sys_position,0,sizeof(sys_position)); // Clear machine position.
    // Reset system variables.
    uint8_t prior_state = sys.state;
    memset(&sys, 0, sizeof(system_t)); // Clear system struct variable.
    sys.state = prior_state;
    sys.f_override = DEFAULT_FEED_OVERRIDE;  // Set to 100%
    sys.r_override = DEFAULT_RAPID_OVERRIDE; // Set to 100%
    sys.spindle_speed_ovr = DEFAULT_SPINDLE_SPEED_OVERRIDE; // Set to 100%
		memset(sys_probe_position,0,sizeof(sys_probe_position)); // Clear probe position.
    sys_probe_state = 0;
    sys_rt_exec_state = 0;
    sys_rt_exec_alarm = 0;
    sys_rt_exec_motion_override = 0;
    sys_rt_exec_accessory_override = 0;
    step_per_click = 0;
    
	sys_rt_exec_axis  = 0;   // added
	sys_rt_exec_position = 0;   // added
	
	sys.sfeed_rate = 250;
    sys.cmd_count  = 0;
    sys.cmd_count_enable = 0;
    //sys.step_per_click = 0.01;
        
    sys.encoderXPinALast = LOW;
    sys.nX = LOW;
    sys.encoderYPinALast = LOW;
    sys.nY = LOW;

	sys.bX = 0;
	sys.bY = 0;
	sys.step_click = 0.001;
	sys.encoderXFPos = 0.0;
	sys.encoderYFPos = 0.0;

	

    // Reset Grbl primary systems.
    serial_reset_read_buffer(); // Clear serial read buffer
    gc_init(); // Set g-code parser to default state
    spindle_init();
    coolant_init();
    limits_init();
    probe_init();
    sleep_init();
    plan_reset(); // Clear block buffer and planner variables
    st_reset(); // Clear stepper subsystem variables.

    // Sync cleared gcode and planner positions to current system position.
    plan_sync_position();
    gc_sync_position();

    // Print welcome message. Indicates an initialization has occured at power-up or with a reset.
    report_init_message();

    // Start Grbl main loop. Processes program inputs and executes them.
    protocol_main_loop();

  }
  return 0;   /* Never reached */
}
