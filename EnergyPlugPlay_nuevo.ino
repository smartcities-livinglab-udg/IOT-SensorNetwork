/**
  * Description: This code has been only tested in Intel Galileo Gen 1 and gets the readings of the next sensors:        
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
#define DEBUG 1

//FRECUENCIA DE MUESTREO
#define SECOND         			1000
#define CONNECTION_DELAY        SECOND *1 		// 1 second
#define TIME_POTENCIA 			1              	//min
#define TIME_DISC       		SECOND * 2     	//Time for connecting sensors
//LEDS
#define LED_ERROR 13
#define LED_WARNING 12
#define LED_OK 11  

//SENSOR LEYENDO
#define LED_SENS_1 10
#define LED_SENS_2 9
#define LED_SENS_3 8


//CANTIDAD DE SENSORES DE ENERGIA
//#define ENERGY_COUNT  3
#define ENERGY_NUM  6
//VALOR DE CALIBRACION DE SENSORES DE ENERGIA
#define ENERGY_CAL    130 //para 2

byte readingFlag = 0x0;

byte warningFlag = 0x0;

byte estatus_sensores, estatus_anterior;

typedef struct {  /*S2*/
  double power[ENERGY_NUM];   /*S2*/
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

//Energy sensors
EnergyMonitor energySensors[ENERGY_NUM];  /*S2*/
//Energy IN array
int energyPINS1[] = {A0,A1, A2, A3,A4,A5};
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


void _sendDataMQTT(String tipoLectura, String status, int energyIndex) {
  char data[25];
  String path = String("python /home/root/PythonMQTT/sendDataMQTTEnergy.py " + tipoLectura + energyIndex + " " + status);
  galileoCreateFile("values.txt");
  if (DEBUG == 1) Serial.println(tipoLectura);
  if (tipoLectura.equals("power")) {
    addValueToFile("values.txt", m.power[energyIndex]*127); // VALOR MODIFICADO - CALIBRACIÓN NUEVA
  }                                                                                            /*S2*/
  char command [path.length() + 1  ];
  path.toCharArray(command, sizeof(command));
  if (DEBUG == 1) Serial.println(command);
  system(command);
  blinkERRORLed(5);
}

////////////    MQTT    ///////////////


void clearData()
{
  for(int i = 0; i < ENERGY_NUM; i++)
  {
    m.power[i] = 0.0;
  }

}

////////////////////////////////////
boolean getCurrent(EnergyMonitor em, int index)
{
  
  //Calcula a corrente
  //I = 0.28, P = 25 kW
  em.serialprint();
  Serial.println("--------------");
  double Irms = em.calcIrms(5860);
  if(Irms < 0.26) Irms = 0.0;
  m.power[index] = Irms;
  return true;

}
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
    
  }
}

void check_sensors()

{
  estatus_anterior = estatus_sensores;
  estatus_sensores = 0;
  if (getCurrent(energySensors[0], 0)) {
    estatus_sensores = estatus_sensores | POTENCIA;
  }
  else {
    if (DEBUG == 1)Serial.println("Sensor de potencia1 desconectado");
  }
}

void calibrateCurrentSensor(EnergyMonitor *em)
{
  em->calcIrms(5860);
}

void setup(void)
{
  int val=0;
  Serial.begin(9600);
  //Current
  //Pino, calibracao - Cur Const= Ratio/BurdenR. 1800/62 = 29.
  for(int i = 0; i < ENERGY_NUM; i++)
  {
    if(analogRead(energyPINS1[i])>1){
      energySensors[i].current(energyPINS1[i], ENERGY_CAL);
      energySensors[i].serialprint();
      calibrateCurrentSensor(&energySensors[i]);
    }
  }
  
  
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
    if (DEBUG == 1) Serial.println("existe");
  }
  else {
    if (DEBUG == 1) Serial.println("no existe");
    char system_message[256];
    char directory[] = "/media/realroot";
    sprintf(system_message, "touch %s/%s", directory, charFileName);
    SD.open(charFileName,FILE_WRITE);
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
      delay(SECOND * 6);
      if ( (estatus_sensores & POTENCIA) == POTENCIA ) {
        cont_current++;
      } else {
        cont_current = 0;
      }

      if (DEBUG == 1) {
        Serial.print("cont_current = ");
        Serial.println(cont_current);
      }
      if (DEBUG == 1) Serial.println("state:Idle");
      
      if (cont_current >= TIME_POTENCIA) {
        readingFlag = readingFlag | POTENCIA;
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
        for(int i = 0; i < ENERGY_NUM; i++)
        {
          if(analogRead(energyPINS1[i])>1){
            if(DEBUG == 1)
            {
              Serial.print("Enviando POTENCIA ");
              Serial.print(i);
              Serial.print(":");
              Serial.print(m.power[i] * 127);
              Serial.println(" W");
              Serial.print("***CORRIENTE electrica: ");
              Serial.print(m.power[i]);
              Serial.println(" A");
            }
            _sendDataMQTT("power", ((warningFlag & POTENCIA) == POTENCIA) ? "W" : "K", i);   
          }
       
        }

        cont_current = 0;
        turnOnSensorLeds(POTENCIA);
        turnOnStatusLeds(POTENCIA);
        resetLeds();
        readingFlag = readingFlag ^ POTENCIA;
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
      if( (readingFlag & POTENCIA) == POTENCIA)
      {
        
          if(DEBUG == 1) Serial.println("ENERGY SENSORS");
          for(int i = 0; i < ENERGY_NUM; i++)
          {
             if(analogRead(energyPINS1[i])>1){
              getCurrent(energySensors[i],i);
              if(DEBUG == 1)
              {
                Serial.print(i+1);
                Serial.print(" Sensor: ");
                Serial.println(m.power[i]);
              }
            }
          }
        
      }
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
