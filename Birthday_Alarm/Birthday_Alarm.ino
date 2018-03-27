//==============================================================================//
//                                                                              //
//   BIRTHDAY ALARM                                                             //
//   Author : Vishnu M Aiea                                                     //
//   Website : www.vishnumaiea.in                                               //
//   Date created : IST 7:39 PM 01-01-2018, Monday                              //
//   Last updated : 1:12 AM 04-01-2018, Thursday                                //
//   Version : 7                                                                //
//                                                                              //
//==============================================================================//

#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <Wire.h>

//==============================================================================//
//Global variables and defines

#define ALARM_WAKEUP_PIN 2
#define TIME_SET_PIN 3
#define LED_PIN 13

#define SC 0x00
// #define MN 0x01
// #define HR 0x02
// #define DT 0x03
// #define MO 0x04
// #define YR 0x05
// #define DW 0x06

#define SCA 0x0C
// #define MNA 0x0D
// #define HRA 0x0E
// #define DTA 0x0F
// #define MOA 0x10
// #define DWA 0x11

String incomingString;
byte yearValue, monthValue, dateValue, hourValue, minuteValue, secondValue, periodValue;

bool isTimeSet = false;
bool isAlarmSet = false;
bool isSerialEstablished = false;

volatile bool isAlarmInterrupt = false;
volatile bool isTimeUpdateInterrupt = false;

//==============================================================================//
//runs once at the startup

void setup(void) {
  for (byte i = 0; i <= A7; i++) { //set all pins as outputs/inputs and LOW (least current consumption)
    pinMode (i, INPUT); //INPUT and OUTPUT has the same effect
    digitalWrite (i, LOW);
  }

  MCUSR = 0; //this is the proper way to disable WDT
  wdt_disable( );

  pinMode(ALARM_WAKEUP_PIN, INPUT_PULLUP); //setup interrupt pin
  pinMode(TIME_SET_PIN, INPUT_PULLUP); //time setting pin
  pinMode(LED_PIN, OUTPUT); //set pin 13 as an output so we can use LED to monitor
  digitalWrite(LED_PIN, HIGH);  //turn pin 13 LED on

  Wire.begin(); //begin I2C
  delay(100);

  Wire.beginTransmission(0x6F);
  Wire.write(0x07); //status register
  Wire.write(0x90); //SR register (ARST = 1, WRTC = 1)
//  Wire.write(0xE0); //INT register (IM = 1, ALME = 1, LPMOD = 1)
  Wire.write(0xC9); //INT register (IM = 1, ALME = 1, LPMOD = 0)
  Wire.endTransmission();

  //the following is to reset the power failure bit RTCF
  Wire.beginTransmission(0x6F);
  Wire.write(0x00);
  Wire.write(0x01);
  Wire.endTransmission();
}

//==============================================================================//
//main loop

void loop(void) {
  if((!isTimeSet) || (!isAlarmSet)) { //set time at startup
    fetchTime();
  }

  if(isTimeSet && isAlarmSet) { //go to sleep if all set
    delay(6000);
    sleepNow();
  }
}

//==============================================================================//
//puts the MCU into sleep mode by disabling all peripherals

