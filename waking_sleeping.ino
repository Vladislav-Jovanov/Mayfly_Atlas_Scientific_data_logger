/*
 * ------------------------------------------------------------
 *
 * This sketch wakes the Mayfly up at specific times, records
 * the temperature from the attached probe, writes the data to
 * the microSD card, prints the data string to the serial port
 * and goes back to sleep.
 *
 * Change (currentminute % 1 == 0) in loop() function
 * to deired time to wake up and record data.
 *
 * ------------------------------------------------------------
*/

#include  <Arduino.h>

#include  <Wire.h> // library to communicate with I2C / TWI devices
#include  <avr/sleep.h> // library to allow an application to sleep
#include  <SD.h> // library for reading and writing to SD cards
#include  <RTCTimer.h> // library to schedule tasks using RTC
#include  <Sodaq_DS3231.h> // library for the DS3231 RTC
#include  <Sodaq_PcInt.h> // library to handle Pin Change Interrupts
#include <Ezo_i2c.h> //include the EZO I2C library from https://github.com/Atlas-Scientific/Ezo_I2c_lib


#define   RTC_PIN A7 // RTC Interrupt pin
#define   RTC_INT_PERIOD EveryMinute

#define   SD_SS_PIN 12 // Digital pin 12 is the MicroSD slave select pin on the Mayfly

char const * filename = "logfile.csv"; // The data log file

// https://stackoverflow.com/questions/20944784/why-is-conversion-from-string-constant-to-char-valid-in-c-but-invalid-in-c
// #define   FILE_NAME "logfile.txt" // ERROR - ISO C++ forbids converting a string constant to 'char*

// Data Header
#define   DATA_HEADER "Sampling_Unit: 2c90adc7-aefe-4b25-bc88-6eb0d100a4aa\r\nBoard_name: EnviroDIY_Mayfly_Data_Logger\r\nSensors_Name: Ezo_ORP Ezo_RTD Mayfly_Battery Mayfly_RTD\r\nVariable_Name:\r\nLocal Sensor Time,Redox potential,Temperature,Battery_Voltage,Board_Tempemperature\r\nUnits:\r\ndate and time,mV,degreeCelsius,V,degreeCelsius"


RTCTimer  timer;

//declare Atlas Scientific sensors here
Ezo_board ORP=Ezo_board(98, "ORP");
Ezo_board RTD=Ezo_board(102, "RTD");


String    dataRec = "";

int       currentminute;
long      currentepochtime = 0; // Number of seconds elapsed since 00:00:00 Thursday, 1 January 1970
float     boardtemp;

int       batteryPin = A6; // select the input pin for the potentiometer
int       batterysenseValue = 0; // variable to store the value coming from the sensor
float     batteryvoltage;

/*
 * --------------------------------------------------
 *
 * PlatformIO Specific
 *
 * C++ requires us to declare all custom functions
 * (except setup and loop) before they are used.
 *
 * --------------------------------------------------
*/

void showTime(uint32_t ts);
void setupTimer();
void wakeISR();
void setupSleep();
void systemSleep();
void sensorsSleep();
void sensorsWake();
void greenred4flash();
void setupLogFile();
void logData(String rec);

String getDateTime();
uint32_t getNow();

static void addFloatToString(String & str, float val, char width, unsigned char precision);

/*
 * --------------------------------------------------
 *
 * repeatAction()
 *
 * Retrieve and display the current date/time and data from sensors
 *
 * --------------------------------------------------
 */

void repeatAction(uint32_t ts)
{

  greenred4flash(); // blink the LEDs to show the board is on
  Serial.println("Initiating sensor reading and logging data to SDcard");
  Serial.print("----------------------------------------------------");

  dataRec = createDataRecord();//in this function sensors are read

  // Save the data record to the log file
  logData(dataRec);

  // Echo the data to the serial connection
  Serial.println();

  Serial.print("Data Record: ");
  Serial.println(dataRec);
  Serial.print("\n\r");
  dataRec = "";  //clear data record

}

/*
 * --------------------------------------------------
 *
 * setupTime()
 *
 * --------------------------------------------------
 */

void setupTimer()
{

  // Schedule the wakeup
  timer.every(120, repeatAction); //this time is in seconds

  // Instruct the RTCTimer how to get the current time reading
  timer.setNowCallback(getNow);

}

/*
 * --------------------------------------------------
 *
 * wakeISR()
 *
 * --------------------------------------------------
 */

void wakeISR()
{
  // Leave this blank

}

/*
 * --------------------------------------------------
 *
 * setupSleep()
 *
 * --------------------------------------------------
 */

void setupSleep()
{
  pinMode(RTC_PIN, INPUT_PULLUP);
  PcInt::attachInterrupt(RTC_PIN, wakeISR);

  // Setup the RTC in interrupt mode
  rtc.enableInterrupts(RTC_INT_PERIOD);

  // Set the sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

}

/*
 * --------------------------------------------------
 *
 * systemSleep()
 *
 * --------------------------------------------------
 */

void systemSleep()
{

  // Wait until the serial ports have finished transmitting
  Serial.flush();

  // The next timed interrupt will not be sent until this is cleared
  rtc.clearINTStatus();

  // Disable ADC
  ADCSRA &= ~_BV(ADEN);

  // Sleep time
  noInterrupts();
  sleep_enable();
  interrupts();
  sleep_cpu();
  sleep_disable();

  // Enbale ADC
  ADCSRA |= _BV(ADEN);

}


