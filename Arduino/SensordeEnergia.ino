/**
  * Description: This code has been only tested in Intel Galileo Gen 1 and gets the readings of the next sensors:
        * TSL2561 - Light sensor
        * MPL115A - Temperature and pressure sensor
        * MAX4466 - Noise sensor
        * HTU21DF - Humidity sensor
        * Grove multichanel gas sensor
        * SCT-013-000 - Electric current sensor
    Then it calls the python scripts to send the data to the server via MQTT. All readings are writen to a file
    located in the SD card, located by default in '/media/mmcblk0p1/'. The SD functions were taken from:
    https://gist.github.com/carlynorama/9316252, we only override writing methods so it could support multiple
    data types.

  * Author:       Gustavo Adrián Jiménez González (Ruxaxup)
  * Date:         03 - 27 - 2017
  * Version:      1.0.0
  * Contact:      gustavo.jim.gonz@gmail.com
**/

#include <Wire.h>
#include <SD.h>
#include "EmonLib.h"

#define POTENCIA1 0X20
#define POTENCIA 0X40

#define ALL_CONNECTED 48
#define DEBUG 0

//FRECUENCIA DE MUESTREO
#define SECOND         			1000
#define CONNECTION_DELAY        SECOND * 1 		// 1 second
#define TIME_POTENCIA 			1              	//seconds
#define TIME_DISC       		SECOND * 2     	//Time for connecting sensors
//LEDS
#define LED_ERROR 13
#define LED_WARNING 12
#define LED_OK 11  

//SENSOR LEYENDO
#define LED_SENS_1 10
#define LED_SENS_2 9
#define LED_SENS_3 8

//PINES SENSORES
#define CURRENT_PIN	1
#define CURRENT_PIN2	2   /*S2*/


byte readingFlag = 0x0;

byte warningFlag = 0x0;

byte estatus_sensores, estatus_anterior;

typedef struct {
  double current, current1;  /*S2*/
  double power,power1;   /*S2*/
} Mediciones;

enum states {
  start,
  idle,
  readSensors,
  sendMetadata,
  sendDataMQTT,
  error,
  checkRanges,
  connectSensors,
  idleIterator
};

states state = start;

//Energy
EnergyMonitor emon1,emon2;  /*S2*/

Mediciones m;


/////////////   MQTT    ///////////////




void blinkERRORLed(int times)
{
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_ERROR, HIGH);
    delay(250);
    digitalWrite(LED_ERROR, LOW);
    delay(250);
  }
}

void blinkConnectionLEDS(int times)
{
  for (int i = 0; i < times; i++) {
    digitalWrite (LED_SENS_1, HIGH);
    digitalWrite (LED_SENS_2, HIGH);
    digitalWrite (LED_SENS_3, HIGH);
    delay(100);
    digitalWrite (LED_SENS_1, LOW);
    digitalWrite (LED_SENS_2, LOW);
    digitalWrite (LED_SENS_3, LOW);
    delay(100);
    digitalWrite (LED_ERROR,  HIGH);
    digitalWrite (LED_WARNING, HIGH);
    digitalWrite (LED_OK, HIGH);
    delay(100);
    digitalWrite (LED_ERROR, LOW);
    digitalWrite (LED_WARNING, LOW);
    digitalWrite (LED_OK, LOW);
    delay(100);
  }
}

void _sendMetaData(String status) {
  char data[30];

  String path = String("python /home/root/PythonMQTT/sendMetadata.py " + status + " " + estatus_sensores);
  char command [path.length() + 1  ];
  path.toCharArray(command, sizeof(command));
  if (DEBUG == 1) Serial.println(command);
  system(command);
}

void _sendDataMQTT(String tipoLectura, String status) {
  char data[25];
  String path = String("python /home/root/PythonMQTT/sendDataMQTT.py " + tipoLectura + " " + status);
  galileoCreateFile("values.txt");
  if (DEBUG == 1) Serial.println(tipoLectura);
  if (tipoLectura.equals("power")) {
    addValueToFile("values.txt", m.current /* 130.8*/); // VALOR MODIFICADO - CALIBRACIÓN NUEVA
  } 
  if (tipoLectura.equals("power1")) {
    addValueToFile("values.txt", m.current1 /* 130.8*/); // VALOR MODIFICADO - CALIBRACIÓN NUEVA
  }                                                                                             /*S2*/
  char command [path.length() + 1  ];
  path.toCharArray(command, sizeof(command));
  if (DEBUG == 1) Serial.println(command);
  system(command);
}

