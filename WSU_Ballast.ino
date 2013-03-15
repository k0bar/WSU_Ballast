/*
  WSU Ballast Valve Control and Temperature Logging
 
 The circuit:
 * One Wire Bus - pin 3
 * Ballast Valve Input - pin 4 (defined in Ballast_Input)
 * Ballast Valve Output - pin 6 (defined in Ballast_Output)
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4
 
created  12 March 2013
 
by Bruce Rahn, K0BAR
 
 */

#include <SD.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Arduino.h>

//#define SDDEBUG
//#define ONEWIREADDRESS
//#define EDM
//#define TEMPERATURE

#define CYCLE_BALLAST_VALVE

#ifdef TEMPERATURE

// Data wire is plugged into pin 3 on the Arduino
#define ONE_WIRE_BUS 3

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// Assign the addresses of your 1-Wire temp sensors.
#ifdef EDM
// EDM Sensors
DeviceAddress batterySensor = {0x28, 0xD4, 0x93, 0x18, 0x04, 0x00, 0x00, 0x45};
DeviceAddress boxSensor = {0x28, 0x92, 0xDA, 0x00, 0x04, 0x00, 0x00, 0xBC};
DeviceAddress outsideSensor = {0x28, 0x16, 0xCC, 0x00, 0x04, 0x00, 0x00, 0x6A};
#else
// Flight Sensors
DeviceAddress batterySensor = {0x28, 0xFB, 0xAD, 0x8F, 0x02, 0x00, 0x00, 0x47};
DeviceAddress boxSensor = {0x28, 0xC2, 0x8E, 0x8F, 0x02, 0x00, 0x00, 0x8A};
DeviceAddress outsideSensor = {0x28, 0xA3, 0x76, 0x8F, 0x02, 0x00, 0x00, 0xE6};
#endif

#endif

// On the Ethernet Shield, CS is pin 4. Note that even if it's not
// used as the CS pin, the hardware CS pin (10 on most Arduino boards,
// 53 on the Mega) must be left as an output or the SD library
// functions will not work.
const int chipSelect = 10;
const int Ballast_Input = 4;
const int Ballast_Output = 6;
unsigned long timer=0;   //general purpose timer
unsigned long ballastTimer = 0;
unsigned long Ballast_Valve_Cycle_Timer = 0;
boolean DTMFballast;
boolean DTMFballaststate = 0;
boolean ballastvalvestate = 0;
boolean Ballast_Valve_Cycle_State = 0;
unsigned int filecounter = 0;
char filename[40];
char counter[3];
File dataFile;
float batterytemp;
float boxtemp;
float outsidetemp;

#define TempMeasurementTime 10000 //(in milliseconds) 10 seconds
#define BallastOpenTime 30000 //(in miliiseconds) 30 seconds
#define Ballast_Valve_Cycle_Time 10000 //(in milliseconds) 10 seconds
#define Ballast_Valve_Cycle_Open_Time 10000 //(in milliseconds 10 seconds

void setup()
{
#ifdef SDDEBUG
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.print("Initializing SD card...");
#endif
#ifdef ONEWIREADDRESS
  // Find one wire device address
  discoverOneWireDevices();
  return;
#endif

#ifdef TEMPERATURE
  // Start up the library
  sensors.begin();
  // set the resolution to 12 bit
  sensors.setResolution(batterySensor, 12);
  sensors.setResolution(boxSensor, 12);
  sensors.setResolution(outsideSensor, 12);
#endif

  // make sure that the default chip select pin is set to
  // output, even if you don't use it:
  pinMode(10, OUTPUT);
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
#ifdef SDDEBUG
    Serial.println("Card failed, or not present");

#endif
    // don't do anything more:
    return;
  }
  dataFile = SD.open("/");
  dataFile.rewindDirectory();
  while (dataFile.openNextFile() != NULL)
  {
    ++filecounter;
  }
  strcpy(filename,"EBWL");
  sprintf(counter, "%02u",filecounter);
  strcat(filename, counter);
  strcat(filename, ".txt");
  dataFile.close();

  digitalWrite(Ballast_Output,LOW);
  pinMode(Ballast_Output,OUTPUT);
  pinMode(Ballast_Input,INPUT);
  digitalWrite(Ballast_Input,HIGH);

  timer=millis();
  Ballast_Valve_Cycle_Timer= timer;
  