/*
 * --------------------------------------------------
 *
 * getDateTime()
 *
 * --------------------------------------------------
 */

String getDateTime()
{
  String dateTimeStr;
  DateTime dt(rtc.makeDateTime(rtc.now().getEpoch())); // Create a DateTime object

  currentepochtime = (dt.get()); // Unix time in seconds
  currentminute = (dt.minute());
  
  dt.addToString(dateTimeStr); // Convert it to a String

  return dateTimeStr;

}

/*
 * --------------------------------------------------
 *
 * getNow()
 *
 * --------------------------------------------------
 */

uint32_t getNow()
{
  currentepochtime = rtc.now().getEpoch();
  return currentepochtime;

}

/*
 * --------------------------------------------------
 *
 * greenred4Flash()
 *
 * Blink the LEDs to show the board is on.
 *
 * --------------------------------------------------
 */

void greenred4flash()
{
  for (int i=1; i <= 4; i++){
    digitalWrite(8, HIGH);
    digitalWrite(9, LOW);
    delay(50);

    digitalWrite(8, LOW);
    digitalWrite(9, HIGH);
    delay(50);
  }

  digitalWrite(9, LOW);

}

/*
 * --------------------------------------------------
 *
 * setupLogFile()
 *
 * --------------------------------------------------
 */

void setupLogFile()
{
  // Initialise the SD card
  if (!SD.begin(SD_SS_PIN))
  {
    Serial.println("Error: SD card failed to initialize or is missing.");
    // Hang
    // while (true);
  }

  // Check if the file already exists
  bool oldFile = SD.exists(filename);

  // Open the file in write mode
  File logFile = SD.open(filename, FILE_WRITE);

  // Add header information if the file did not already exist
  if (!oldFile)
  {
    // logFile.println(LOGGERNAME);
    logFile.println(DATA_HEADER);
  }

  // Close the file to save it
  logFile.close();

}

/*
 * --------------------------------------------------
 *
 * logData()
 *
 * --------------------------------------------------
 */

void logData(String rec)
{
  // Re-open the file
  File logFile = SD.open(filename, FILE_WRITE);

  // Write the CSV data
  logFile.println(rec);

  // Close the file to save it
  logFile.close();

}

/*
 * --------------------------------------------------
 *
 * createDataRecord()
 *
 * Create a String type data record in csv format:
 *
 * DateTime, SensorTemp_C, BatteryVoltage,
 * BoardTemp_C
 *
 * --------------------------------------------------
 */

 void readSensor(Ezo_board * Board, String &Data)
 { 
   Board->send_cmd("i");//wakeup sensor
   delay(1000);//give time for sensor to be ready
   Board->send_read_cmd();//read sensor
   delay(1000);//allow time for information to be ready
   Board->receive_read_cmd();//get reading
   Data+=Board->get_last_received_reading();//put reading into thje variable
   Data += ",";
   Board->send_cmd("SLEEP");//put sensor back to sleep
   delay(1000);//give time before moving to next sensor
 }

String createDataRecord()
{
  String data = getDateTime();
  data += ",";

  // read all Atlas Scientific sensors here
  //----------------------------------------
  readSensor(&ORP, data);
  readSensor(&RTD, data);

  // Battery Voltage and Board Temperature --------------------

  rtc.convertTemperature(); // convert current temperature into registers
  boardtemp = rtc.getTemperature(); // Read temperature sensor value
  batterysenseValue = analogRead(batteryPin);
  /*
   * --------------------------------------------------
   *
   * Mayfly version 0.3 and 0.4 have a different
   * resistor divider than v0.5 and newer.
   *
   * Please choose the appropriate formula based on
   * your board version:
   *
   * --------------------------------------------------
  */

  // For Mayfly v0.3 and v0.4:
  // batteryvoltage = (3.3/1023.) * 1.47 * batterysenseValue;

  // For Mayfly v0.5 and newer:
  batteryvoltage = (3.3/1023.) * 4.7 * batterysenseValue;

  

  addFloatToString(data, batteryvoltage, 4, 2);

  data += ",";

  addFloatToString(data, boardtemp, 3, 1); // float

  return data;

}

/*
 * --------------------------------------------------
 *
 * addFloatToString()
 *
 * --------------------------------------------------
 */

static void addFloatToString(String & str, float val, char width, unsigned char precision)
{
  char buffer[10];
  dtostrf(val, width, precision, buffer);
  str += buffer;

}

/*
 * --------------------------------------------------
 *
 * Setup()
 *
 * --------------------------------------------------
 */

void setup()
{

  Serial.begin(9600); // Initialize the serial connection
  rtc.begin();//this also does Wire.begin() and starts I2C
  delay(500); // Wait for newly restarted system to stabilize

  pinMode(8, OUTPUT); // for flashing LED
  pinMode(9, OUTPUT); // for flashing LED

  greenred4flash(); // blink the LEDs to show the board is on


  setupLogFile();
  setupTimer(); // Setup timer events
  setupSleep(); // Setup sleep mode


  Serial.println("Power On, running: Temperature Logging");
  Serial.print("\n\r");
  Serial.println(DATA_HEADER);
  Serial.print("\n\r");

  
}

/*
 * --------------------------------------------------
 *
 * loop()
 *
 * --------------------------------------------------
 *
 */
void loop()
{
  timer.update();// Update the timer
  systemSleep(); // Sleep
  
}
