// SpoddyCoder Arduino LED Light PWM Controller
//
// v0.95

#include <EEPROM.h>
#include "SevSeg.h"


//////////////////////////////////////////////////////////////////////////////////
// config
//
// - hardware
#define NUM_LIGHTS 6
#define SELECT_LIGHT_PIN 2
#define SET_PATTERN_PIN 4
#define SET_BRIGHTNESS_PIN 8
#define LIGHT_PIN_1 11
#define LIGHT_PIN_2 10
#define LIGHT_PIN_3 9
#define LIGHT_PIN_4 6
#define LIGHT_PIN_5 5
#define LIGHT_PIN_6 3
#define SEVSEG_NUM_DIGITS 4
#define SEVSEG_DIGIT_PIN_1 0    // will disable serial (USB) communication
#define SEVSEG_DIGIT_PIN_2 1    // 
#define SEVSEG_DIGIT_PIN_3 7
#define SEVSEG_DIGIT_PIN_4 12
#define SEVSEG_PIN_A 13
#define SEVSEG_PIN_B A0
#define SEVSEG_PIN_C A1
#define SEVSEG_PIN_D A2
#define SEVSEG_PIN_E A3
#define SEVSEG_PIN_F A4
#define SEVSEG_PIN_G A5
#define SEVSEG_DISABLE_DECIMAL_POINT true   // NB: not enough IO pins for the decimal point
#define SEVSEG_RESISTORS_ON_SEGMENTS false
#define SEVSEG_HARDWARE_CONFIG COMMON_ANODE
// - overall pattern speed / resolution
#define PATTERN_STEPS 5000
#define PATTERN_DELAY 2 // ms
// - UX
#define WELCOME_MESSAGE "  --- yo yo yo --- happy holidays to you anjou --- love paulus                     ___and ugly light dude the"  // NB: not all chars supported by 7 seg display
#define SCROLLING_MESSAGE_DELAY 150 // ms
#define SELECT_LIGHT_FLASH_DURATION 1650  // ms
#define SELECT_LIGHT_FLASH_PULSE 150  // ms
#define SELECT_LIGHT_ROTATE_SETTINGS_DELAY 3333 // ms
#define LONG_PRESS_THRESHOLD 2000 // ms
#define INACTIVITY_THRESHOLD 15000 // ms
#define DEBOUNCE_DELAY 50 // ms
#define USER_ACTION_NONE 0
#define USER_ACTION_SELECT_LIGHT 1
#define USER_ACTION_SET_PATTERN 2
#define USER_ACTION_SET_BRIGHTNESS 3
#define USER_ACTION_SELECT_LIGHT_LONG_PRESS 4
#define USER_ACTION_SET_PATTERN_LONG_PRESS 5
#define USER_ACTION_INACTIVITY 6
#define NUM_PATTERNS 10
#define LIGHT_MAX_BRIGHTNESS 9
#define DISPLAY_NONE 0
#define DISPLAY_LIGHT 1
#define DISPLAY_PATTERN 2
#define DISPLAY_BRIGHTNESS 3
#define DISPLAY_MESSAGE 4
#define DISPLAY_RAND 5
static const float LIGHT_BRIGHTNESS_SCALE[] = {0, 0.05, 0.1, 0.15, 0.25, 0.35, 0.4, 0.6, 0.75, 1};    // normalised pwm values for "apparant brightness" on a scale of 0-9
// - EEPROM persistent state
#define RELOAD_ADDRESS 128
#define RELOAD_INDICATOR_VALUE 100
#define UNIT_SETTINGS_ADDRESS 64
#define LIGHT_SETTINGS_ADDRESS 0
struct UnitSettings {
  unsigned long on_count;
};
struct LightSettings {
  int lp_0;
  int lb_0;
  int lo_0;
  int lp_1;
  int lb_1;
  int lo_1;
  int lp_2;
  int lb_2;
  int lo_2;
  int lp_3;
  int lb_3;
  int lo_3;
  int lp_4;
  int lb_4;
  int lo_4;
  int lp_5;
  int lb_5;
  int lo_5;
};


