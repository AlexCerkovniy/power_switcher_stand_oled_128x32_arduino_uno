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

#define INPUT_PIN_PLUS   13
#define INPUT_PIN_MINUS  7
#define INPUT_PIN_OK     10

#ifndef USE_BUTTONS
EncButton<EB_CALLBACK, 2, 3, 4> enc;
#else
typedef struct {
  uint8_t pin;
  bool last_raw_state;
  bool stable_state;
  uint32_t debounce_timer;
} button_t;
#endif

#define OUTPUT_PIN 8

bool draw = false;

uint16_t period_ms = 12000;
uint16_t pause_ms = 1000;

typedef enum {
  SELECTED_NONE = 0,
  SELECTED_PERIOD,
  SELECTED_PAUSE,
  SELECTED_STATE
} selected_param_t;

selected_param_t param = SELECTED_NONE;

bool output = false;
uint32_t output_timer = period_ms;
bool output_enabled = true;

uint32_t millis_prev = 0;


/* -------------------------------------------------------------------------- */
/* Common control handlers                                                    */
/* -------------------------------------------------------------------------- */

void controlPlus() {
  if (param == SELECTED_PERIOD) {
    period_ms += 100;
  }
  else if (param == SELECTED_PAUSE) {
    pause_ms += 100;
  }
  else if (param == SELECTED_STATE) {
    output_enabled = true;
    output = true;
    output_timer = pause_ms;

    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(OUTPUT_PIN, HIGH);
  }

  draw = true;
}


void controlMinus() {
  if (param == SELECTED_PERIOD) {
    if (period_ms >= 100) {
      period_ms -= 100;
    }
  }
  else if (param == SELECTED_PAUSE) {
    if (pause_ms >= 100) {
      pause_ms -= 100;
    }
  }
  else if (param == SELECTED_STATE) {
    output_enabled = false;
    output = false;

    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(OUTPUT_PIN, LOW);
  }

  draw = true;
}


void controlOk() {
  if (param == SELECTED_NONE) {
    param = SELECTED_PERIOD;
  }
  else if (param == SELECTED_PERIOD) {
    param = SELECTED_PAUSE;
  }
  else if (param == SELECTED_PAUSE) {
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

#define BUTTON_DEBOUNCE_MS 30

button_t button_plus = {
  INPUT_PIN_PLUS,
  HIGH,
  HIGH,
  0
};

button_t button_minus = {
  INPUT_PIN_MINUS,
  HIGH,
  HIGH,
  0
};

button_t button_ok = {
  INPUT_PIN_OK,
  HIGH,
  HIGH,
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

      /*
       * INPUT_PULLUP:
       * LOW = button pressed
       */
      if (button->stable_state == LOW) {
        return true;
      }
    }
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
  if (millis() - millis_prev >= 100) {
    millis_prev = millis();

    if (output_enabled) {
      if (output) {
        if (output_timer > 100) {
          output_timer -= 100;
        }
        else {
          output = false;

          digitalWrite(LED_BUILTIN, LOW);
          digitalWrite(OUTPUT_PIN, LOW);

          output_timer = period_ms;
        }
      }
      else {
        if (output_timer > 100) {
          output_timer -= 100;
        }
        else {
          output = true;

          digitalWrite(LED_BUILTIN, HIGH);
          digitalWrite(OUTPUT_PIN, HIGH);

          output_timer = pause_ms;
        }
      }
    }
  }


  if (draw) {
    draw = false;

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);

    if (param == SELECTED_PERIOD) {
      display.print(F(">"));
    }

    display.print(F("Period:"));
    display.print(period_ms, DEC);
    display.println(F("ms"));


    if (param == SELECTED_PAUSE) {
      display.print(F(">"));
    }

    display.print(F("Pause:"));
    display.print(pause_ms, DEC);
    display.println(F("ms"));


    if (param == SELECTED_STATE) {
      display.print(F(">"));
    }

    if (output_enabled) {
      display.println(F("State: On"));
    }
    else {
      display.println(F("State: Off"));
    }

    display.display();
  }


#ifdef USE_BUTTONS
  buttonsTick();
#else
  enc.tick();
#endif
}
