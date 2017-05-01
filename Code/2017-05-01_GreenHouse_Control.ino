#include <SD.h>
#include <SPI.h>
#include "Wire.h"
#include "DHT.h"

#define DS3231_I2C_ADDRESS 0x68 // Real time clock
#define DHTPIN 6
#define DHTTYPE DHT22


DHT dht(DHTPIN, DHTTYPE);

byte decToBcd(byte val){
  return( (val/10*16) + (val%10) );
  }
  
byte bcdToDec(byte val){
  return( (val/16*10) + (val%16) );
   }

long previousMillisdeshum = 0;
long previousMillislog = 0;
long previousMillisroof = 0;
long previousMillisheat = 0;
int deshumstate = 0;

/*#####################ADJUSTMENTS#######################################*/

//////Intervals

long loginterval = 60000; // time in milliseconds between status log on SD card
long heatinterval = 20000; // time in millisecond between status check for heating
long roofinterval = 120000; // time in millisecond between status check for cooling

//////Basic settings

int tempset = 25; // Desired optimal temperature in celsius 
int maxhum = 73; // Desired maximum relative humidity inside the greenhouse
//////Déshumidification

int ouvdeshum = 10000; //time of roof opening (in milliseconds) during a deshumidification cycle
int closedeshum = 15000; //time of roof closin (in milliseconds) during a deshumidification cycle
long interval1 = 480000; //time (in milliseconds) when the roof is closed during a deshumidification cycle
long interval2 = 120000; //time (in milliseconds) when the roof is open durind a deshumidification cycle

//////Ventilation

int tempopen = 1; // number of celcius degree on top of tempset necessary for roof opening stge 1
int tempopenstage2 = 2; // number of celcius degree on top of tempset necessary for stage 2 roof opening
int openstage1 = 5000; // number of millisecond of roof opening for one verification cycle for stage 1 roof opening
int openstage2 = 8000; //number of millisecond of roof opening for one verification cycle for stage 2 roof opening

int tempclose = 0; //number of celcius degree under tempset necessary to close roof stage 1
int tempclosestage2 = 1; // number of celcius degree under tempset necessary to close roof stage 2
int tempclosestage3 = 2; // number of celciun degree under tempset necessary to close roof stage 3
int closestage1 = 5000; // number of millisecond of roof closing for 1 verification cycle if close roof stage 1 
int closestage2 = 8000; // number of millisecond of roof closing for 1 verification cycle if close roof stage 2
int closestage3 = closedeshum; //number of millisecond of roof closing for 1 verification cycle if close roof stage 3 

//////Heating

int startnight = 18; // hour considered for the beginnig of the night period
int startdip = 5; // hour considered for the beginnig of the dip period
int startday = 8; // hour considered for the beginnig of the day period

int endnight = startdip;
int enddip = startday;
int endday = startnight;

int tempheatnight = 6; // number of celcius degree under tempset necessary to trigger heating
int tempheatday = 3; // number of celcius degree under tempset necessary to trigger heating
int tempheatdip = 4; // number of celcius degree under tempset necessary to trigger heating
float heaton = 0.6; // number of celcius degree under heating temperature set necessary to trigger heating 
float heatoff = 0.6; // number of celcius degree above heating temperature set necessary to shot off heating
int tempheat; // 
int heatstate = 0; // this variavble as two possible stat, 1 = on, 0=off and is using at the moment of writing status on SD card

File myFile;

void setup(){
  Wire.begin();
  Serial.begin(9600);
  pinMode(2,OUTPUT); //roof opening
  pinMode(3,OUTPUT); //roof closing
  pinMode(5,OUTPUT); //heating

  
  digitalWrite(2,HIGH); // It is necessary to put these digital pin on HIGH state during setup to avoind malfunction of deveces. The opto-isolated relay board will tregger relay when the digital pin are on LOW state, not HIGH
  digitalWrite(3,HIGH);
  digitalWrite(5,HIGH);

  dht.begin();
  Serial.print("Initializing SD card...");
      if (!SD.begin(4)) {
      Serial.println("initialization failed!");
      return;
      }
}

void readDS3231time(
byte *second,
byte *minute,
byte *hour,
byte *dayOfWeek,
byte *dayOfMonth,
byte *month,
byte *year)

  {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7); // request seven bytes of data from DS3231 starting from register 00h
  *second = bcdToDec(Wire.read() & 0x7f);
  *minute = bcdToDec(Wire.read());
  *hour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
  }
  
