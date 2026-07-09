//--LIBRARIES--
#include <DHT11.h>          //Include the DHT11 Sensor Library
#include <LiquidCrystal.h>  //Include the LCD Display Library

//--VARIABLES--
//Variables that won't change
DHT11 dht11(13);                            //Connecting the sensor DHT11 to digital I/O [PIN 13]
LiquidCrystal lcd(A2, A3, A4, A5, 12, A1);  //Connecting the LCD Display (RS=A2, EN=A3, D4=A4, D5=A5, D6=12, D7=A1)
#define FAN_MOTOR_PWM 11                    //Connecting the Fan Motor Module (PN2222) to digital PWM I/O [PIN 11]
#define TEMP_ALARM 35                       //The Fan Motor Module will turn OFF, alarm condition (countermeasure)
#define TEMP_HIGH 28                        //The Fan Motor Module will have a full speed at 28C
#define TEMP_MED 24                         //The Fan Motor Module will have a medium speed at 24C
#define MODE_BUTTON 2                       //Button Interrup: AUTO, MANUAL, and Sleep.
#define SPEED_BUTTON 3                      //Button Interrup: When MODE_BUTTON is at MANUAL, the speed of fan motor is change between OFF, LOW, MEDIUM, FULL speed
#define BUZZER 6                            //Active Buzzer
#define LED_BLUE 10                         //State LED Indicators: MANUAL | OFF in SLEEP
#define LED_GREEN 8                         //State LED Indicators: AUTO
#define LED_RED 7                           //State LED Indicators: ALARM

//Variables that will change
int temperature = 0, humidity = 0;  //DHT11 Sensor
bool alarmAlert = false;            //Temperature reach less than TEMP_ALARM
int ldrValue = 0;                   //Setting up the Photoresistor analog pin
String environmentLighting = "";    //Photoresistor lightning status: dark, dim, light, bright, or very bright
int fanSpeed = 0;                   //Fan Motor speed value: 0 = OFF, 60 = low, 150 = medium, 255 = full
int modeCounter = 0;                //Mode Button Interrup counter between states
String modeState  = "AUTO";
int speedCounter = 0;               //Speed Button Interrup counter between speeds
int lastModeState  = HIGH;
int lastSpeedState = HIGH;

void setup() {
  //Initialize serial communication to allow debugging and data readout
  // ---------- LCD Display Module ---------------
  lcd.begin(16, 2); //Set up the size of the screen, 16 columns and 2 rows
  lcd.clear(); //Clear any previuos characters from the screen
  lcd.setCursor(0, 0);
  lcd.print("EEL4730 - FIU");
  lcd.setCursor(0, 1);
  lcd.print("Smart Home");
  delay(2000);
  lcd.clear();

  // ---------- Temperature and Humidity Module ---------------
  Serial.begin(9600); //Set up serial communication in 9600 bauds
  //dht11.setDelay(500);//dht11.setDelay(2000); //Delay of 2 seconds

  // ---------- Active Buzzer Module ---------------
  pinMode(BUZZER, OUTPUT); //Active Buzzer

  // ---------- Fan Motor ---------------
  pinMode(FAN_MOTOR_PWM, OUTPUT); //Fan Motor pin is being assigned as an output
  analogWrite(FAN_MOTOR_PWM, 0); //Fan Motor is OFF at starup

  // ---------- Interrupt Buttons ---------------
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  pinMode(SPEED_BUTTON, INPUT_PULLUP);

  // ---------- State LED Indicators ---------------
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

}

void loop() {
  // ---------- Interrupt Buttons ---------------
  if(digitalRead(MODE_BUTTON) == LOW && lastModeState == HIGH){
    delay(20);

    if(digitalRead(MODE_BUTTON) == LOW){
      modeCounter++; //Button One is pressed, so modeCounter = 1
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

        analogWrite(FAN_MOTOR_PWM, fanSpeed); //Update fan

      }else{
        Serial.println("Switch to MANUAL mode first!");
      }
    }
  }

  lastModeState  = digitalRead(MODE_BUTTON);
  lastSpeedState = digitalRead(SPEED_BUTTON);

  // ---------- Temperature and Humidity Module ---------------
  //Reading temperature and humidity values from the DHT11 sensor
  int dht11Values = dht11.readTemperatureHumidity(temperature, humidity);

  //Checking the results of the readings
  if(dht11Values == 0){ //If reading is successful, print the temperature and humidity values
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print("C | Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
  }else{                //If there are errors, print the error message
    Serial.println(DHT11::getErrorString(dht11Values));
  }

  // ---------- Photoresistor LDR ---------------
  ldrValue = analogRead(A0); //Connecting the Photoresistor to analog pin [PIN A0] | Reads the input on analog, value between 0 and 1023
  //Serial.println(ldrValue); //Testing the raw analog reading
  
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

  Serial.print("Light Detection. Enviroment Lighting is ");
  Serial.println(environmentLighting);
  delay(2000); //Delay of 2 seconds

 // ---------- Fan Motor ---------------
  if(modeState.equals("AUTO") || modeCounter == 0){
    if(temperature >= TEMP_ALARM){
      fanSpeed = 0;   //Fan Motor OFF - Temperature TOO High
      Serial.println("Fan Motor: OFF --- ALARM!! ---");
      alarmAlert = true;
    }else if(temperature >= TEMP_HIGH){
      fanSpeed = 255; //Fan Motor speed is 100% PWM cycle
      Serial.println("Fan Motor: FULL speed");
      alarmAlert = false;
    }else if(temperature >= TEMP_MED){
      fanSpeed = 150; //Fan Motor speed is 60% PWM cycle
      Serial.println("Fan Motor: MEDIUM speed");
      alarmAlert = false;
    }else{
      fanSpeed = 60; //Fan Motor speed is 24% PWM cycle
      Serial.println("Fan Motor: LOW speed");
      alarmAlert = false;
    }
    analogWrite(FAN_MOTOR_PWM, fanSpeed); //Send PWM signal to fan via PN2222 transistor

  }else if(modeState.equals("SLEEP")){
    analogWrite(FAN_MOTOR_PWM, 0);
  } //ELSE: MANUAL - fan controlled by button 2
  

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

    analogWrite(LED_BLUE, 0);     //PWM: using analog write to get 0 brightness
    digitalWrite(LED_GREEN, LOW);

  }else if(modeState.equals("AUTO")){
    digitalWrite(LED_GREEN, HIGH);
    lcd.print("AUTO MODE      ");

    digitalWrite(LED_RED, LOW);
    analogWrite(LED_BLUE, 0);     //PWM: using analog write to get 0 brightness

  }else if(modeState.equals("MANUAL")){
    analogWrite(LED_BLUE, 255);   //PWM: using analog write to get full brightness
    lcd.print("MANUAL MODE    ");

    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);

  }else if(modeState.equals("SLEEP")){
    //Dark Room = Brighter LED for SLEEP Mode | Bright Room = dimmer LED for SLEEP Mode
    int sleepBrightness = map(ldrValue, 0, 1023, 80, 10);
    analogWrite(LED_BLUE, sleepBrightness);
    lcd.print("SLEEP MODE     ");
    
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
  }

}
