#include <SD.h>
#include <SPI.h>
#include "Wire.h"
#include "DHT.h"

#define DS3231_I2C_ADDRESS 0x68 // Real time clock
#define DHTPIN 5
#define DHT2PIN 2
#define DHTTYPE DHT22
#define DHT2TYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
DHT dht2(DHT2PIN, DHT2TYPE);

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

/*#####################AJUSTEMENTS#######################################*/

//////Intervals

long loginterval = 60000; // durrée en millisecondes d'un cycle de mesure pour enregistrement dans le journal
long heatinterval = 20000; // durrée en millisecondes d'un cycle de mesure pour vérification de température pour chauffage
long roofinterval = 120000; // durrée en millisecondes d'un cycle de mesure pour vérification de température pour ventilation

//////Consignes de base

int tempset = 25; // Temperature desiree en celsius
int maxhum = 73; // Humidité max pour cylce de deshum

//////Déshumidification

int ouvdeshum = 10000; //temps d'ouverture des toits (en millisecondes) lors du cycle de deshum
int closedeshum = 15000; //temps de fermeture des toits (en millisecondes) lors du cycle de deshum
long interval1 = 480000; //intervalle durant laquelle les toits sont fermés durant le cycle de déshumidification
long interval2 = 120000; //intervalle durant laquelle toits ouverts durant le cylcle de déshumidification

//////Ventilation

int tempopen = 1; // nombre de degrés au dessus du tempset pour ouverture des toits
int tempopenstage2 = 2; // nombre de degrés au dessus du tempset nécessaires pour que la vitesse d'ouverture passe du stage 1 au stage 2
int openstage1 = 5000; //nombre de milliseconde pendant lesquels les toits ouvrent lors d'un cycle de mesure
int openstage2 = 8000; //nombre de milliseconde pendant lesquels les toits ouvrent lors d'un cycle de mesure

int tempclose = 0; //nombre de degrés en dessous du tempset pour fermeture des toits
int tempclosestage2 = 1; // nombre de degrés en dessous du tempset nécessaires pour que la vitesse de fermeture passe du stage 1 au stage 2
int tempclosestage3 = 2; // nombre de degrés en dessous du tempset - tempheat nécessaires pour que la vitesse de fermeture passe du stage 2 au stage 3
int closestage1 = 5000; //nombre de milliseconde pendant lesquels les toits ferment lors d'un cycle de mesure
int closestage2 = 8000; //nombre de milliseconde pendant lesquels les toits ferment lors d'un cycle de mesure
int closestage3 = closedeshum; //nombre de milliseconde pendant lesquels les toits ferment lors d'un cycle de mesure

//////Chaufage

int startnight = 18;
int startdip = 5;
int startday = 8;

int endnight = startdip;
int enddip = startday;
int endday = startnight;

int tempheatnight = 6; //nombre de degrés en dessous du tempset nécessaires pour enclancher le chauffage
int tempheatday = 3; //nombre de degrés en dessous du tempset nécessaires pour enclancher le chauffage
int tempheatdip = 4; //nombre de degrés en dessous du tempset nécessaires pour enclancher le chauffage
float heaton = 0.6; // nombre de degrés celcius en dessous de la température de chauffage nécessaire pour partir le chauffage
float heatoff = 0.6; // nombre de degrés celcius au dessus de la temperature de chauffage nécessaire pour fermer le chauffage
int tempheat; // cette variable change en fonction de l'heure du jour et de des consignes tempheatday et tempheatnight
int heatstate = 0; // cette variable comporte deux état possible 1 = on, 0=off et indique si les aérotherme fonctionnenent au moment d'enregistrer sur la carte SD

File myFile;

void setup(){
  Wire.begin();
  Serial.begin(9600);
  pinMode(6,OUTPUT); //chauffage s2
  pinMode(7,OUTPUT); //chauffage s4
  pinMode(8,OUTPUT); //ouverture toits
  pinMode(9,OUTPUT); //fermeture toits
  pinMode(10,OUTPUT); //chauffage s6
  pinMode(3,OUTPUT); //chauffage s8
  
  digitalWrite(6,HIGH); // Il est nécessaire de mettre tout les relais en mode HIGH au début pour avoir une position de départ où les relais n'activent aucun équipement
  digitalWrite(7,HIGH);
  digitalWrite(8,HIGH);
  digitalWrite(9,HIGH);
  digitalWrite(10,HIGH);
  digitalWrite(3,HIGH);
  
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
  
  float h = dht2.readHumidity(); // capteur DHT22 serre 6
  float temp = dht2.readTemperature(); // capteur DHT22 serre 6
     
     /* Fermeture des toits fin du cylcre de déshumiditfication */
     
     if(currentMillis - previousMillisdeshum >= interval2 && deshumstate == 1 && temp < tempset + tempopen) {
     previousMillisdeshum = currentMillis;
     digitalWrite (9,LOW);
     delay(closedeshum);
     digitalWrite(9,HIGH);
     deshumstate = 0;
     }
  
     /* Ouverture des toits pour début cylcle de déshumidification */
     
     if(currentMillis - previousMillisdeshum >= interval1 && h >= maxhum && deshumstate == 0) {
     previousMillisdeshum = currentMillis;
     digitalWrite (8,LOW);
     delay(ouvdeshum);
     digitalWrite(8,HIGH);
     deshumstate = 1;
     }
     
delay(5000); // ajout d'un délais de 5 secondes pour être certain que le capteur de température ait le temps de lire la température (le DHT22 est un capteur très lent)

}

