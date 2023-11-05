#include <SoftwareSerial.h>
#include <DHT.h>

#define DHTPIN A0 //DHT11 Sensor connected to A0 Pin
#define DHTTYPE DHT11 //DHT11 Sensor declared
#define WATERPIN 10
#define SOILSENSOR A1


#define MAXDRYNESS 666
#define MAXWETNESS 333 
#define MAXTEMP 25

#define PUMP_ON_TIME_SENSOR 5000
#define PUMP_OFF_HYSTERESIS 5000
#define PUMP_ON_TIME_SMS 15000
#define PUMP_OFF_TIME_SMS 120000

void updateSerial();
SoftwareSerial SIM900(9, 8); //RX, TX
DHT dht(DHTPIN, DHTTYPE); // Initialize DHT Sensor for Arduino


float hum; //Stores humidity value
float temp; //Stores temperature value
int soilval; //stores soil dryness level

String message;
String sms_r;
String sms_s;

enum STATE{
  IDLE,
  PUMP_ON_SENSOR,
  WAIT_TO_OFF,
  PUMP_OFF_WAIT, 
  PUMP_ON_SMS,
  WAIT_TO_OFF_SMS,
  SEND_USER_DATA
};

STATE current_state = IDLE;
unsigned long timer = 0;

void setup(){
  pinMode(WATERPIN, OUTPUT);
  digitalWrite(WATERPIN, LOW);
  Serial.begin(9600);    // Setting the baud rate of Serial Monitor (Arduino)
  SIM900.begin(9600);   // Setting the baud rate of GSM Module  
  dht.begin();
  delay(10000);  // Give time to log on to network
  Serial.println("Initialising GSM900a.");
  delay(1500);
  SIM900.print("AT\r"); //Handshaking with SIM900
  updateSerial();
  SIM900.print("AT+CSQ\r"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  SIM900.print("AT+CCID\r"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();
  SIM900.print("AT+CREG?\r"); //Check whether it has registered in the network
  updateSerial();
  SIM900.print("AT+COPS?\r"); // Print out carrier name
  updateSerial();
  SIM900.print("AT+CMGF=1\r");  // Setting SIM900 to SMS Mode
  updateSerial();
  SIM900.println("AT+CNMI=2,2,0,0,0\r"); // AT command to receive a live message
  updateSerial();
}

void loop(){
  sms_r="";
  switch(current_state){
    case IDLE: {
      if(SIM900.available()>0) {
        sms_r = SIM900.readString();
        Serial.print(sms_r);    
        delay(100);
      } 

      hum = dht.readHumidity();
      temp= dht.readTemperature();
      soilval = analogRead(SOILSENSOR); // Reading Soil Moisture Sensor
      Serial.println("Moisture="+String(soilval));
      Serial.println("TEmp="+String(temp));
      Serial.println("Humidity="+String(hum));
      if(sms_r.indexOf("check sensors")>=0){ //If the sms does not match this string, returns -1
        current_state = SEND_USER_DATA;
      }
      else if(sms_r.indexOf("pump on")>=0){
        current_state = PUMP_ON_SMS;
      }
      else if(sms_r.indexOf("pump off")>=0){
        current_state = WAIT_TO_OFF_SMS;
        timer=millis();
      }
      else if(soilval>MAXDRYNESS || (temp>MAXTEMP && soilval<=MAXWETNESS)){
        current_state = PUMP_ON_SENSOR;
      }
      else if(soilval>MAXDRYNESS || (temp>MAXTEMP && soilval>=MAXWETNESS))
      {
        current_state = PUMP_ON_SENSOR;
      }
      break;
    }
    case PUMP_ON_SENSOR: {
      message = "Dehydrating conditions, PUMP ON.";
      Serial.println("Update:Dehydrating conditions, PUMP ON.");
      sendSMS(message);
      digitalWrite(WATERPIN, HIGH);
      current_state = WAIT_TO_OFF;
      timer = millis()+PUMP_ON_TIME_SENSOR;
      break;
    }
    case WAIT_TO_OFF: {
      if(millis()>timer){
        Serial.println("Update: Soil Hydrated, PUMP OFF");
        digitalWrite(WATERPIN, LOW);
        current_state = PUMP_OFF_WAIT;
        timer = millis()+PUMP_OFF_HYSTERESIS;
      }
      break;
    }
    case PUMP_OFF_WAIT: {
      if(millis()>timer){
        current_state = IDLE;
      }
      break;
    }
    case PUMP_ON_SMS: {
      message = "PUMP ON BY SMS";
      Serial.println("Update: PUMP ON BY SMS");
      sendSMS(message);
      digitalWrite(WATERPIN, HIGH);
      current_state = WAIT_TO_OFF_SMS;
      timer = millis()+PUMP_ON_TIME_SMS;
      break;
    }
    case WAIT_TO_OFF_SMS: {
      if(SIM900.available()>0){
        sms_r = SIM900.readString();
        Serial.print(sms_r);    
        delay(100);
      } 
      if(millis()>timer || sms_r.indexOf("pump off")>=0){
        digitalWrite(WATERPIN, LOW);
        current_state = PUMP_OFF_WAIT;
        message = "PUMP OFF";
        Serial.println("Update: PUMP OFF");
        sendSMS(message);
        timer = millis()+PUMP_OFF_TIME_SMS;
      }
      break;
    }
    case SEND_USER_DATA: {
      message = "Temperature=" + String(temp) + "*C, Humidity=" + String(hum) + ", Soil Moisture= " + String(soilval);
      sendSMS(message);
      Serial.println(message); 
      current_state = IDLE;
      break;
    }
  }
}

void sendSMS(String sms_s){
  SIM900.print("AT+CMGF=1\r"); // AT command to set SIM900 to SMS mode 
  delay(100);
  SIM900.print("AT+CMGS=\"+91xxxxxxxxxx\"\r");// change to your sim900's your phone number 
  delay(100);
  SIM900.println(sms_s);// Send the SMS 
  delay(100);
  SIM900.print((char)26);// End AT command with a ^Z, ASCII code 26 
  delay(100);
  SIM900.println();  // Give module time to send SMS
  delay(100);
  SIM900.print("AT+CNMI=2,2,0,0,0\r"); // AT command to receive a live message
  delay(100);
}

void updateSerial(){
  delay(500);
  while(SIM900.available()) 
  {
    Serial.write(SIM900.read());//Forward what Software Serial received to Serial Port
  }
}