void sleepNow(void) {
  endSerial(); //end serial connection if active

  attachInterrupt(digitalPinToInterrupt(ALARM_WAKEUP_PIN), alarmInterrupt, LOW); //INT0 and 1 interrupts
  attachInterrupt(digitalPinToInterrupt(TIME_SET_PIN), timeUpdateInterrupt, LOW);
  delay(100);

  noInterrupts (); //temporarily disable interrupts

  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Choose our preferred sleep mode:
  sleep_enable(); // Set sleep enable (SE) bit:

  ADCSRA = 0; //disable ADC
  power_all_disable(); //disbales all modules

  digitalWrite(LED_PIN, LOW);  //turn LED off to indicate sleep

  interrupts(); //re-enable interrupts
  sleep_mode(); //goes to sleep

  //----------------------------------------//
  sleep_disable(); //Upon waking up, sketch continues from this point.

  power_timer0_enable(); //enable the required modules only
  power_twi_enable();
  power_usart0_enable();

  //----------------------------------------//
  if(isTimeUpdateInterrupt) { //if you need to update the time and alarm
    blinkLED(); //blinks LED 3 times
    isTimeSet = false;
    isAlarmSet = false;
    establishSerial();
    delay(500);
    Serial.println(F("Time update wake up."));
    fetchTime();
  }

  else if(isAlarmInterrupt) {
    blinkLED();
    blinkLED();
    establishSerial();
    delay(500);

    printTime();
    Serial.println(F("Tada! It's your birthday! Happy B'Day Vishnu :)")); //replace with your name

    blinkLED(); //blink LEDs
    delay(1000);
    blinkLED();
    delay(1000);
    blinkLED();
    delay(1000);
    blinkLED();
    delay(1000);
    blinkLED();
    delay(1000);
    blinkLED();
    delay(1000);
    blinkLED();
    delay(1000);
    blinkLED();
    delay(1000);
    blinkLED();
    delay(1000);
    blinkLED();
    Serial.println(F("See you on your next birthday! TC, Bye!"));
    Serial.println(F("Sleeping in 6 seconds.."));
  }

  loop();
  //----------------------------------------//
}

//==============================================================================//
//alarm interrupt ISR

void alarmInterrupt(void) {
  detachInterrupt(digitalPinToInterrupt(ALARM_WAKEUP_PIN)); //detach interrupts until next sleep
  detachInterrupt(digitalPinToInterrupt(TIME_SET_PIN));
  isAlarmInterrupt = true; //set the type of alarmInterrupt
  isTimeUpdateInterrupt = false;
}

//==============================================================================//
//time update interrupt ISR

void timeUpdateInterrupt(void) {
  detachInterrupt(digitalPinToInterrupt(TIME_SET_PIN)); //detach interrupts until next sleep
  detachInterrupt(digitalPinToInterrupt(ALARM_WAKEUP_PIN));
  isAlarmInterrupt = false; //set the type of interrupt
  isTimeUpdateInterrupt = true;
}

//==============================================================================//
//fetches time and alarm from serial monitor and update the RTC registers

