#include <Wire.h>
#include <SSD1306AsciiWire.h>
#include "SD.h"
#include <dht.h>
#include <SoftwareSerial.h>

#define DHT11_PIN 6
#define LM35_PIN A3
#define PHOTORESISTOR A0
#define SOILMOISTURE A1
#define RXPin 8
#define TXPin 9
#define SDPIN 7
#define MINUTE 30

#define INVALID_CLIMATE 0
#define TROPICAL 1
#define SUBTROPICAL 2
#define TEMPERATE 3
#define POLAR 4

SSD1306AsciiWire display;

int timesRead = 0;
int sumTemps = 0;
int sumAirMoist = 0;
int sumSoilMoist = 0;
int sumLight = 0;
int climate = INVALID_CLIMATE;

volatile bool buttonState = LOW;
volatile bool lastButtonState = LOW;
volatile unsigned long lastDebounceTime = 0;
volatile bool buttonPressed = false;

volatile unsigned long int timerDisplayInfo = 0;
volatile int timerUser = 0;
int menuFrame = 0;
volatile int frameTime = 0;

// functions which returns an int (according to the climate)
// dependng on the given latitude value
int getClimate(float latitude)
{
  if (latitude < 0) {
    return INVALID_CLIMATE;
  }
  if (latitude >= 0 && latitude < 24) {
    return TROPICAL;
  }
  if (latitude >= 24 && latitude < 40) {
    return SUBTROPICAL;
  }
  if (latitude >= 40 && latitude < 60) {
    return TEMPERATE;
  }
  if (latitude >= 60 && latitude <= 90) {
    return POLAR;
  }
  return INVALID_CLIMATE;
}

// function which, from an int,
// returns a string with the climate name
String getClimateFromMetric(int climate)
{
  if (climate == 1) {
    return "TROPICAL";
  }
  if (climate == 2) {
    return "SUBTROPICAL";
  }
  if (climate == 3) {
    return "TEMPERATE";
  }
  if (climate == 4) {
    return "POLAR";
  }
}

// function which, from an input NMEA sentence,
// extracts a string with the latitude
String extractCoordinates(const String& sentence)
{
  int commas;
  if (sentence.indexOf("GPRMC") != -1) {
    commas = 3;
  } else if (sentence.indexOf("GPGGA") != -1){
    commas = 2;
  } else {
    return "";
  }

  int commaCount = 0;
  int startIndex = -1;
  int endIndex = -1;

  for (size_t i = 0; i < sentence.length(); i++) {
    if (sentence[i] == ',') {
      commaCount++;
      if (commaCount == commas) {
        startIndex = i + 1;
      } else if (commaCount == commas + 1) {
        endIndex = i;
        break;
      }
    }
  }

  if (startIndex != -1 && endIndex != -1) {
    return sentence.substring(startIndex, endIndex);
  } else {
    return "";
  }
}

// function which reads NMEA sentences from
// software serial for 20 seconds and returns the
// latitude in case the sentence is valid
String getGPGGAsentence()
{
  String sentence = "";
  bool startedSentence = false;
  int currentTime = timerDisplayInfo;
  SoftwareSerial gpsSerial(RXPin, TXPin);
  gpsSerial.begin(9600);

  while (timerDisplayInfo - currentTime < MINUTE / 4) {
    if (gpsSerial.available()) {
      char c = gpsSerial.read();
      if (c == '$') {
        // check if we have successfuly read an entire GPGGA sentence
        String extractedCoordinate = extractCoordinates(sentence);
        if (!extractedCoordinate.equals("")) {
          return extractedCoordinate;
        }
        
        sentence = "";
        sentence += c;
        startedSentence = true;
      } else {
        if (startedSentence == true) {
          sentence += c;
        }
      }
    }
  }
  sentence = "";
  return sentence;
}

// function for displaying loading
void displayLoading()
{
  display.clear();
  display.setCursor(0, 0);
  display.println("LOADING...");
  display.displayRows();
}

// function for displaying details about a plant
void displayPlantDetails(String name, int values[9])
{
  display.clear(); // Clear the display
  display.setCursor(0, 0);
  display.println(name);
  display.println();
  display.print("Climate: ");
  display.println(getClimateFromMetric(values[0]));
  display.print("Temperature: ");
  display.print(values[1]);
  display.print("-");
  display.println(values[2]);
  display.print("Air Moisture: ");
  display.print(values[3]);
  display.print("-");
  display.println(values[4]);
  display.print("Soil Moisture: ");
  display.print(values[5]);
  display.print("-");
  display.println(values[6]);
  display.print("Light: ");
  display.print(values[7]);
  display.print("-");
  display.println(values[8]);
  delay(5000);
}