void loop(){
  
  serialdisplay();
  
  unsigned long currentMillis = millis();
  
  if(currentMillis - previousMillislog >= loginterval) {
  previousMillislog = currentMillis;
  statuslog();
  }
  
  if(currentMillis - previousMillisroof >= roofinterval){
  previousMillisroof = currentMillis;
  roofcontrol();
  }
  
  if(currentMillis - previousMillisheat >= heatinterval){
  previousMillisheat = currentMillis;
  heatercontrol();
  }
  
  float h = dht.readHumidity();
  float temp = dht.readTemperature();
     
     /* Roof closing end of deshumidification cycle */
     
     if(currentMillis - previousMillisdeshum >= interval2 && deshumstate == 1 && temp < tempset + tempopen) {
     previousMillisdeshum = currentMillis;
     digitalWrite (9,LOW);
     delay(closedeshum);
     digitalWrite(9,HIGH);
     deshumstate = 0;
     }
  
     /* Roof opening beginnig of deshumidification cycle */
     
     if(currentMillis - previousMillisdeshum >= interval1 && h >= maxhum && deshumstate == 0) {
     previousMillisdeshum = currentMillis;
     digitalWrite (8,LOW);
     delay(ouvdeshum);
     digitalWrite(8,HIGH);
     deshumstate = 1;
     }
     
delay(5000); // for stability

}

/*######## Writing of status in the serial monitor ########*/

  void serialdisplay(){ 
   

  float h = dht.readHumidity();
  float temp = dht.readTemperature();
   
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){ 
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){
       tempheat = tempheatday;
     }
  
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(year, DEC);  
  Serial.print(" ");
  Serial.print(hour, DEC);
  Serial.print(":");
    if (minute<10){
    Serial.print("0");
    }
  Serial.print(minute, DEC);
  
  Serial.print(";");
  Serial.print(tempset - tempheat);
  Serial.print(";");
  Serial.print(tempset - tempclose);
  Serial.print(";");
  Serial.print(tempset + tempopen);
  Serial.print(";");
  Serial.print(h);
  Serial.print(";");
  Serial.print(temp);
  Serial.print(";");
  Serial.print(deshumstate);
  Serial.print(";");
  Serial.print(millis());
  Serial.print(";");
  Serial.print(maxhum);
  Serial.print(";");
  Serial.println(heatstate);
  }
  
  /*######## Writing of staus on the SD card ########*/

 void statuslog(){ 
   
  float h = dht.readHumidity();
  float temp = dht.readTemperature();
   
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){
       tempheat = tempheatday;
     }
  
  myFile = SD.open("datalog.txt", FILE_WRITE);
  
  myFile.print(month, DEC);
  myFile.print("/");
  myFile.print(dayOfMonth, DEC);
  myFile.print("/");
  myFile.print(year, DEC);  
  myFile.print(" ");
  myFile.print(hour, DEC);
  myFile.print(":");
    if (minute<10){
    myFile.print("0");
    }
  myFile.print(minute, DEC);
  
  myFile.print(";");
  myFile.print(tempset - tempheat);
  myFile.print(";");
  myFile.print(tempset - tempclose);
  myFile.print(";");
  myFile.print(tempset + tempopen);
  myFile.print(";");
  myFile.print(h);
  myFile.print(";");
  myFile.print(temp);
  myFile.print(";");
  myFile.print(deshumstate);
  myFile.print(";");
  myFile.print(millis());
  myFile.print(";");
  myFile.print(maxhum);
  myFile.print(";");
  myFile.println(heatstate);
  myFile.close();
  
 }
  
void roofcontrol(){
  
  float h = dht.readHumidity(); // capteur DHT22 serre 6
  float temp = dht.readTemperature(); // capteur DHT22 serre 6
     
     /*roof opening if temperature is too high */
     
     if (temp >= tempset + tempopen){ 
          if (temp >= tempset + tempopenstage2){ // The roof will open a little faster
          digitalWrite(8,LOW);
          delay(openstage2);
          digitalWrite(8,HIGH);
          }
          else{
          digitalWrite(8,LOW);
          delay(openstage1);
          digitalWrite(8,HIGH);
          }
      }
     
     /* Roof closing if temperature is too low */  
         
     if (temp <= tempset - tempclose){ 
         if (temp <= tempset - tempheat - tempclosestage3 ){ //Roof will close even if the deshumidification cycle is on
         digitalWrite(9,LOW);
         delay(closestage3);
         digitalWrite(9,HIGH);
         }  
         else if (temp <= tempset - tempclosestage2 && deshumstate == 0){ //Roof will close a little faster than stage 1 but no closing if deshum cycle is on
         digitalWrite(9,LOW);
         delay(closestage2);
         digitalWrite(9,HIGH);
         }  
         else if (temp <= tempset - tempclose && deshumstate == 0){ //Roof will close if dehumiditicetion cycle is not on
         digitalWrite(9,LOW);
         delay(closestage1);
         digitalWrite(9,HIGH);
         }
     }
 }
  
void heatercontrol(){
     
  float h = dht.readHumidity();
  float temp = dht.readTemperature();
  
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){
       tempheat = tempheatday;
     }
       
     if (temp <= tempset - tempheat - heaton){
     digitalWrite(6,LOW);
     digitalWrite(7,LOW);
     digitalWrite(10,LOW);
     digitalWrite(3,LOW);
     heatstate = 1;
     }
     
     if (temp > tempset - tempheat + heatoff){
     digitalWrite(6,HIGH);
     digitalWrite(7,HIGH);
     digitalWrite(10,HIGH);
     digitalWrite(3,HIGH);
     heatstate = 0;
     }
     
    }
