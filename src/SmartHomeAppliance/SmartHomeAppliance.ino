// Smart Home Appliance Controller
// Arduino UNO R3 (ATmega328P)
//
// Reads temperature/humidity (DHT11) and ambient light (LDR on ADC),
// drives a DC fan through a PN2222 transistor with PWM, and reports
// status on an LCD1602, three state LEDs, an active buzzer, and UART.
// Four operating modes: AUTO, MANUAL, SLEEP, ALARM.
// Full wiring: see schematics/ and docs/hardware.md.

//--LIBRARIES--
#include <DHT11.h>          // DHT11 temperature/humidity sensor
#include <LiquidCrystal.h>  // LCD1602 in 4-bit parallel mode

//--PIN MAP & CONSTANTS--
DHT11 dht11(13);                            // DHT11 data on pin 13
LiquidCrystal lcd(A2, A3, A4, A5, 12, A1);  // LCD: RS=A2, EN=A3, D4=A4, D5=A5, D6=12, D7=A1 (RW tied to GND)
#define FAN_MOTOR_PWM 11                    // Fan driver (PN2222 base via 1k) on PWM pin 11
#define TEMP_ALARM 35                       // At/above this (C): fan OFF, buzzer + red LED (safety countermeasure)
#define TEMP_HIGH 28                        // At/above this: full fan speed
#define TEMP_MED 24                         // At/above this: medium fan speed
#define MODE_BUTTON 2                       // Cycles mode: AUTO -> MANUAL -> SLEEP (polled, debounced)
#define SPEED_BUTTON 3                      // In MANUAL: cycles fan OFF -> LOW -> MEDIUM -> FULL
#define BUZZER 6                            // Active buzzer
#define LED_BLUE 10                         // MANUAL indicator; adaptive nightlight in SLEEP (PWM)
#define LED_GREEN 8                         // AUTO indicator
#define LED_RED 7                           // ALARM indicator

//--STATE--
int temperature = 0, humidity = 0;  // Latest DHT11 readings
bool alarmAlert = false;            // True while temperature >= TEMP_ALARM
int ldrValue = 0;                   // Raw LDR reading, 0-1023
String environmentLighting = "";    // Dark / Dim / Light / Bright / Very Bright
int fanSpeed = 0;                   // PWM duty: 0 = off ... 255 = full
int modeCounter = 0;                // Mode button press counter (1=AUTO, 2=MANUAL, 3=SLEEP)
String modeState  = "AUTO";
int speedCounter = 0;               // Manual speed step (0=OFF ... 3=FULL)
int lastModeState  = HIGH;          // Previous button levels for edge detection
int lastSpeedState = HIGH;

void setup() {
  // ---------- LCD Display Module ---------------
  lcd.begin(16, 2);  // 16 columns x 2 rows
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EEL4730 - FIU");
  lcd.setCursor(0, 1);
  lcd.print("Smart Home");
  delay(2000);
  lcd.clear();

  // ---------- Serial (UART) ---------------
  Serial.begin(9600);

  // ---------- Active Buzzer Module ---------------
  pinMode(BUZZER, OUTPUT);

  // ---------- Fan Motor ---------------
  pinMode(FAN_MOTOR_PWM, OUTPUT);
  analogWrite(FAN_MOTOR_PWM, 0);  // Fan off at startup

  // ---------- Mode/Speed Buttons ---------------
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  pinMode(SPEED_BUTTON, INPUT_PULLUP);

  // ---------- State LED Indicators ---------------
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
}