// functions which displays the valid plants
// on the display
void displayPlantsList(bool showAll)
{
  display.clear();
  display.setCursor(0, 0);
  // try to read from SD card
  if (SD.begin(7)) {
    File myFile;
    if (SD.exists("plants.txt")) {
      myFile = SD.open("plants.txt");
      // if the file exists, read from it
      if (myFile) {
        int noPlantsOk = 0;
        // parse the data of the plant by extracting it;
        // each data is delimited by a comma
        while (myFile.available()) {
          bool okPlant = false;
          String line = myFile.readStringUntil('\n');
          String name;
          int values[9];
          int index = 0;
          // read line
          while (line.length() > 0) {
            int commaIndex = line.indexOf(',');
            if (commaIndex != -1) {
              String element = line.substring(0, commaIndex);
              line = line.substring(commaIndex + 2);
              
              if (index == 0) {
                name = element;
              } else {
                values[index - 1] = element.toInt();
                // check if the plant is between parameters
                if (showAll == false) {
                  if (index == 1) {
                    if (climate != INVALID_CLIMATE) {
                      if (values[0] != climate) {
                        break;
                      }
                    }
                  }

                  if (index == 3) {
                    int mediumTemp = sumTemps / timesRead;
                    if ((mediumTemp >= values[1] && mediumTemp <= values[2]) == false) {
                      break;
                    }
                  }

                  if (index == 5) {
                    int mediumAirMoist = sumAirMoist / timesRead;
                    if ((mediumAirMoist >= values[3] && mediumAirMoist <= values[4]) == false) {
                      break;
                    }
                  }

                  if (index == 7) {
                    int mediumSoilMoist = sumSoilMoist / timesRead;
                    if ((mediumSoilMoist >= values[5] && mediumSoilMoist <= values[6]) == false) {
                      break;
                    }
                  }
                }
              }

              index++;
            } else {
              // read last value from line
              String element = line;
              values[index - 1] = element.toInt();
              int mediumLight = sumLight / timesRead;
              if (showAll == false && (mediumLight >= values[7] && mediumLight <= values[8]) == false) {
                break;
              }

              okPlant = true;
              noPlantsOk++;
              break;
            }
          }
          // if a plant was found, display it
          if (okPlant) {
            displayPlantDetails(name, values);
          }
        }
        // if no plants were found
        if (noPlantsOk == 0) {
          display.println("No plants matched!");
          display.displayRows();
          delay(8000);
        }
        myFile.close();
          
      } else {
        // if the file is not found
        display.println("Add plants.txt file first!");
        display.displayRows();
        delay(8000);
      }
    } 
  } else {
    // if unable to read file
    display.println("Failed to display plants!");
    display.displayRows();
    delay(8000);
  }
}

// return a string depending on the input metric value
String getValueOfMetrics(int metric)
{
  if (round(metric) == 1) {
    return "LOW";
  }
  if (round(metric) == 2) {
    return "MEDIUM";
  }
  if (round(metric) == 3) {
    return "HIGH";
  }
}

// function which writes the average metrics to sd card
void writeMetricsToSD()
{
  if (SD.begin(7)) {
    File myFile;
    myFile = SD.open("metrics.txt", FILE_WRITE);
    if (myFile) {
      myFile.print("----- Medium metrics (");
      myFile.print(timesRead);
      myFile.println(" times read data) -----");
      myFile.print("Temperature: ");
      myFile.print(sumTemps / timesRead);
      myFile.println(" C");
      myFile.print("Air Moisture: ");
      myFile.print(sumAirMoist / timesRead);
      myFile.println("%");
      myFile.print("Soil Moisture: ");
      myFile.println(getValueOfMetrics(sumSoilMoist / timesRead));
      myFile.print("Luminousity: ");
      myFile.println(getValueOfMetrics(sumLight / timesRead));
      if (climate != INVALID_CLIMATE) {
        myFile.print("Climate: ");
        myFile.println(getClimateFromMetric(climate));
      }
      myFile.close();
    } else {
      Serial.println("Couldn't create file.");
    }
  } else{
    Serial.println("Initialization failed.");
  }
}

