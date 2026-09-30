#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* Uncomment to use buttons instead of encoder */
#define USE_BUTTONS

#ifndef USE_BUTTONS
#include <EncButton.h>
#endif

#define OLED_RESET     4
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(128, 32, &Wire, OLED_RESET);

/*
 * Encoder pins:
 *   A   = 2
 *   B   = 3
 *   KEY = 4
 *
 * Buttons mode:
 *   PLUS  = 13
 *   MINUS = 7
 *   OK    = 10
 */

#define INPUT_PIN_PLUS   12
#define INPUT_PIN_MINUS  7
#define INPUT_PIN_OK     10

#ifndef USE_BUTTONS
  EncButton<EB_CALLBACK, 2, 3, 4> enc;
#else
  #define BUTTON_DEBOUNCE_MS      30
  #define BUTTON_HOLD_DELAY_MS    1000
  #define BUTTON_REPEAT_on_time_ms 50

  typedef struct {
    uint8_t pin;
    bool last_raw_state;
    bool stable_state;
    uint32_t debounce_timer;

    uint32_t press_start_time;
    uint32_t repeat_timer;
  } button_t;
#endif

#define TRIGGER_PIN   5
#define OUTPUT_PIN    8

bool draw = false;

uint32_t on_time_ms = 1000;
uint32_t off_time_ms = 1000;
uint32_t *switch_time_ms = &on_time_ms;

typedef enum {
  MODE_OFF = 0,
  MODE_CYCLED,
  MODE_TRIGGER,

  MODE_COUNT
} mode_t;

typedef enum {
  SELECTED_NONE = 0,
  SELECTED_MODE,
  SELECTED_ON_TIME,
  SELECTED_OFF_TIME
} selected_param_t;

uint8_t mode = MODE_OFF;
selected_param_t param = SELECTED_NONE;

bool output = true;
bool trigger_running = false;
uint32_t millis_prev = 0;


/* -------------------------------------------------------------------------- */
/* Common control handlers                                                    */
/* -------------------------------------------------------------------------- */

void controlPlus() {
  if (param == SELECTED_ON_TIME) {
    on_time_ms += 50;
  }
  else if (param == SELECTED_OFF_TIME) {
    off_time_ms += 50;
  }
  else if (param == SELECTED_MODE) {
    if(mode < (MODE_COUNT - 1)) {
      mode += 1;

      if (mode == MODE_OFF) {
        digitalWrite(LED_BUILTIN, HIGH);
        digitalWrite(OUTPUT_PIN, HIGH);
        output = true;
      }
    }
  }

  millis_prev = millis();
  draw = true;
}

void controlMinus() {
  if (param == SELECTED_ON_TIME) {
    if (on_time_ms >= 50) {
      on_time_ms -= 50;
    }
  }
  else if (param == SELECTED_OFF_TIME) {
    if (off_time_ms >= 50) {
      off_time_ms -= 50;
    }
  }
  else if (param == SELECTED_MODE) {
    if(mode != MODE_OFF) {
      mode -= 1;

      if (mode == MODE_OFF) {
        digitalWrite(LED_BUILTIN, LOW);
        digitalWrite(OUTPUT_PIN, LOW);
        output = false;
      }
    }
  }

  millis_prev = millis();
  draw = true;
}


void controlOk() {
  if (param == SELECTED_NONE) {
    param = SELECTED_MODE;
  }
  else if(param == SELECTED_MODE) {
    param = mode == MODE_TRIGGER ? SELECTED_OFF_TIME : SELECTED_ON_TIME;
  }
  else if (param == SELECTED_ON_TIME) {
    param = SELECTED_OFF_TIME;
  }
  else if (param == SELECTED_OFF_TIME) {
    param = SELECTED_NONE;
  }

  draw = true;
}


/* -------------------------------------------------------------------------- */
/* Encoder                                                                    */
/* -------------------------------------------------------------------------- */

#ifndef USE_BUTTONS

void encRight() {
  controlPlus();
}


void encLeft() {
  controlMinus();
}


void encPress() {
  controlOk();
}

#endif


/* -------------------------------------------------------------------------- */
/* Buttons                                                                    */
/* -------------------------------------------------------------------------- */

#ifdef USE_BUTTONS

button_t button_plus = {
  INPUT_PIN_PLUS,
  HIGH,
  HIGH,
  0,
  0,
  0
};

button_t button_minus = {
  INPUT_PIN_MINUS,
  HIGH,
  HIGH,
  0,
  0,
  0
};

button_t button_ok = {
  INPUT_PIN_OK,
  HIGH,
  HIGH,
  0,
  0,
  0
};