void loop() {
  // ---------- Mode/Speed Buttons (polled, 20 ms debounce) ---------------
  if(digitalRead(MODE_BUTTON) == LOW && lastModeState == HIGH){
    delay(20);

    if(digitalRead(MODE_BUTTON) == LOW){
      modeCounter++;
      if(modeCounter > 3) modeCounter = 1;

      if(modeCounter == 1){
        modeState = "AUTO";
        Serial.println("MODE: AUTO");

      }else if(modeCounter == 2){
        modeState = "MANUAL";
        Serial.println("MODE: MANUAL");

      }else if(modeCounter == 3){
        modeState = "SLEEP";
        Serial.println("MODE: SLEEP");
      }
    }
  }

  if(digitalRead(SPEED_BUTTON) == LOW && lastSpeedState == HIGH){
    delay(20);

    if(digitalRead(SPEED_BUTTON) == LOW){
      if(modeState.equals("MANUAL")){
        speedCounter++;
        if(speedCounter > 3) speedCounter = 0;

        if(speedCounter == 0){
          fanSpeed = 0;
          Serial.println("Manual Fan: OFF");
        }else if(speedCounter == 1){
          fanSpeed = 85;
          Serial.println("Manual Fan: LOW");
        }else if(speedCounter == 2){
          fanSpeed = 170;
          Serial.println("Manual Fan: MEDIUM");
        }else if(speedCounter == 3){
          fanSpeed = 255;
          Serial.println("Manual Fan: FULL");
        }

        analogWrite(FAN_MOTOR_PWM, fanSpeed);

      }else{
        Serial.println("Switch to MANUAL mode first!");
      }
    }
  }

  lastModeState  = digitalRead(MODE_BUTTON);
  lastSpeedState = digitalRead(SPEED_BUTTON);

  // ---------- Temperature and Humidity Module ---------------
  int dht11Values = dht11.readTemperatureHumidity(temperature, humidity);

  if(dht11Values == 0){
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("C | Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  }else{
    Serial.println(DHT11::getErrorString(dht11Values));
  }

  // ---------- Photoresistor LDR ---------------
  ldrValue = analogRead(A0);  // LDR/10k voltage divider on A0, 0-1023

  if(ldrValue < 10){
    environmentLighting = "Dark";
  }else if(ldrValue < 200){
    environmentLighting = "Dim";
  }else if(ldrValue < 500){
    environmentLighting = "Light";
  }else if(ldrValue < 800){
    environmentLighting = "Bright";
  }else{
    environmentLighting = "Very Bright";
  }

  Serial.print("Light Detection. Environment Lighting is ");
  Serial.println(environmentLighting);
  delay(2000);  // Sampling period; also satisfies the DHT11's minimum read interval

 // ---------- Fan Motor ---------------
  if(modeState.equals("AUTO") || modeCounter == 0){
    if(temperature >= TEMP_ALARM){
      fanSpeed = 0;   // Overtemperature: fan off, alarm on
      Serial.println("Fan Motor: OFF --- ALARM!! ---");
      alarmAlert = true;
    }else if(temperature >= TEMP_HIGH){
      fanSpeed = 255; // Full speed (100% duty)
      Serial.println("Fan Motor: FULL speed");
      alarmAlert = false;
    }else if(temperature >= TEMP_MED){
      fanSpeed = 150; // Medium speed (~60% duty)
      Serial.println("Fan Motor: MEDIUM speed");
      alarmAlert = false;
    }else{
      fanSpeed = 60;  // Low speed (~24% duty)
      Serial.println("Fan Motor: LOW speed");
      alarmAlert = false;
    }
    analogWrite(FAN_MOTOR_PWM, fanSpeed);  // PWM to fan via PN2222 transistor

  }else if(modeState.equals("SLEEP")){
    analogWrite(FAN_MOTOR_PWM, 0);
  } // MANUAL: fan is controlled by the speed button handler above

  // ---------- Active Buzzer Module ---------------
  if(alarmAlert){
    digitalWrite(BUZZER, HIGH);
    Serial.println("BUZZER: ON");
  }else{
    digitalWrite(BUZZER, LOW);
  }

  // ---------- LCD Display Module ---------------
  lcd.setCursor(0, 0);
  lcd.print("T: ");
  lcd.print(temperature);
  lcd.print("C H:  ");
  lcd.print(humidity);
  lcd.print("% ");

  lcd.setCursor(0, 1);

  // ---------- State LED Indicators ---------------
  if(alarmAlert){
    digitalWrite(LED_RED, HIGH);
    lcd.print("ALARM MODE!!   ");
    modeCounter = 0;

    analogWrite(LED_BLUE, 0);
    digitalWrite(LED_GREEN, LOW);

  }else if(modeState.equals("AUTO")){
    digitalWrite(LED_GREEN, HIGH);
    lcd.print("AUTO MODE      ");

    digitalWrite(LED_RED, LOW);
    analogWrite(LED_BLUE, 0);

  }else if(modeState.equals("MANUAL")){
    analogWrite(LED_BLUE, 255);
    lcd.print("MANUAL MODE    ");

    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);

  }else if(modeState.equals("SLEEP")){
    // Adaptive nightlight: darker room -> brighter LED, bright room -> dimmer LED
    int sleepBrightness = map(ldrValue, 0, 1023, 80, 10);
    analogWrite(LED_BLUE, sleepBrightness);
    lcd.print("SLEEP MODE     ");

    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
  }

}