// function
void writeStatsToDisplay()
{
  displayLoading();

  // extract the latitude from the read NMEA sentence
  String latitude = getGPGGAsentence();
  float latitudeValue;
  // check if the value is valid
  if (latitude.equals("")) {
    latitudeValue = -1;
  } else {
    // if so, extract it (the first 2 digits represent the
    // latitude value)
    latitudeValue = (latitude.substring(0, 2)).toFloat();
  }
  // change the climate variable only if the value is valid
  int newClimate = getClimate(latitudeValue);
  if (newClimate != INVALID_CLIMATE) {
    climate = newClimate;
  }

  display.clear();
  display.setCursor(0, 0);

  // read the data from the sensors and increase the
  // global values with the sum of each metric
  dht DHT;
  timesRead++;

  int chk = DHT.read11(DHT11_PIN);

  int adcVal = analogRead(LM35_PIN);
  // convert the ADC value to voltage in millivolt
  float milliVolt = adcVal * (5000.0 / 1024.0);
  // convert the voltage to the temperature in Celsius
  int temperature = milliVolt / 10;

  int lightValue = analogRead(PHOTORESISTOR);
  int soilMoisture = analogRead(SOILMOISTURE);

  sumTemps += temperature;
  sumAirMoist += int(DHT.humidity);

  // print the data
  display.print("Temperature: ");
  display.print(temperature);
  display.println(" C");
  display.println();
  display.print("Air Moisture: ");
  display.print(int(DHT.humidity));
  display.println(" %");
  display.print("Soil Moisture: ");
  if (soilMoisture <= 500) {
    sumSoilMoist += 3;
    display.println("HIGH");
  } else if (soilMoisture <= 650) {
    sumSoilMoist += 2;
    display.println("MEDIUM");
  } else {
    sumSoilMoist += 1;
    display.println("LOW");
  }
  display.println();
  display.print("Luminousity: ");
  if (lightValue < 100) {
    sumLight += 1;
    display.println("LOW");
  } else if (lightValue < 200) {
    sumLight += 2;
    display.println("MEDIUM");
  } else {
    sumLight += 3;
    display.println("HIGH");
  }
  
  // display climate only if it is valid
  if (climate != INVALID_CLIMATE) {
    display.println();
    display.print("Climate: ");
    display.println(getClimateFromMetric(climate));
  }
  display.displayRows();
  writeMetricsToSD();
  
  timerDisplayInfo = 0;
}

// functions for displaying the 5 menus
void displayMenu1()
{
  menuFrame = 1;
  frameTime = 0;
  display.clear();
  display.setCursor(0, 0);
  display.println(F("Press the button to"));
  display.println();
  display.println(F("perform the action:"));
  display.println();
  display.println();
  display.println(F("1 - Show valid plants list"));
  display.displayRows();
}

void displayMenu2()
{
  menuFrame = 2;
  frameTime = 0;
  display.clear();
  display.setCursor(0, 0);
  display.println(F("Press the button to"));
  display.println();
  display.println(F("perform the action:"));
  display.println();
  display.println();
  display.println(F("2 - Show all plants"));
  display.displayRows();
}

void displayMenu3()
{
  menuFrame = 3;
  frameTime = 0;
  display.clear();
  display.setCursor(0, 0);
  display.println(F("Press the button to"));
  display.println();
  display.println(F("perform the action:"));
  display.println();
  display.println();
  display.println(F("3 - Reset metrics"));
  display.displayRows();
}

void displayMenu4()
{
  menuFrame = 4;
  frameTime = 0;
  display.clear();
  display.setCursor(0, 0);
  display.println(F("Press the button to"));
  display.println();
  display.println(F("perform the action:"));
  display.println();
  display.println();
  display.println(F("4 - Show average"));
  display.println(F("metrics"));
  display.displayRows();
}

void displayMenu5()
{
  menuFrame = 5;
  frameTime = 0;
  display.clear();
  display.setCursor(0, 0);
  display.println(F("Press the button to"));
  display.println();
  display.println(F("perform the action:"));
  display.println();
  display.println();
  display.println(F("5 - Return to stats"));
  display.displayRows();
}

// function for resetting the metrics
void resetMetrics()
{
  timesRead = 0;
  sumTemps = 0;
  sumAirMoist = 0;
  sumSoilMoist = 0;
  sumLight = 0;
  climate = 0;
  display.clear();
  display.setCursor(0, 0);
  display.println(F("Metrics reseted!"));
  display.displayRows();
  delay(8000);
}

