#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EncButton.h>

#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(128, 32, &Wire, OLED_RESET);

EncButton<EB_CALLBACK, 2, 3, 4> enc;   // энкодер с кнопкой <A, B, KEY>

#define OUTPUT_PIN  (8)

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

void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(OUTPUT_PIN, OUTPUT);

  enc.attach(RIGHT_HANDLER, encRight);
  enc.attach(LEFT_HANDLER, encLeft);
  enc.attach(PRESS_HANDLER, encPress);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(1000);
  draw = true;
}

void encRight() {
  if(param == SELECTED_PERIOD){
    period_ms += 100;
  }
  else if(param == SELECTED_PAUSE){
    pause_ms += 100;
  }
  else if(param == SELECTED_STATE){
    output_enabled = true;
    output = true;
    output_timer = pause_ms;
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(OUTPUT_PIN, HIGH);
  }

  draw = true;
}

void encLeft() {
  if(param == SELECTED_PERIOD){
    if(period_ms >= 100){
      period_ms -= 100;
    }
  }
  else if(param == SELECTED_PAUSE){
    if(pause_ms >= 100){
      pause_ms -= 100;
    }
  }
  else if(param == SELECTED_STATE){
    output_enabled = false;
    output = false;
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(OUTPUT_PIN, LOW);
  }

  draw = true;
}

void encPress() {
  if(param == SELECTED_NONE){
    param = SELECTED_PERIOD;
  }
  else if(param == SELECTED_PERIOD){
    param = SELECTED_PAUSE;
  }
  else if(param == SELECTED_PAUSE){
    param = SELECTED_STATE;
  }
  else if(param == SELECTED_STATE){
    param = SELECTED_NONE;
  }
  
  draw = true;
}

void loop() {
  if(millis() - millis_prev >= 100){
    millis_prev = millis();
    
    if(output_enabled){
      if(output){
          if(output_timer > 100){
            output_timer -= 100;
          }
          else{
            output = false;
            digitalWrite(LED_BUILTIN, LOW);
            digitalWrite(OUTPUT_PIN, LOW);
            output_timer = period_ms;
          }
      }
      else{
        if(output_timer > 100){
          output_timer -= 100;
        }
        else{
          output = true;
          digitalWrite(LED_BUILTIN, HIGH);
          digitalWrite(OUTPUT_PIN, HIGH);
          output_timer = pause_ms;
        }
      }
    }
  }
  
  if(draw){
    draw = false;
    display.clearDisplay();
    display.setTextSize(1);             
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    if(param == SELECTED_PERIOD) display.print(F(">")); display.print(F("Period:")); display.print(period_ms, DEC); display.println(F("ms"));
    if(param == SELECTED_PAUSE) display.print(F(">")); display.print(F("Pause:")); display.print(pause_ms, DEC); display.println(F("ms"));
    if(param == SELECTED_STATE) display.print(F(">")); if(output_enabled) display.print(F("State: On")); else display.println(F("State: Off"));
    display.display();
  }

  enc.tick();
}