//////////////////////////////////////////////////////////////////////////////////
// state
//
// lights
int light_pins[] = {LIGHT_PIN_1, LIGHT_PIN_2, LIGHT_PIN_3, LIGHT_PIN_4, LIGHT_PIN_5, LIGHT_PIN_6};
int light_pattern[NUM_LIGHTS];
int light_offset[NUM_LIGHTS];
int light_brightness[NUM_LIGHTS];
int selected_light = 0;
bool play_patterns = true;
unsigned int on_count = 0;
// buttons
long user_inactivity_start = 0;
bool inactive = false;
// - select button
int select_light_counter = 0;
int select_light_state = 0;
int select_light_last_state = 0;
long select_light_start = 0;
long select_light_long_press_start = 0;
// - pattern button
int set_pattern_counter = 0;
int set_pattern_state = 0;
int set_pattern_last_state = 0;
long set_pattern_long_press_start = 0;
// - brightness button
int set_brightness_counter = 0;
int set_brightness_state = 0;
int set_brightness_last_state = 0;
// - seven segment display
SevSeg sevseg;
int display_mode = 0;
String scrolling_message = "";
int scrolling_message_pos = 0;
int scrolling_message_len = 0;
bool scrolling_message_play = false;
bool select_light_rotate_settings = false;


//////////////////////////////////////////////////////////////////////////////////
// init
//
void setup() {

  // init input
  pinMode(SELECT_LIGHT_PIN, INPUT);
  pinMode(SET_PATTERN_PIN, INPUT);
  pinMode(SET_BRIGHTNESS_PIN, INPUT);

  // init SevSeg display
  byte digitPins[] = {SEVSEG_DIGIT_PIN_1, SEVSEG_DIGIT_PIN_2, SEVSEG_DIGIT_PIN_3, SEVSEG_DIGIT_PIN_4};
  byte segmentPins[] = {SEVSEG_PIN_A, SEVSEG_PIN_B, SEVSEG_PIN_C, SEVSEG_PIN_D, SEVSEG_PIN_E, SEVSEG_PIN_F, SEVSEG_PIN_G};
  bool updateWithDelays = false;
  bool leadingZeros = false;
  sevseg.begin(SEVSEG_HARDWARE_CONFIG, SEVSEG_NUM_DIGITS, digitPins, segmentPins, SEVSEG_RESISTORS_ON_SEGMENTS, updateWithDelays, leadingZeros, SEVSEG_DISABLE_DECIMAL_POINT);
  sevseg.setBrightness(90);
  
  // init lights
  randomSeed(analogRead(0));
  for(int i=0; i<NUM_LIGHTS; i++) {
    pinMode(light_pins[i], OUTPUT);
    // init state
    analogWrite(light_pins[i], 0);
    light_pattern[i] = i;                         // demo all patterns on first ever run (restored from EEPROM on all future runs)
    light_brightness[i] = LIGHT_MAX_BRIGHTNESS;   // max brighness on first ever run
    light_offset[i] = random(PATTERN_STEPS);
  }

  // reload saved settins from EEPROM
  reloadSettings();
  // count num times unit has been turned on
  on_count ++;
  saveUnitSettings();

  // init user state
  inactive = true;
  
  // start with welcome message
  scrolling_message = String("    ") + WELCOME_MESSAGE;
  scrolling_message = scrolling_message + " " + String(on_count);
  scrolling_message = scrolling_message + " " + getOrdinalSuffix(on_count) + "     ";   // pad for scrolling effect
  scrolling_message_len = scrolling_message.length();
  scrolling_message_pos = 0;
  scrolling_message_play = true;
  display_mode = DISPLAY_MESSAGE;
}