void fetchTime() {
  while (digitalRead(TIME_SET_PIN) == HIGH); //wait for time update pin to be activated
  delay(300);

  if(digitalRead(TIME_SET_PIN) == LOW) { //if to update the time
    establishSerial();
    Serial.println(F("Ready to update time."));

    while ((isTimeSet == false) || (isAlarmSet == false)) { //must set alarm and time
      if(Serial.available() > 0) {
        incomingString = Serial.readStringUntil('#'); //# is the delimiter

        if(incomingString.equals("t")) { //prints the current time to the console
          printTime();
          Serial.flush();
          continue;
        }

        else if(incomingString.equals("a")) { //prints the current alarm time to the console
          printAlarmTime();
          Serial.flush();
          continue;
        }

        else if(incomingString.equals("c")) { //cancel the time update operation and go to sleep
          isTimeSet = true;
          isAlarmSet = true;
          Serial.flush();
        }

        else if((incomingString.length() != 14) && (incomingString.length() != 12)) { //chek if time inputs are valid
          Serial.flush();
          Serial.print(F("Invalid time input - "));
          Serial.print(incomingString);
          Serial.print(F(", "));
          Serial.println(incomingString.length());
          continue;
        }

        else {
          //Time format is : T1712241030421#
          if(incomingString.charAt(0) == 'T') { //update time register
            Serial.println();
            Serial.print(F("Time update received = "));
            Serial.println(incomingString);
            incomingString.remove(0,1); //remove 'T'
            incomingString.remove(14); //remove delimiter

            yearValue = byte((incomingString.substring(0, 2)).toInt()); //convert string values to decimals
            monthValue = byte((incomingString.substring(2, 4)).toInt());
            dateValue = byte((incomingString.substring(4, 6)).toInt());
            hourValue = byte((incomingString.substring(6, 8)).toInt());
            minuteValue = byte((incomingString.substring(8, 10)).toInt());
            secondValue = byte((incomingString.substring(10, 12)).toInt());
            periodValue = byte((incomingString.substring(12)).toInt());

            Serial.print(F("Date and Time is "));
            Serial.print(hourValue);
            Serial.print(F(":"));
            Serial.print(minuteValue);
            Serial.print(F(":"));
            Serial.print(secondValue);
            Serial.print(F(" "));

            if(periodValue == 1) Serial.print(F("PM, "));
            else Serial.print(F("AM, "));

            Serial.print(dateValue);
            Serial.print(F("-"));
            Serial.print(monthValue);
            Serial.print(F("-"));
            Serial.println(yearValue);

            //write to time register
            Wire.beginTransmission(0x6F); //send the I2C address of RTC
            Wire.write(SC); //starting address of time register
            Wire.write(decToBcd(secondValue)); //convert the dec value to BCD ans send
            Wire.write(decToBcd(minuteValue));

            hourValue = decToBcd(hourValue); //convert to BCD
            if(periodValue == 1) hourValue |= B00100000; //if PM (1 = PM)
            else hourValue &= B00011111; //if AM (0 = AM)
            Wire.write(hourValue); //write the modified hour value with AM/PM

            Wire.write(decToBcd(dateValue));
            Wire.write(decToBcd(monthValue));
            Wire.write(decToBcd(yearValue));
            Wire.endTransmission();

            isTimeSet = true;
          }

          //Alarm time format is : A12241030421#
          if(incomingString.charAt(0) == 'A') { //update alarm register
            Serial.println();
            Serial.print(F("Alarm update received = "));
            Serial.println(incomingString);
            incomingString.remove(0,1); //remove 'A'
            incomingString.remove(12); //remove delimiter #

            monthValue = byte((incomingString.substring(0, 2)).toInt()); //convert string values to decimals
            dateValue = byte((incomingString.substring(2, 4)).toInt());
            hourValue = byte((incomingString.substring(4, 6)).toInt());
            minuteValue = byte((incomingString.substring(6, 8)).toInt());
            secondValue = byte((incomingString.substring(8, 10)).toInt());
            periodValue = byte((incomingString.substring(10)).toInt());

            Serial.print(F("Alarm Date and Time is "));
            Serial.print(hourValue);
            Serial.print(F(":"));
            Serial.print(minuteValue);
            Serial.print(F(":"));
            Serial.print(secondValue);
            Serial.print(F(" "));

            if(periodValue == 1) Serial.print(F("PM, "));
            else Serial.print(F("AM, "));

            Serial.print(dateValue);
            Serial.print(F("-"));
            Serial.print(monthValue);
            Serial.println(F(" Every year"));

            //write to alarm register
            Wire.beginTransmission(0x6F);
            Wire.write(SCA); //alarm seconds register
            Wire.write((B10000000 | (decToBcd(secondValue)))); //the OR operation is required to enable the alarm register
            Wire.write((B10000000 | (decToBcd(minuteValue))));

            hourValue = decToBcd(hourValue); //convert to BCD
            if(periodValue == 1) hourValue |= B00100000; //if PM (1 = PM)
            else hourValue &= B00011111; //if AM (0 = AM)
            Wire.write((B10000000 | hourValue)); //write the modified hour value with AM/PM

            Wire.write((B10000000 | (decToBcd(dateValue))));
            Wire.write((B10000000 | (decToBcd(monthValue))));
            Wire.endTransmission();

            isAlarmSet = true;
          }
        }
      }
    }

    if(isTimeSet && isAlarmSet) {
      Serial.println();
      Serial.println(F("Everything's set. Please disable the time set pin now."));
      while(digitalRead(TIME_SET_PIN) == LOW); //wait for the time update pin to go HIGH (inactive/disabled)
      Serial.println(F("Well done! Sleeping in 6 seconds.."));
    }
  }
}

//==============================================================================//
//LED blink pattern