/*######## Écriture des status sur le moniteur série ########*/

  void serialdisplay(){ 
   
  float h2 = dht.readHumidity(); //capteur DHT22 boitier de commande
  float t2 = dht.readTemperature(); // capteur DHT22 boitier de commande
  float h = dht2.readHumidity(); // capteur DHT22 serre 6
  float temp = dht2.readTemperature(); // capteur DHT22 serre 6
   
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){ // heures considérées pour la nuit
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){ // heures considérées pour le dip
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){  // heures considérées pour le jour
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
  Serial.print(h2);
  Serial.print(";");
  Serial.print(t2);
  Serial.print(";");
  Serial.print(deshumstate);
  Serial.print(";");
  Serial.print(millis());
  Serial.print(";");
  Serial.print(maxhum);
  Serial.print(";");
  Serial.println(heatstate);
  }
  
  /*######## Écriture des status sur carte SD ########*/

 void statuslog(){ 
   
  float h2 = dht.readHumidity(); //capteur DHT22 boitier de commande
  float t2 = dht.readTemperature(); // capteur DHT22 boitier de commande
  float h = dht2.readHumidity(); // capteur DHT22 serre 6
  float temp = dht2.readTemperature(); // capteur DHT22 serre 6
   
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){ // heures considérées pour la nuit
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){ // heures considérées pour le dip
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){  // heures considérées pour le jour
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
  myFile.print(h2);
  myFile.print(";");
  myFile.print(t2);
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
  
  float h = dht2.readHumidity(); // capteur DHT22 serre 6
  float temp = dht2.readTemperature(); // capteur DHT22 serre 6
     
     /*ouverture des toits si trop chaud */
     
     if (temp >= tempset + tempopen){ 
          if (temp >= tempset + tempopenstage2){ // ouverture un peu plus rapide des toits
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
     
     /* fermeture des toits si la tempérauture est trop basse */  
         
     if (temp <= tempset - tempclose){ 
         if (temp <= tempset - tempheat - tempclosestage3 ){ //fermeture des toits meme si le cycle de deshum est activé
         digitalWrite(9,LOW);
         delay(closestage3);
         digitalWrite(9,HIGH);
         }  
         else if (temp <= tempset - tempclosestage2 && deshumstate == 0){ //fermeture un peut plus rapide des toits si le cycle de deshum n'est pas activé
         digitalWrite(9,LOW);
         delay(closestage2);
         digitalWrite(9,HIGH);
         }  
         else if (temp <= tempset - tempclose && deshumstate == 0){ //fermeture des toits si le cycle de deshum n'est pas activé
         digitalWrite(9,LOW);
         delay(closestage1);
         digitalWrite(9,HIGH);
         }
     }
 }
  
void heatercontrol(){
     
  float h = dht2.readHumidity(); // capteur DHT22 serre 6
  float temp = dht2.readTemperature(); // capteur DHT22 serre 6
  
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  
     if (hour >= startnight or hour < endnight){ // heures considérées pour la nuit
       tempheat = tempheatnight;
     }
     
     if (hour >=startdip && hour <enddip){ // heures considérées pour le dip
        tempheat = tempheatdip;
     }
     
     if(hour >=startday && hour <endday){  // heures considérées pour le jour
       tempheat = tempheatday;
     }
       
     if (temp <= tempset - tempheat - heaton){ // temperature limite pour consigne de chauffage
     digitalWrite(6,LOW);
     digitalWrite(7,LOW);
     digitalWrite(10,LOW);
     digitalWrite(3,LOW);
     heatstate = 1;
     }
     
     if (temp > tempset - tempheat + heatoff){  // fermeture du chauffage si temp plus haute que tempset - différence de chauffage
     digitalWrite(6,HIGH);
     digitalWrite(7,HIGH);
     digitalWrite(10,HIGH);
     digitalWrite(3,HIGH);
     heatstate = 0;
     }
     
    }