//////////////////////////////////////////////////////////////////////////////////
// controller
//
void loop() {
  float light_brightness_value = 0;
  int action = 0;
  long now;
  
  for(int t=0; t<PATTERN_STEPS; t++) {
     now = millis();
    // scrolling message
    if( t % (SCROLLING_MESSAGE_DELAY / PATTERN_DELAY) == 0 && scrolling_message_play ) {
      scrolling_message_pos = scrolling_message_pos + 1;
      if( scrolling_message_pos > (scrolling_message_len + SEVSEG_NUM_DIGITS) ) {
        scrolling_message_play = false;
        display_mode = DISPLAY_NONE;
      }
    }
    // rotate through select light settings
    if( select_light_start != 0 && now > (select_light_start + SELECT_LIGHT_ROTATE_SETTINGS_DELAY) && display_mode == DISPLAY_LIGHT ) {
      display_mode = DISPLAY_PATTERN;
    }
    if( select_light_start != 0 && now > (select_light_start + (SELECT_LIGHT_ROTATE_SETTINGS_DELAY * 2)) && display_mode == DISPLAY_PATTERN ) {
      display_mode = DISPLAY_BRIGHTNESS;
    }
    if( select_light_start != 0 && now > (select_light_start + (SELECT_LIGHT_ROTATE_SETTINGS_DELAY * 3)) && display_mode == DISPLAY_BRIGHTNESS ) {
      display_mode = DISPLAY_LIGHT;
      select_light_start = 0;
    }
    // get user input and update lights + display
    updateSevSegDisplay();
    action = getInput();
    doAction(action);
    for(int i=0; i<NUM_LIGHTS; i++) {
      // lookup pattern value
      light_brightness_value = getPatternValue( t, i );
      // apply user selected overall brightness & convert to duty cycle
      light_brightness_value = abs(light_brightness_value) * LIGHT_BRIGHTNESS_SCALE[light_brightness[i]] * 255;
      // write duty cycle to pin
      if (play_patterns || i == selected_light) {
        analogWrite(light_pins[i], int(light_brightness_value));
      }
    }
    sevSegSafeDelay(PATTERN_DELAY);
  }
}

void doAction (int action) {
  if (action > 0 ) {
    turnOffAllLights();
  }
  switch(action) {
    case USER_ACTION_SELECT_LIGHT:
        if( ! inactive ) {
          selected_light ++;
          if (selected_light >= NUM_LIGHTS) {
            selected_light = 0;
          }
        }
        inactive = false;
        display_mode = DISPLAY_LIGHT;
        select_light_rotate_settings = true;
        updateSevSegDisplay();
        flashLight(selected_light);   // this is ugly... and leads to the code smell seen in the flashLight() function... flashing should be in the main loop, TODO
        break;
    case USER_ACTION_SET_PATTERN:
        if ( ! inactive ) {
          light_pattern[selected_light] ++;
          if (light_pattern[selected_light] >= NUM_PATTERNS) {
            light_pattern[selected_light] = 0;
          }
        }
        display_mode = DISPLAY_PATTERN;
        select_light_start = 0;
        updateSevSegDisplay();
        saveLightSettings();
        break;
    case USER_ACTION_SET_BRIGHTNESS:
        if ( ! inactive ) {
          light_brightness[selected_light] --;
          if (light_brightness[selected_light] < 0) {
            light_brightness[selected_light] = LIGHT_MAX_BRIGHTNESS;
          }
        }
        display_mode = DISPLAY_BRIGHTNESS;
        select_light_start = 0;
        updateSevSegDisplay();
        saveLightSettings();
        break;
    case USER_ACTION_SELECT_LIGHT_LONG_PRESS:
        turnOffAllLights();
        play_patterns = ! play_patterns;
        display_mode = DISPLAY_LIGHT;
        updateSevSegDisplay();
        break;
    case USER_ACTION_SET_PATTERN_LONG_PRESS:
        randomiseOffsets();
        display_mode = DISPLAY_RAND;
        updateSevSegDisplay();
        break;
    case USER_ACTION_INACTIVITY:
        inactive = true;
        display_mode = DISPLAY_NONE;
        updateSevSegDisplay();
        break;
  }
}