bool buttonPressed(button_t *button) {
  bool raw_state = digitalRead(button->pin);

  if (raw_state != button->last_raw_state) {
    button->last_raw_state = raw_state;
    button->debounce_timer = millis();
  }

  if ((millis() - button->debounce_timer) >= BUTTON_DEBOUNCE_MS) {
    if (raw_state != button->stable_state) {
      button->stable_state = raw_state;

      if (button->stable_state == LOW) {
        button->press_start_time = millis();
        button->repeat_timer = millis();

        return true;
      }
      else {
        button->press_start_time = 0;
        button->repeat_timer = 0;
      }
    }
  }

  return false;
}

bool buttonRepeat(button_t *button) {
  if (button->stable_state != LOW) {
    return false;
  }

  uint32_t now = millis();

  if ((now - button->press_start_time) < BUTTON_HOLD_DELAY_MS) {
    return false;
  }

  if ((now - button->repeat_timer) >= BUTTON_REPEAT_on_time_ms) {
    button->repeat_timer = now;
    return true;
  }

  return false;
}

void buttonsTick() {
  if (buttonPressed(&button_plus)) {
    controlPlus();
  }

  if (buttonPressed(&button_minus)) {
    controlMinus();
  }

  if (buttonPressed(&button_ok)) {
    controlOk();
  }

  if (buttonRepeat(&button_plus)) {
    controlPlus();
  }

  if (buttonRepeat(&button_minus)) {
    controlMinus();
  }
}

#endif


/* -------------------------------------------------------------------------- */
/* Setup                                                                      */
/* -------------------------------------------------------------------------- */

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
  pinMode(TRIGGER_PIN, INPUT);
  
#ifdef USE_BUTTONS
  /*
   * Buttons are expected to connect pin to GND when pressed.
   */
  pinMode(INPUT_PIN_PLUS, INPUT_PULLUP);
  pinMode(INPUT_PIN_MINUS, INPUT_PULLUP);
  pinMode(INPUT_PIN_OK, INPUT_PULLUP);
#else
  enc.attach(RIGHT_HANDLER, encRight);
  enc.attach(LEFT_HANDLER, encLeft);
  enc.attach(PRESS_HANDLER, encPress);
#endif

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));

    for (;;) {
    }
  }

  display.setRotation(2);
  display.display();
  delay(1000);

  draw = true;
}


/* -------------------------------------------------------------------------- */
/* Loop                                                                       */
/* -------------------------------------------------------------------------- */
bool trigger_pin_state_prev = LOW;
bool trigger_pin_state_curr = LOW;

void loop() {
  if(mode == MODE_TRIGGER) {
    trigger_pin_state_prev = trigger_pin_state_curr;
    trigger_pin_state_curr = digitalRead(TRIGGER_PIN);

    if((trigger_pin_state_curr != trigger_pin_state_prev) && !trigger_running) {
      if(trigger_pin_state_curr == HIGH) {
        Serial.println(F("SHIT"));
        millis_prev = millis();
        trigger_running = true;

        output = false;
        switch_time_ms = &off_time_ms;
        digitalWrite(LED_BUILTIN, LOW);
        digitalWrite(OUTPUT_PIN, LOW);
      }
    }
  }

  if (mode != MODE_OFF) {
    if (millis() - millis_prev >= (*switch_time_ms)) {
      millis_prev = millis();

      if (output) {
        /* Supress OFF cyclic logic if trigger mode selected */
        if(mode != MODE_TRIGGER) {
          switch_time_ms = &off_time_ms;
          output = false;
        }
      }
      else {
        switch_time_ms = &on_time_ms;
        output = true;
        trigger_running = false;
      }

      digitalWrite(LED_BUILTIN, output);
      digitalWrite(OUTPUT_PIN, output);
    }
  }

  if (draw) {
    draw = false;

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);

    if (param == SELECTED_MODE) {
      display.print(F(">"));
    }

    display.print(F("STATE: "));
    switch(mode){
      case MODE_OFF: display.println(F("OFF")); break;
      case MODE_CYCLED: display.println(F("CYCLED")); break;
      case MODE_TRIGGER: display.println(F("TRIGGER")); break;
      default:
        display.println(F("UNKNOWN"));
        break;
    }

    if(mode != MODE_TRIGGER) {
      if (param == SELECTED_ON_TIME) {
        display.print(F(">"));
      }
      display.print(F("ON TIME:"));
      display.print(on_time_ms, DEC);
      display.println(F("ms"));
    }
    
    if (param == SELECTED_OFF_TIME) {
      display.print(F(">"));
    }
    display.print(F("OFF TIME:"));
    display.print(off_time_ms, DEC);
    display.println(F("ms"));

    display.display();
  }


#ifdef USE_BUTTONS
  buttonsTick();
#else
  enc.tick();
#endif
}