////////////    MQTT    ///////////////


void clearData()
{
  m.current = 0;
  m.current1 = 0;/*S2*/
}

////////////////////////////////////
boolean getCurrent()
{
  //Calcula a corrente
  //I = 0.28, P = 25 kW
  double Irms = emon1.calcIrms(1480);
  m.current = Irms;
  return true;

}

boolean getCurrent1()          /*S2*/
{
  double Irms = emon2.calcIrms(1480);
  m.current1 = Irms;
  return true;
}

//////////////////////////////////////////////////////
//boolean getCurrent1()
//{
  //Calcula a corrente
  //I = 0.28, P = 25 kW
  //double Irms1 = emon1.calcIrms1(5860);
  //1 = Irms1;
  //return true;
//}
//////////////////////////////////////////////////////

//////////// Rangos ////////////////

void turnOnStatusLeds(byte sensor)
{
  switch (sensor) {
    case POTENCIA:
      if ((warningFlag & POTENCIA) == POTENCIA) {
        digitalWrite (LED_WARNING, HIGH);
        digitalWrite (LED_OK, LOW);
        digitalWrite (LED_ERROR, LOW);
      }
      else {
        digitalWrite(LED_WARNING, LOW);
        digitalWrite (LED_ERROR, LOW);
        digitalWrite(LED_OK, HIGH);

      }
      break;
      case POTENCIA1:
      if ((warningFlag & POTENCIA1) == POTENCIA1) {
        digitalWrite (LED_WARNING, HIGH);
        digitalWrite (LED_OK, LOW);
        digitalWrite (LED_ERROR, LOW);
      }
      else {
        digitalWrite(LED_WARNING, LOW);
        digitalWrite (LED_ERROR, LOW);
        digitalWrite(LED_OK, HIGH);

      }
      break;
  }
}

void turnOnSensorLeds(byte sensor)
{
  switch (sensor) {
    case POTENCIA:
      digitalWrite (LED_SENS_1, HIGH);
      digitalWrite (LED_SENS_2, HIGH);
      digitalWrite (LED_SENS_3, HIGH);

      break;
      case POTENCIA1:
      digitalWrite (LED_SENS_1, HIGH);
      digitalWrite (LED_SENS_2, HIGH);
      digitalWrite (LED_SENS_3, HIGH);

      break;
  }

}

void resetLeds()
{
  delay(SECOND);
  digitalWrite (LED_SENS_1, LOW);
  digitalWrite (LED_SENS_2, LOW);
  digitalWrite (LED_SENS_3, LOW);
  digitalWrite(LED_WARNING, LOW);
  digitalWrite (LED_ERROR, LOW);
  digitalWrite(LED_OK, LOW);

}


void check_ranges()
{
  warningFlag = 0;
  if ( (estatus_sensores & POTENCIA) == POTENCIA ) {
    if (m.current > 100 || m.current <= 0) {
      warningFlag = warningFlag | POTENCIA;
    }
  }
  if ( (estatus_sensores & POTENCIA1) == POTENCIA1 ) {
    if (m.current1 > 100 || m.current1 <= 0) {     /*S2*/    //REVISAR
      warningFlag = warningFlag | POTENCIA1;
    }
  }  

}

void check_sensors()

{
  estatus_anterior = estatus_sensores;
  estatus_sensores = 0;
  if (getCurrent()) {
    estatus_sensores = estatus_sensores | POTENCIA;
  }
  else {
    if (DEBUG == 1)Serial.println("Sensor de potencia1 desconectado");
  }
  
  if (getCurrent1()) {
    estatus_sensores = estatus_sensores | POTENCIA1;
  }
  else {
    if (DEBUG == 1)Serial.println("Sensor de potencia2 desconectado");
  }
}