//////////////////////////////////////////////////////////////////////////////////
// input
//
int getInput() {
  int action = USER_ACTION_NONE;
  long now = millis();
  
  // select light button
  select_light_state = digitalRead(SELECT_LIGHT_PIN);
  if ( select_light_long_press_start != 0 && now - select_light_long_press_start > LONG_PRESS_THRESHOLD ) {
    select_light_long_press_start = 0;
    action = USER_ACTION_SELECT_LIGHT_LONG_PRESS;
  }
  if ( select_light_state != select_light_last_state ) {
    if (select_light_state == HIGH) { // low to high transition
      select_light_counter++;
      select_light_start = now;
      select_light_long_press_start = now;
      action = USER_ACTION_SELECT_LIGHT;
    } else {                          // high to low transition
      select_light_long_press_start = 0;
    }
    sevSegSafeDelay(DEBOUNCE_DELAY);
  }
  select_light_last_state = select_light_state;
  
  // set pattern button
  set_pattern_state = digitalRead(SET_PATTERN_PIN);
  if ( set_pattern_long_press_start != 0 && now - set_pattern_long_press_start > LONG_PRESS_THRESHOLD ) {
    set_pattern_long_press_start = 0;
    action = USER_ACTION_SET_PATTERN_LONG_PRESS;
  }
  if ( set_pattern_state != set_pattern_last_state ) {
    if (set_pattern_state == HIGH) {
      set_pattern_counter++;
      set_pattern_long_press_start = now;
      action = USER_ACTION_SET_PATTERN;
    } else {
      set_pattern_long_press_start = 0;
    }
    sevSegSafeDelay(DEBOUNCE_DELAY);
  }
  set_pattern_last_state = set_pattern_state;
  
  // set brightness button
  set_brightness_state = digitalRead(SET_BRIGHTNESS_PIN);
  if ( set_brightness_state != set_brightness_last_state ) {
    if (set_brightness_state == HIGH) {
      set_brightness_counter++;
      action = USER_ACTION_SET_BRIGHTNESS;
    }
    sevSegSafeDelay(DEBOUNCE_DELAY);
  }
  set_brightness_last_state = set_brightness_state;

  // user inactivity
  if( action != USER_ACTION_NONE ) {    // reset counter if activity
    user_inactivity_start = 0;
  }
  if( action == USER_ACTION_NONE && user_inactivity_start == 0 && ! scrolling_message_play) {  // start counter if no activity (NB: scrolling message counts as user activity)
    user_inactivity_start = millis();
  }
  if( action == USER_ACTION_NONE && user_inactivity_start != 0 && (now - user_inactivity_start > INACTIVITY_THRESHOLD) ) {    // fire action if inactive
    user_inactivity_start = 0;
    action = USER_ACTION_INACTIVITY;
  }

  // return the action
  return action;
}

//////////////////////////////////////////////////////////////////////////////////
// output
//
void updateSevSegDisplay() {
  String msg = "";
  int light = selected_light + 1;
  int pattern = light_pattern[selected_light] + 1;
  int brightness = light_brightness[selected_light];
  switch( display_mode ) {
    case DISPLAY_MESSAGE:
        msg = scrolling_message.substring(scrolling_message_pos, scrolling_message_pos + SEVSEG_NUM_DIGITS);
        break;
    case DISPLAY_LIGHT:
        msg = String("LI ") + light;
        break;
    case DISPLAY_PATTERN:
        char display_pattern = "0123456789ABCDEF"[pattern]; // we only have 1 7-seg display to print the pattern number
        msg = String("PA ") + display_pattern;
        break;
    case DISPLAY_BRIGHTNESS:
        msg = String("BR ") + brightness;
        break;
    case DISPLAY_RAND:
        msg = String("RAND");
        break; 
    case DISPLAY_NONE:
    default:
        msg = "";
        break;
  }
  setSevSegChars(msg);
}