#ifdef SDDEBUG
  Serial.println("card initialized.");
  Serial.println(timer);
#endif
}

void loop()
{
  DTMFballast = digitalRead(Ballast_Input);

  if((!DTMFballast) && (!DTMFballaststate))
  {
    DTMFballaststate = 1;
    ballastTimer = millis();
    digitalWrite(Ballast_Output, HIGH);
    ballastvalvestate = 1;
  }
  
  if((DTMFballast) && (DTMFballaststate))
  {
    DTMFballaststate = 0;
  } 

  if(((millis()-ballastTimer)>=BallastOpenTime) && (ballastvalvestate))
  {
    digitalWrite(Ballast_Output,LOW);
    ballastvalvestate = 0;
    Ballast_Valve_Cycle_Timer = millis();
  }

#ifdef CYCLE_BALLAST_VALVE
  if(((millis()-Ballast_Valve_Cycle_Timer)>=Ballast_Valve_Cycle_Time) && (!Ballast_Valve_Cycle_State)&& (!ballastvalvestate))
  {
    digitalWrite(Ballast_Output,HIGH);
    Ballast_Valve_Cycle_State = 1;
    Ballast_Valve_Cycle_Timer = millis();
  }
 
 if(((millis()-Ballast_Valve_Cycle_Timer)>=Ballast_Valve_Cycle_Open_Time) && (Ballast_Valve_Cycle_State)&& (!ballastvalvestate))
  {
    digitalWrite(Ballast_Output,LOW);
    Ballast_Valve_Cycle_State = 0;
    Ballast_Valve_Cycle_Timer = millis();
  } 
#endif

#ifdef TEMPERATURE
  readsensors();
#endif

}

#ifdef TEMPERATURE
void readsensors() 
{
  if((millis()-timer)>=TempMeasurementTime) 
  {
#ifdef SDDEBUG 
    Serial.println(timer);
#endif
    timer=millis();
    sensors.requestTemperatures();
    // make a string for assembling the data to log:
    String dataString = "";
    char buffer[100];
    batterytemp = sensors.getTempC(batterySensor);
    dtostrf(batterytemp,5,2,buffer);
    dataString += String(buffer);
    dataString += ",";
    boxtemp = sensors.getTempC(boxSensor);
    dtostrf(boxtemp,5,2,buffer);
    dataString += String(buffer);
    dataString += ",";
    outsidetemp = sensors.getTempC(outsideSensor);
    dtostrf(outsidetemp,5,2,buffer);
    dataString += String(buffer);
    dataString += ",";
    dataString += String(DTMFballast);
    dataString += ",";
    dataString += String(ballastvalvestate);
    dataString += ",";
    dataString += String(DTMFhelium);
    dataString += ",";
    dataString += String(heliumvalvestate);
    dataString += ",";
    dataString += String(Ballast_Valve_Cycle_State);
    dataString += ",";
    dataString += String(Helium_Valve_Cycle_State);

    
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.

    dataFile = SD.open(filename, FILE_WRITE);

    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
#ifdef SDDEBUG
      // print to the serial port too:
      Serial.println(dataString);
#endif
    }  
    // if the file isn't open, pop up an error:
    else {
#ifdef SDDEBUG
      Serial.println("error opening datalog.txt");
#endif
    } 
  }
}
#endif

#ifdef ONEWIREADDRESS
void discoverOneWireDevices(void) {
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  
  Serial.print("Looking for 1-Wire devices...\n\r");
  while(oneWire.search(addr)) {
    Serial.print("\n\rFound \'1-Wire\' device with address:\n\r");
    for( i = 0; i < 8; i++) {
      Serial.print("0x");
      if (addr[i] < 16) {
        Serial.print('0');
      }
      Serial.print(addr[i], HEX);
      if (i < 7) {
        Serial.print(", ");
      }
    }
    if ( OneWire::crc8( addr, 7) != addr[7]) {
        Serial.print("CRC is not valid!\n");
        return;
    }
  }
  Serial.print("\n\r\n\rThat's it.\r\n");
  oneWire.reset_search();
  return;
}
#endif