void blinkLED() {
  digitalWrite(LED_PIN, HIGH);  //turn LED on to indicate awake
  delay(100);
  digitalWrite(LED_PIN, LOW);  //turn LED on to indicate awake
  delay(100);
  digitalWrite(LED_PIN, HIGH);  //turn LED on to indicate awake
  delay(100);
  digitalWrite(LED_PIN, LOW);  //turn LED on to indicate awake
  delay(100);
  digitalWrite(LED_PIN, HIGH);  //turn LED on to indicate awake
  delay(100);
  digitalWrite(LED_PIN, LOW);  //turn LED on to indicate awake
  delay(100);
  digitalWrite(LED_PIN, HIGH);  //turn LED on to indicate awake
}

//==============================================================================//
//establishes serial communication

bool establishSerial() {
  if(!isSerialEstablished) {
    Serial.begin(9600);
    while (!Serial) {
      ;
    }
    isSerialEstablished = true; //set the status
    Serial.println();
    Serial.println(F("Serial Established."));
  }
}

//==============================================================================//
//ends serial communication if active

bool endSerial() {
  if(isSerialEstablished) {
    Serial.end();
    isSerialEstablished = false; //set the status
    return true; //success
  }
  return false;
}

//==============================================================================//
//converts the BCD read from RTC register to DEC for transmission

byte bcdToDec(byte val) {
  return ((val/16*10) + (val%16));
}

//==============================================================================//
//converts the DEC input values to BCD in order to update the RTC register

byte decToBcd(byte val) {
  return ((val/10*16) + (val%10));
}

//==============================================================================//
//reads the RTC time register and prints the time
//type "t" to the console to get the current time

void printTime() {
  Wire.beginTransmission(0x6F); //send I2C address of RTC
  Wire.write(SC); //seconds register
  Wire.endTransmission();

  Wire.requestFrom(0x6F,6); // now get the byte of data...

  secondValue = Wire.read();
  minuteValue = Wire.read();
  hourValue = Wire.read();
  dateValue = Wire.read();
  monthValue = Wire.read();
  yearValue = Wire.read();

  establishSerial();

  Serial.println();
  Serial.print(F("Time is "));
  Serial.print(bcdToDec(hourValue & B00011111)); //convert the read BCD to DEC
  Serial.print(':');
  Serial.print(bcdToDec(minuteValue));
  Serial.print(':');
  Serial.print(bcdToDec(secondValue));
  Serial.print(' ');

  if((hourValue & B00100000) == 0) { //check HR21 bit (AM/PM)
    periodValue = 0;
    Serial.print(F("AM, "));
  }
  else {
    periodValue = 1;
    Serial.print(F("PM, "));
  }

  Serial.print(bcdToDec(dateValue));
  Serial.print('-');
  Serial.print(bcdToDec(monthValue));
  Serial.print('-');
  Serial.println(bcdToDec(yearValue));
}

//==============================================================================//
//reads the RTC alarm register and prints the alarm time
//type "a" to the console for alarm time

void printAlarmTime() {
  Wire.beginTransmission(0x6F);
  Wire.write(SCA);
  Wire.endTransmission();

  Wire.requestFrom(0x6F,5); // now get the byte of data...

  //AND operation is to remove the ENABLE bit (MSB) of each register value
  secondValue = B01111111 & Wire.read();
  minuteValue = B01111111 & Wire.read();
  hourValue = B01111111 & Wire.read();
  dateValue = B01111111 & Wire.read();
  monthValue = B01111111 & Wire.read();

  establishSerial();

  Serial.println();
  Serial.print(F("Alarm Time is "));
  Serial.print(bcdToDec(hourValue & B00011111));
  Serial.print(':');
  Serial.print(bcdToDec(minuteValue));
  Serial.print(':');
  Serial.print(bcdToDec(secondValue));
  Serial.print(' ');

  if((hourValue & B00100000) == 0) { //check HR21 bit (AM/PM)
    periodValue = 0;
    Serial.print(F("AM, "));
  }
  else {
    periodValue = 1;
    Serial.print(F("PM, "));
  }

  Serial.print(bcdToDec(dateValue));
  Serial.print('-');
  Serial.print(bcdToDec(monthValue));
  Serial.println(F(" Every year"));
}

//==============================================================================//