void calibrateCurrentSensor()
{
  emon1.calcIrms(5860);
  emon1.calcIrms(5860);
  emon1.calcIrms(5860);
  emon1.calcIrms(5860);
  emon1.calcIrms(5860);
}
void calibrateCurrentSensor1()
{
  emon2.calcIrms(1480);
  emon2.calcIrms(1480);
  emon2.calcIrms(1480);
  emon2.calcIrms(1480);
  emon2.calcIrms(1480);
}

void setup(void)
{

  Serial.begin(9600);
  //Current
  //Pino, calibracao - Cur Const= Ratio/BurdenR. 1800/62 = 29.

  
  emon1.current(1, 504);
  //emon1.current(CURRENT_PIN, 14.00);
  calibrateCurrentSensor();

  /*S2*/
  emon2.current(2, 504);
  calibrateCurrentSensor1();
  
  pinMode(LED_ERROR, OUTPUT);
  digitalWrite(LED_ERROR, LOW);
  pinMode(LED_WARNING, OUTPUT);
  digitalWrite(LED_WARNING, LOW);
  pinMode(LED_OK, OUTPUT);
  digitalWrite(LED_OK, LOW);
  pinMode(LED_SENS_1, OUTPUT);
  digitalWrite(LED_SENS_1, LOW);
  pinMode(LED_SENS_2, OUTPUT);
  digitalWrite(LED_SENS_2, LOW);
  pinMode(LED_SENS_3, OUTPUT);
  digitalWrite(LED_SENS_3, LOW);

  //Indicador de arranque
  turnOnSensorLeds(POTENCIA);
  resetLeds();
  turnOnSensorLeds(POTENCIA);
  resetLeds();
  
  turnOnSensorLeds(POTENCIA1);
  resetLeds();
  turnOnSensorLeds(POTENCIA1);
  resetLeds();

  estatus_sensores = 0;
}

/******************************** SD **********************************/
//SD.open retrieves the file in append more.
void addValueToFile(String fileName, float content) {
  char charFileName[fileName.length() + 1];
  fileName.toCharArray(charFileName, sizeof(charFileName));

  if (SD.exists(charFileName)) {
    File targetFile = SD.open(charFileName, FILE_WRITE);
    targetFile.print(content);
    targetFile.print("\n");
    targetFile.close();
  }
}

void addValueToFile(String fileName, double content) {
  char charFileName[fileName.length() + 1];
  fileName.toCharArray(charFileName, sizeof(charFileName));

  if (SD.exists(charFileName)) {
    File targetFile = SD.open(charFileName, FILE_WRITE);
    targetFile.print(content);
    targetFile.print("\n");
    targetFile.close();
  }
}

void addValueToFile(String fileName, uint32_t content) {
  char charFileName[fileName.length() + 1];
  fileName.toCharArray(charFileName, sizeof(charFileName));

  if (SD.exists(charFileName)) {
    File targetFile = SD.open(charFileName, FILE_WRITE);
    targetFile.print(content);
  }
}

void galileoCreateFile(String fileName) {
  String status_message = String();
  status_message = fileName;
  char charFileName[fileName.length() + 1];
  fileName.toCharArray(charFileName, sizeof(charFileName));

  if (SD.exists(charFileName)) {
    status_message += " exists already.";
  }
  else {
    char system_message[256];
    char directory[] = "/media/realroot";
    sprintf(system_message, "touch %s/%s", directory, charFileName);
    system(system_message);
    if (SD.exists(charFileName)) {
      status_message += " created.";
    }
    else {
      status_message += " creation tried and failed.";
    }
  }
}

/**********************************************************************/

int cont_current = 0;
int cont_current1 = 0;   /*S2*/
int cont_disconnected = 0;