void setSevSegChars(String msg) {
  String display_msg;
  char display_chars[SEVSEG_NUM_DIGITS+1];
  display_msg = msg.substring(0, 4);
  display_msg.toCharArray(display_chars, 5);
  sevseg.setChars(display_chars);
}

void sevSegSafeDelay(int delay_ms) {
  for(int i=0; i<delay_ms; i++) {
    sevseg.refreshDisplay();
    delay(1);
  }
}

String getOrdinalSuffix(unsigned long num) {
  switch(num) {
    case 10:
    case 11:
    case 12:
    case 13:
      return "th";
      break;
    default:
      switch(num % 10) {
        case 1: 
        return "st";
          break;
        case 2:
          return "nd";
          case 3:
          return "rd";
        default:
          return "th";
          break;
      }
  }
}

void turnOffAllLights() {
  for(int i=0; i<NUM_LIGHTS; i++) {
    analogWrite(light_pins[i], 0);
  }
}

void randomiseOffsets() {
  for(int i=0; i<NUM_LIGHTS; i++) {
    light_offset[i] = random(PATTERN_STEPS);     // apply random pattern offset to to each light - so they're not perfectly in sync
  }
}

void flashLight(int light_num) {
  int action = 0;
  int modulo = SELECT_LIGHT_FLASH_DURATION / SELECT_LIGHT_FLASH_PULSE;
  for(int i=0; i<SELECT_LIGHT_FLASH_DURATION; i++) {
    // still listen for user input & refresh SevSeg while flashing
    updateSevSegDisplay();
    sevseg.refreshDisplay();
    action = getInput();
    if( action > 0 ) {
      doAction(action);
      break;  // avoid an infinite recursive loop - ugly!
    }
    if ( i / SELECT_LIGHT_FLASH_PULSE % 2 == 0 ) {
      analogWrite(light_pins[selected_light], 255);
      if( display_mode == DISPLAY_NONE ) {
        display_mode = DISPLAY_LIGHT;
      }
    } else {
      analogWrite(light_pins[selected_light], 0);
      if( display_mode == DISPLAY_LIGHT ) {
        display_mode = DISPLAY_NONE;
      }
    }
    sevSegSafeDelay(1);
  }
  if( display_mode == DISPLAY_NONE ) {
    display_mode = DISPLAY_LIGHT;   // leave light display on after flashing it
  }
}

float getPatternValue( int t, int light_num ) {
  t = t + light_offset[light_num]; // apply the random pattern offset
  float rads = 0;
  float value = 0;
  int rando = 0;
  switch ( light_pattern[light_num] ) {
    case 0:
        // slow breathing
        rads = t * (PI / PATTERN_STEPS);
        value = sin(rads);
        break;
    case 1:
        // medium breathing
        rads = (t * 2) * (PI / PATTERN_STEPS);
        value = sin(rads);
        break;
    case 2:
        // fast breathing
        rads = (t * 4) * (PI / PATTERN_STEPS);
        value = sin(rads);
        break;
    case 3:
        // slow flash
        value = ( int(((t * 2)/(PATTERN_STEPS/2))) % 2 == 0 ) ? 0 : 1;
        break;
    case 4:
        // fast flash
        value = ( int(((t * 4)/(PATTERN_STEPS/5))) % 2 == 0 ) ? 0 : 1;
        break;
    case 5:
        // slow random flash
        rando = random(2);
        value = ( int((t/(PATTERN_STEPS/2.37 + (rando/10))) + (t*2/(PATTERN_STEPS/5))) % 2 == 0 ) ? 0 : 1;
        break;
    case 6:
        // fast random flash
        value = ( int(((t * 3)/(PATTERN_STEPS/1.37)) + ((t * 3)/(PATTERN_STEPS/7)) - ((t * 3)/(PATTERN_STEPS/16.7836746))) % 5 == 0 ) ? 1 : 0;
        break;
    case 7:
        // spooky flash
        rando = random(5);
        value = ( int((t/(PATTERN_STEPS/(2+rando))) + (t/(PATTERN_STEPS/(5+rando)))) % 2 == 0 ) ? 0 : 1;
        break;
    case 8:
        // always on
        value = 1;
        break;
    case 9:
        // flamey flicker
        value = ( int(((t * 3)/(PATTERN_STEPS/1.37)) + ((t * 3)/(PATTERN_STEPS/7)) - ((t * 3)/(PATTERN_STEPS/16.7836746))) % 5 == 0 ) ? 0 : 1;
        if ( value < 0.5 ) {
          rando = random(500,1000);
          value = float(rando) / 1000;
        } else {
          rando = random(950,1000);
          value = float(rando) / 1000;
        }
        break;
    default:
        // always off
        value = 0;
        break;
  }
  return value * value * value * value; // account for LED "apparrant brightness"
}