// function for displaying the average metrics
void showAverageMetrics()
{
  display.clear();
  display.setCursor(0, 0);
  // check if any data were read
  if (timesRead == 0) {
    display.println(F("No stats read!"));
    display.displayRows();
    delay(8000);
    return;
  }
  display.print("Temperature: ");
  display.print(sumTemps / timesRead);
  display.println(" C");
  display.println();
  display.print("Air Moisture: ");
  display.print(sumAirMoist / timesRead);
  display.println("%");
  display.println();
  display.print("Soil Moisture: ");
  display.println(getValueOfMetrics(sumSoilMoist / timesRead));
  display.println();
  display.print("Luminousity: ");
  display.println(getValueOfMetrics(sumLight / timesRead));
  display.displayRows();
  delay(8000);
}

ISR(TIMER1_COMPA_vect) {
  timerDisplayInfo++;
  if (timerUser != -1) {
    timerUser++;
    frameTime++;
  }
}

void configure_timer1() {
	// initialize registers
	TCCR1A = 0;
	TCCR1B = 0;
	TCNT1 = 0;

  // set for 2 seconds
  OCR1A = 31312;
  TCCR1B |= (1 << WGM12);
  // set prescaler to 1024
  TCCR1B |= (1 << CS12) | (1 << CS10);
	// enable timer compare interrupts
	TIMSK1 |= (1 << OCIE1A);
}

ISR(INT0_vect) {
  // read the current button state
  bool currentState = digitalRead(2);

  // Check if the button state has changed
  if (currentState != lastButtonState) {
    // Reset the debounce timer
    lastDebounceTime = millis();
  }

  // Update the button state
  lastButtonState = currentState;

  // Check if the debounce delay has passed
  if (millis() - lastDebounceTime > 300) {
    // Update the buttonPressed flag if the button is pressed
    if (lastButtonState == LOW) {
      buttonPressed = true;
    }
  }
}

void configure_external_intrerrupt()
{
  // configure external interrupt on falling edge for INT0 (digital pin 2)
  EICRA |= (1 << ISC01);
  EICRA &= ~(1 << ISC00);

  // enable external interrupt for INT0
  EIMSK |= (1 << INT0);
}

void setup(void)
{
  Serial.begin(9600);
  pinMode(2, INPUT_PULLUP);

  // initialize intrerruptions
  cli();
  configure_external_intrerrupt();
  configure_timer1();
  sei();

  // initialize display
  Wire.begin();
  Wire.setClock(400000L);

  display.begin(&Adafruit128x64, 0x3C, -1);
  display.setFont(System5x7);
  writeStatsToDisplay();
}

void loop() {
  if (buttonPressed) {
    // if the button was pressed, check frame
    // and perform action
    if (menuFrame == 1) {
      if (timesRead > 0) {
        displayPlantsList(false);
      } else {
        // check if any stats are read
        display.clear();
        display.setCursor(0, 0);
        display.println(F("No stats read!")); 
        display.displayRows();
        delay(8000);
      }
      displayMenu1();
    }
    if (menuFrame == 2) {
      displayPlantsList(true);
      displayMenu1();
    }
    if (menuFrame == 3) {
      resetMetrics();
      displayMenu1();
    }
    if (menuFrame == 4) {
      showAverageMetrics();
      displayMenu1();
    }

    timerUser = 0;
    // display menu for the first time
    if (menuFrame == 0) {
      displayMenu1();
    }

    if (menuFrame == 5) {
      // return to initial state
      writeStatsToDisplay();
      menuFrame = 0;
      timerUser = -1;
    }
  
    buttonPressed = false;
    delay(500);
  }

  // if used by the user and 6 seconds have passes
  // on the same menu, move to the next one
  if (timerUser != -1 && frameTime > 3) {
    if (menuFrame == 1) {
      displayMenu2();
    } 
    else if (menuFrame == 2) {
      displayMenu3();
    }
    else if (menuFrame == 3) {
      displayMenu4();
    }
    else if (menuFrame == 4) {
      displayMenu5();
    }
    else if (menuFrame == 5) {
      displayMenu1();
    }
  }

  // if after 3 minutes there is no input, stop it
  if (timerUser >= MINUTE * 3) {
    menuFrame = 0;
    timerUser = -1;
    writeStatsToDisplay();
  }

  // if no input from user and 60 minutes have passed,
  // read data again
  if (timerUser == -1 && timerDisplayInfo >= MINUTE * 60) {
    writeStatsToDisplay();
  }
}
