#include <UC1701.h>
#include <Indio.h>
#include <Wire.h>


/*
Controls contactors that control the direction of rotation a 3-phase motor (used to rotate the filter in the leach tank) by reversing two of the phases. 
Also controls two solonoid valves (to inflate and deflate a bladder) and a vacuum pump. 


*/

// Prototypes

bool raiseflag(void);

void LCDprint(char* cycle_stage, int set_time);

// Set sytem timings here

const unsigned long rotation_time = 60000;
const unsigned int motor_pause_time = 2000;

const unsigned long bladder_venting_time = 60000;
unsigned long vacuum_pump_runtime = 60000;
const unsigned long bladder_empty_time = 60000;
const unsigned long bladder_inflation_time = 30000;

// Set I/O pins

const int contactor1 = 1;
const int contactor2 = 2;
const int vacuum_pump = 3;
const int air = 5; // solenoid
const int vacuum = 4; //solenoid
const int air_switch = 7; // digital input

// Timing system variables

unsigned long LCD_update_currentMillis = 0;
unsigned long LCD_update_previousMillis = 0;

unsigned long motor_currentMillis = 0;
unsigned long motor_previousMillis = 0;

unsigned long bladder_currentMillis = 0;
unsigned long bladder_previousMillis = 0;

// Motor control flags

bool running_clockwise = false;
bool running_counter_clockwise = false;
bool cycle_end = false;

// Bladder control flags

bool vacuuming = false;
bool venting = false;
bool resting = false;
bool inflating = false;

// Air switch

bool first_contact = false;

bool inflate_override = false;

unsigned long debounce = 0;

int debounce_period = 200;

// LCD 

static UC1701 lcd;

const int backlightPin = 26;


void setup() {

  // LCD setup

  pinMode(backlightPin, OUTPUT);

  analogWrite(backlightPin, 255);

  lcd.begin();

  lcd.clear();

  lcd.setCursor(5, 1);

  lcd.print("System start");
  
  // Set pin mode for pressure/release solonoid
  
  Indio.digitalMode(air, OUTPUT);
  
  Indio.digitalWrite(air, LOW);

  // Set pin mode for vacuum pump solonoid
  
  Indio.digitalMode(vacuum, OUTPUT);
  
  Indio.digitalWrite(vacuum, LOW);
  
  // Set pin mode for vacuum pump contactor
  
  Indio.digitalMode(vacuum_pump, OUTPUT);
    
  Indio.digitalWrite(vacuum_pump, LOW);
  
  // Set pin modes for the motor control contactors
  
  Indio.digitalMode(contactor1, OUTPUT);
  
  Indio.digitalWrite(contactor1, LOW);
  
  Indio.digitalMode(contactor2, OUTPUT);
  
  Indio.digitalWrite(contactor2, LOW);
  
  // Set pin mode for air switch
  
  Indio.digitalMode(air_switch, INPUT);

  // Run motor clockwise
  
  running_clockwise = true;
  
  Indio.digitalWrite(contactor1, HIGH);

  motor_previousMillis = millis();

  // Deflate bladder by opening vacuum solonoid and running vacuum pump

  Indio.digitalWrite(vacuum, LOW);
  
  Indio.digitalWrite(vacuum_pump, HIGH);

  vacuuming = raiseflag();

  // Start bladder timer

  bladder_previousMillis = millis();

  LCDprint("Initial Vacuuming", vacuum_pump_runtime);

  LCD_update_previousMillis = millis();
  

  
}