void loop(void)
{
  String tipoLectura = String();

  switch (state) {

    case start:
      check_sensors();
      if (estatus_sensores == 0) {
        state = error;
      } else {
        if (estatus_sensores != estatus_anterior) {
          if (DEBUG == 1) Serial.println("Hubo cambios");
          state = sendMetadata;
        } else {
          if (DEBUG == 1) Serial.println("No hubo cambios");
          state = idle;
        }
      }


      break;

    case idle:
      if (DEBUG == 1) Serial.println("state:Duermo 1 min");
      delay(SECOND);
      if ( (estatus_sensores & POTENCIA) == POTENCIA ) {
        cont_current++;
      } else {
        cont_current = 0;
      }
      
      if ( (estatus_sensores & POTENCIA1) == POTENCIA1) {    /*S2*/
        cont_current1++;
      } else {
        cont_current1 = 0;
      }

      if (DEBUG == 1) {
        Serial.print("cont_current = ");
        Serial.println(cont_current);
      }
      if (DEBUG == 1) {
        Serial.print("cont_current2 = ");
        Serial.println(cont_current1);
      }
      if (DEBUG == 1) Serial.println("state:Idle");
      
      if (cont_current >= TIME_POTENCIA) {
        readingFlag = readingFlag | POTENCIA;
      }
      
      if (cont_current1 >= TIME_POTENCIA) {   /*s2*/
        readingFlag = readingFlag | POTENCIA1;
      }
      
      if (readingFlag != 0x00) {
        state = readSensors;
      }

      break;

    case sendMetadata:
      if (DEBUG == 1) Serial.println("state:sendMetadata");
      _sendMetaData("S");
      state = idle;
      break;

    case sendDataMQTT:
      if (DEBUG == 1) Serial.println("state:sendDataMQTT");
     
      if ( (readingFlag & POTENCIA) == POTENCIA) {
        if (DEBUG == 1)Serial.print("enviando POTENCIA electrica: ");
        if (DEBUG == 1)Serial.print(m.current /* 127*/);
        if (DEBUG == 1)Serial.println(" W");
        if (DEBUG == 1)Serial.print("***CORRIENTE electrica: ");
        if (DEBUG == 1)Serial.print(m.current);
        if (DEBUG == 1)Serial.println(" A");
        _sendDataMQTT("power", ((warningFlag & POTENCIA) == POTENCIA) ? "W" : "K");
        cont_current = 0;
        turnOnSensorLeds(POTENCIA);
        turnOnStatusLeds(POTENCIA);
        resetLeds();
        readingFlag = readingFlag ^ POTENCIA;
      }
      
      if ( (readingFlag & POTENCIA1) == POTENCIA1) {
        if (DEBUG == 1)Serial.print("enviando POTENCIA2 electrica: ");
        if (DEBUG == 1)Serial.print(m.current1 /* 127*/);
        if (DEBUG == 1)Serial.println(" W");
        if (DEBUG == 1)Serial.print("***CORRIENTE electrica: ");
        if (DEBUG == 1)Serial.print(m.current1);
        if (DEBUG == 1)Serial.println(" A");
        _sendDataMQTT("power1", ((warningFlag & POTENCIA1) == POTENCIA1) ? "W" : "K");
        cont_current1 = 0;
        
        turnOnSensorLeds(POTENCIA1);
        turnOnStatusLeds(POTENCIA1);
        resetLeds();
        readingFlag = readingFlag ^ POTENCIA1;
      }
      
      clearData();
      if (DEBUG == 1)Serial.print("SENSORS STATUS: ");
      if (DEBUG == 1)Serial.println(estatus_sensores);
      if (false) {
        if (DEBUG == 1)Serial.println("CONNECT SENSOR");
        state = connectSensors;
      } else {
        state = start;
      }

      break;

    case readSensors:
      if (DEBUG == 1) Serial.println("state:readSensors");
      state = checkRanges;
      break;

    case checkRanges:
      check_ranges();
      state = sendDataMQTT;
      break;

    case error:
      if (DEBUG == 1) Serial.println("state:Error");
      blinkERRORLed(10);
      cont_disconnected++;
      if (cont_disconnected == TIME_DISC) {
        cont_disconnected = 0;
        state = start;
      } else {
        delay(CONNECTION_DELAY);
      }

      break;

    case connectSensors:
      blinkConnectionLEDS(5);
      cont_disconnected++;
      if (cont_disconnected == TIME_DISC) {
        cont_disconnected = 0;
        state = start;
      } else {
        delay(CONNECTION_DELAY);
      }
      break;

    case idleIterator:

      break;

    default:
      if (DEBUG == 1) Serial.println("state:Default");
      break;
  }

}