//////////////////////////////////////////////////////////////////////////////////
// storage
//
void reloadSettings() {
  UnitSettings unit_settings;
  LightSettings light_settings;
  int reload = EEPROM.read(RELOAD_ADDRESS);
  if (reload == RELOAD_INDICATOR_VALUE) {
    EEPROM.get(UNIT_SETTINGS_ADDRESS, unit_settings);
    on_count = unit_settings.on_count;
    EEPROM.get(LIGHT_SETTINGS_ADDRESS, light_settings);
    light_brightness[0] = light_settings.lb_0;
    light_brightness[1] = light_settings.lb_1;
    light_brightness[2] = light_settings.lb_2;
    light_brightness[3] = light_settings.lb_3;
    light_brightness[4] = light_settings.lb_4;
    light_brightness[5] = light_settings.lb_5;
    light_pattern[0] = light_settings.lp_0;
    light_pattern[1] = light_settings.lp_1;
    light_pattern[2] = light_settings.lp_2;
    light_pattern[3] = light_settings.lp_3;
    light_pattern[4] = light_settings.lp_4;
    light_pattern[5] = light_settings.lp_5;
    light_offset[0] = light_settings.lo_0;
    light_offset[1] = light_settings.lo_1;
    light_offset[2] = light_settings.lo_2;
    light_offset[3] = light_settings.lo_3;
    light_offset[4] = light_settings.lo_4;
    light_offset[5] = light_settings.lo_5;
  }
}

void saveLightSettings() {
  LightSettings light_settings;
  EEPROM.get(LIGHT_SETTINGS_ADDRESS, light_settings);
  light_settings.lb_0 = light_brightness[0];
  light_settings.lb_1 = light_brightness[1];
  light_settings.lb_2 = light_brightness[2];
  light_settings.lb_3 = light_brightness[3];
  light_settings.lb_4 = light_brightness[4];
  light_settings.lb_5 = light_brightness[5];
  light_settings.lp_0 = light_pattern[0];
  light_settings.lp_1 = light_pattern[1];
  light_settings.lp_2 = light_pattern[2];
  light_settings.lp_3 = light_pattern[3];
  light_settings.lp_4 = light_pattern[4];
  light_settings.lp_5 = light_pattern[5];
  light_settings.lo_0 = light_offset[0];
  light_settings.lo_1 = light_offset[1];
  light_settings.lo_2 = light_offset[2];
  light_settings.lo_3 = light_offset[3];
  light_settings.lo_4 = light_offset[4];
  light_settings.lo_5 = light_offset[5];
  EEPROM.put(LIGHT_SETTINGS_ADDRESS, light_settings);
  EEPROM.update(RELOAD_ADDRESS, RELOAD_INDICATOR_VALUE);
}

void saveUnitSettings() {
  UnitSettings unit_settings;
  EEPROM.get(UNIT_SETTINGS_ADDRESS, unit_settings);
  unit_settings.on_count = on_count;
  EEPROM.put(UNIT_SETTINGS_ADDRESS, unit_settings);
}