void loop() {
  
  // Update LCD time-elapsed display

  LCD_update_currentMillis = millis();

  if (LCD_update_currentMillis - LCD_update_previousMillis >= 1000) {

    lcd.setCursor(60, 5);

    lcd.print((millis() - bladder_previousMillis)/1000);

    LCD_update_previousMillis = millis();
  }
  
  // --- After the time for one rotation has passed, open all motor relays
  
  motor_currentMillis = millis();
  
  if ((unsigned long)(motor_currentMillis - motor_previousMillis) >= rotation_time) {
    
    // Open all contactors
  
    Indio.digitalWrite(contactor1, LOW);
    Indio.digitalWrite(contactor2, LOW);
    
    cycle_end = true;
    
    // Update timer
    
    motor_previousMillis = millis();
  
  }
  

  // --- After motor pause time has passed (allowing for weird induction residual magic to dissipate), start the motor in the opposite direction to the last cycle
  
  motor_currentMillis = millis();
    
  if(((unsigned long)(motor_currentMillis - motor_previousMillis) >= motor_pause_time) && (cycle_end == true)) {
    
    if (running_clockwise == true) {
      running_clockwise = false;
      running_counter_clockwise = true;
      
      // Close contactor 2
      
      Indio.digitalWrite(contactor2, HIGH);
  
      } else {
        running_counter_clockwise = false;
        running_clockwise = true;
        
        // Close contactor1
    
        Indio.digitalWrite(contactor1, HIGH);
    }

    cycle_end = false;

    motor_previousMillis = millis();
  }


  // --- When vacuuming time has elapsed, set bladder to rest

  bladder_currentMillis = millis();

  if (((unsigned long)(bladder_currentMillis - bladder_previousMillis) >= vacuum_pump_runtime) && (vacuuming == true)) {
    // Close vacuum relay, stop vacuum pump

    Indio.digitalWrite(vacuum, HIGH);
    
    Indio.digitalWrite(vacuum_pump, LOW);
    
     resting = raiseflag();

    bladder_previousMillis = millis();

    LCDprint("Resting", bladder_empty_time);
  }
  

  // --- When resting time has elapsed, set bladder to inflate

  bladder_currentMillis = millis();

  if (((unsigned long)(bladder_currentMillis - bladder_previousMillis) >= bladder_empty_time) && (resting == true)) {
    // Close air solonoid
    
    Indio.digitalWrite(air, HIGH);
    
    inflating = raiseflag();

    bladder_previousMillis = millis();

    LCDprint("Inflating", bladder_inflation_time);

   }
   

  // --- When inflating time has elapsed, vent bladder

  bladder_currentMillis = millis();

  if (((unsigned long)(bladder_currentMillis - bladder_previousMillis) >= bladder_inflation_time) && (inflating == true)) {  
    // Open air solonoid to allow bladder to vent

    Indio.digitalWrite(air, LOW);

    venting = raiseflag();

    bladder_previousMillis = millis();

    LCDprint("Venting", bladder_venting_time);
  }

  // --- When venting time has elapsed, vacuum bladder

  bladder_currentMillis = millis();

  if (((unsigned long)(bladder_currentMillis - bladder_previousMillis) >= bladder_venting_time) && (venting == true)) {

    // Open vacuum solonoid, start vacuum pump

    Indio.digitalWrite(vacuum, LOW);
    
    Indio.digitalWrite(vacuum_pump, HIGH);

    vacuuming = raiseflag();

    bladder_previousMillis = millis();

    LCDprint("Vacuuming", vacuum_pump_runtime);
  }
  
  // Check if air switch has been set to the on position, if so, start debounce timer
  
  if (Indio.digitalRead(air_switch) == HIGH && inflate_override == false) {
   if (first_contact == false) {
     first_contact = true;
     debounce = millis();
     }
  }
  
  // If the debounce timer has elapsed and the air switch is still high, set all process control flags to false and set solonoids to inflate position

  if ((first_contact == true) && (millis() - debounce > debounce_period)) {
   first_contact = false;
     if (Indio.digitalRead(air_switch) == HIGH) {
       inflate_override = true;
       raiseflag();
       Indio.digitalWrite(vacuum_pump, LOW);
       Indio.digitalWrite(air, HIGH);
       Indio.digitalWrite(vacuum, HIGH);

       bladder_previousMillis = millis();

        lcd.clear();
        lcd.setCursor(5, 1);
        lcd.print("Inflating");
        lcd.setCursor(5, 3);
        lcd.print("Time:");
        lcd.setCursor(60, 3);
        lcd.print("manual");
        lcd.setCursor(5, 5);
        lcd.print("Elapsed:");
     }
  }
  
  // If the debounce period has elapsed and the air swich is in the open poition, restart normal process at the end of the inflating position (next position is venting)

  if (Indio.digitalRead(air_switch) == LOW && inflate_override == true) {
   if ((millis() - debounce > debounce_period)) {
     inflate_override = false;

     bladder_currentMillis = millis();
     
     bladder_previousMillis = (bladder_currentMillis - bladder_inflation_time);
            
    inflating = raiseflag();

    LCDprint("Inflating", bladder_inflation_time);
   }
  }
  
}
  
  
bool raiseflag(void) {

  // Sets all solonoid flags to false, then sets chosen flag to true
  
  vacuuming = false;
  venting = false;
  resting = false;
  inflating = false;

  return true;
}

void LCDprint(const char* cycle_stage, int set_time) {

  lcd.clear();
  lcd.setCursor(5, 1);
  lcd.print(cycle_stage);
  lcd.setCursor(5, 3);
  lcd.print("Time:");
  lcd.setCursor(60, 3);
  lcd.print(set_time/1000);
  lcd.setCursor(5, 5);
  lcd.print("Elapsed:");
}
