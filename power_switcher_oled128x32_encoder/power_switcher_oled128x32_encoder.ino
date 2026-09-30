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

#define OUTPUT_PIN 8

bool draw = false;

uint32_t on_time_ms = 1000;
uint32_t off_time_ms = 1000;
uint32_t *switch_time_ms = &on_time_ms;

typedef enum {
  SELECTED_NONE = 0,
  SELECTED_ON_TIME,
  SELECTED_OFF_TIME,
  SELECTED_STATE
} selected_param_t;

selected_param_t param = SELECTED_NONE;

bool output = true;
bool output_enabled = true;

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
  else if (param == SELECTED_STATE) {
    output_enabled = true;
    output = true;

    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(OUTPUT_PIN, HIGH);
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
  else if (param == SELECTED_STATE) {
    output_enabled = false;
    output = false;

    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(OUTPUT_PIN, LOW);
  }

  millis_prev = millis();
  draw = true;
}


void controlOk() {
  if (param == SELECTED_NONE) {
    param = SELECTED_ON_TIME;
  }
  else if (param == SELECTED_ON_TIME) {
    param = SELECTED_OFF_TIME;
  }
  else if (param == SELECTED_OFF_TIME) {
    param = SELECTED_STATE;
  }
  else if (param == SELECTED_STATE) {
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

void loop() {
  if (output_enabled) {
    if (millis() - millis_prev >= (*switch_time_ms)) {
      millis_prev = millis();

      if (output) {
        switch_time_ms = &off_time_ms;
        output = false;
      }
      else {
        switch_time_ms = &on_time_ms;
        output = true;
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

    if (param == SELECTED_ON_TIME) {
      display.print(F(">"));
    }

    display.print(F("ON TIME:"));
    display.print(on_time_ms, DEC);
    display.println(F("ms"));


    if (param == SELECTED_OFF_TIME) {
      display.print(F(">"));
    }

    display.print(F("OFF TIME:"));
    display.print(off_time_ms, DEC);
    display.println(F("ms"));


    if (param == SELECTED_STATE) {
      display.print(F(">"));
    }

    if (output_enabled) {
      display.println(F("STATE: On"));
    }
    else {
      display.println(F("STATE: Off"));
    }

    display.display();
  }


#ifdef USE_BUTTONS
  buttonsTick();
#else
  enc.tick();
#endif
}
