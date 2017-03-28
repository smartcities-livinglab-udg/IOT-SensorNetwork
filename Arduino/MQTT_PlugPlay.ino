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
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <Adafruit_MPL115A2.h>
#include "MutichannelGasSensor.h"
#include "Adafruit_HTU21DF.h"
#include "EmonLib.h"

#define LUZ 0x01
#define PRESION 0x02
#define TEMPERATURA 0X04
#define RUIDO 0X08
#define GAS 0x10
#define HUMEDAD 0X20
#define POTENCIA 0X40

#define ALL_CONNECTED 48
#define DEBUG 0

//FRECUENCIA DE MUESTREO
#define SECOND         			1000
#define CONNECTION_DELAY        SECOND * 1 		// 1 second
#define TIME_TEMP 				1             	//seconds
#define TIME_PRESS 				1             	//seconds
#define TIME_LIGHT 				1              	//seconds
#define TIME_HUM 				1              	//seconds
#define TIME_GAS				1              	//seconds
#define TIME_POTENCIA 			1              	//seconds
#define TIME_NOISE 				1              	//seconds
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
#define NOISE_PIN	0
#define CURRENT_PIN	1


byte readingFlag = 0x0;

byte warningFlag = 0x0;

byte estatus_sensores, estatus_anterior;

typedef struct{
  float NH3;
  float CO;
  float NO2;
  float C3H8;
  float C4H10;
  float CH4;
  float H2;
  float C2H5OH;
  float pressureKPA;
  double decibelios;
  float Luz;
  float temp;
  float hum;
  double current;
  double power;  
}Mediciones;

enum states{
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

//Temperature & Humidty
Adafruit_HTU21DF htu = Adafruit_HTU21DF();

//Noise
const int sampleWindow = 50; // Ventana de muestra en ms (50 mS = 20Hz)
unsigned int sample;

//Pressure
Adafruit_MPL115A2 mpl115a2;

//Light  
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

//Energy
EnergyMonitor emon1;

Mediciones m;


/////////////   MQTT    ///////////////
void _sendMetaData(String status){
    char data[30];

    String path = String("python /home/root/PythonMQTT/sendMetadata.py "+ status + " " + estatus_sensores);
    char command [path.length() + 1  ];
    path.toCharArray(command, sizeof(command)); 
    if(DEBUG == 1) Serial.println(command);
    system(command);
}

void _sendDataMQTT(String tipoLectura, String status){
    char data[25];
    String path = String("python /home/root/PythonMQTT/sendDataMQTT.py " + tipoLectura + " " + status);
    galileoCreateFile("values.txt");
    if(DEBUG == 1) Serial.println(tipoLectura);
    if(tipoLectura.equals("temp")){
        addValueToFile("values.txt",m.temp);
    }else if(tipoLectura.equals("press")){
        addValueToFile("values.txt",m.pressureKPA);
    }else if(tipoLectura.equals("light")){
        addValueToFile("values.txt",m.Luz);
    }else if(tipoLectura.equals("noise")){
        addValueToFile("values.txt",m.decibelios);
    }else if(tipoLectura.equals("gas")){
        addValueToFile("values.txt",m.NH3);
        addValueToFile("values.txt",m.CO);
        addValueToFile("values.txt",m.NO2);
        addValueToFile("values.txt",m.C3H8);
        addValueToFile("values.txt",m.C4H10);
        addValueToFile("values.txt",m.CH4);
        addValueToFile("values.txt",m.H2);
        addValueToFile("values.txt",m.C2H5OH);
    }else if(tipoLectura.equals("power")){
        addValueToFile("values.txt",m.current*127);
    }else if(tipoLectura.equals("hum")){
        addValueToFile("values.txt",m.hum);
    }

    char command [path.length() + 1  ];
    path.toCharArray(command, sizeof(command)); 
    if(DEBUG == 1) Serial.println(command);
    system(command);
}

////////////    MQTT    ///////////////




void blinkERRORLed(int times)
{
  for(int i = 0; i < times; i++){
    digitalWrite(LED_ERROR,HIGH);
    delay(250);
    digitalWrite(LED_ERROR,LOW);
    delay(250);
  }      
}

void blinkConnectionLEDS(int times)
{
  for(int i = 0; i < times; i++){
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

void clearData()
{
  m.NH3 = 0;
  m.CO = 0;
  m.NO2 = 0;
  m.C3H8 = 0;
  m.C4H10 = 0;
  m.CH4 = 0;
  m.H2 = 0;
  m.C2H5OH = 0;
  m.pressureKPA = 0;
  m.decibelios = 0;
  m.Luz = 0;
  m.temp = 0;
  m.hum = 0; 
  m.current = 0;
}

////////////////////////////////////

void _readSensors()
{  
     if( (readingFlag & TEMPERATURA) == TEMPERATURA){
        getTempHum();
      }
      if( (readingFlag & PRESION) == PRESION){
        getPresion();
      }
      if( (readingFlag & LUZ) == LUZ){
       getLuz();
       
      }
      if( (readingFlag & RUIDO) == RUIDO){
       getRuido();
      }
      if( (readingFlag & GAS) == GAS){
       getGas();
      }
      if( (readingFlag & HUMEDAD) == HUMEDAD){
       getTempHum();
      }
}

boolean getCurrent()
{
	//Calcula a corrente  
	//I = 0.28, P = 25 kW
  double Irms = emon1.calcIrms(1480);
  m.current = Irms;
  return true;

}

boolean getLuz()

{
  sensors_event_t event;
  tsl.getEvent(&event);
    
  if (event.light>65000)
  {
  return false;  
  } 
  else if (event.light>=0 && event.light<65000) {
    m.Luz=event.light;
    return true;
  }
}

boolean getGas()
{
  if((estatus_sensores & GAS) == 0) mutichannelGasSensor.powerOn();
  m.NH3 = mutichannelGasSensor.measure_NH3();
    if(DEBUG == 1)Serial.println(m.NH3);
    if(m.NH3 < 0.0)
  {
     return false;
    }

  m.CO = mutichannelGasSensor.measure_CO();
   if(m.CO < 0.0)
   {
   return false;
   }
   
  m.NO2 = mutichannelGasSensor.measure_NO2();
       if(m.NO2 < 0.0) 
    {
      return false;
     }

 m.C3H8 = mutichannelGasSensor.measure_C3H8();
      if(m.C3H8 < 0.0)
    {
     return false;
    }

  m.C4H10 = mutichannelGasSensor.measure_C4H10();
      if(m.C4H10 < 0.0)
    {
      return false;
    }

  m.CH4 = mutichannelGasSensor.measure_CH4();
      if(m.CH4  < 0.0)
    {
      return false;
    }
 
  m.H2 = mutichannelGasSensor.measure_H2();
      if(m.H2  < 0.0)
    {
      return false;
    }

  m.C2H5OH = mutichannelGasSensor.measure_C2H5OH();
      if(m.C2H5OH < 0.0)
    {
      return false;
    }
    return true;
}

boolean getPresion()
{
  m.pressureKPA = mpl115a2.getPressure();  
  if(m.pressureKPA>300)
  {
    return false;
   }
  else if (m.pressureKPA>=0 && m.pressureKPA<300)
  {
    return true;
  }
}

boolean getRuido()
{
  //Ruido
  unsigned long startMillis= millis();  // Comienza la ventana de muestr, se amplían las variables de tamaño para el almacenamiento de número
  unsigned int peakToPeak = 0;   // Valor pico a pico 

  unsigned int signalMax = 0;
  unsigned int signalMin = 1024; // (0 a 5v)

  // colectar datos de  50 mS
  while (millis() - startMillis < sampleWindow)
  {
    sample = analogRead(NOISE_PIN); //asigna voltajes de entrada entre 0 y 5 v en valores enteros entre 0 y 1023
    if (sample < 1024) 
    {
      if (sample > signalMax)
      {
        signalMax = sample;  // guardar los valores maximos
      }
      else if (sample < signalMin) //si la la muestra es menor aue la señal minima
      {
        signalMin = sample;  // guardar los valores minimos
      }
    }
  }
  peakToPeak = signalMax - signalMin;  // max - min = amplitud del valor pico a pico
  double volts = (peakToPeak * 3.3) / 1024;  // conversion de voltajes
  double decibelios = (20 * log10(volts / 10)) * -1;//Conversión a decibeles
  
  m.decibelios=decibelios;
  if(analogRead(0) <= 1)
  
  {
    return false;
  }
    return true;
}

boolean getTempHum()
{ 
if(htu.begin()) 
{
 m.temp = htu.readTemperature();
 m.hum = htu.readHumidity(); 
 return true;
}
 else return false;  
}



//////////// Rangos ////////////////

void turnOnStatusLeds(byte sensor)
{
  switch(sensor){
  case LUZ:
       if ((warningFlag & LUZ) == LUZ){
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
  
   case PRESION:
       if ((warningFlag & PRESION) == PRESION){
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
  
   case TEMPERATURA:
       if ((warningFlag & TEMPERATURA) == TEMPERATURA){
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
  
   case RUIDO:
       if ((warningFlag & RUIDO) == RUIDO){
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
  
   case GAS:
       if ((warningFlag & GAS) == GAS){
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
 
  case HUMEDAD:
       if ((warningFlag & HUMEDAD) == HUMEDAD){
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
  
   case POTENCIA:
       if ((warningFlag & POTENCIA) == POTENCIA){
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
  switch(sensor){
  case LUZ:
       digitalWrite (LED_SENS_1, LOW);
       digitalWrite (LED_SENS_2, LOW);
       digitalWrite (LED_SENS_3, HIGH);
       
  break;
  
  case PRESION:
       digitalWrite (LED_SENS_1, LOW);
       digitalWrite (LED_SENS_2, HIGH);
       digitalWrite (LED_SENS_3, LOW);
      
  break;
  
  case TEMPERATURA:
       digitalWrite (LED_SENS_1, LOW);
       digitalWrite (LED_SENS_2, HIGH);
       digitalWrite (LED_SENS_3, HIGH);
     
  break;
  
   case RUIDO:
       digitalWrite (LED_SENS_1, HIGH);
       digitalWrite (LED_SENS_2, LOW);
       digitalWrite (LED_SENS_3, LOW);
       
  break;
  
   case GAS:
       digitalWrite (LED_SENS_1, HIGH);
       digitalWrite (LED_SENS_2, LOW);
       digitalWrite (LED_SENS_3, HIGH);
       
  break;
  
   case HUMEDAD:
       digitalWrite (LED_SENS_1, HIGH);
       digitalWrite (LED_SENS_2, HIGH);
       digitalWrite (LED_SENS_3, LOW);
       
  break;
  
   case POTENCIA:
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
     if( (estatus_sensores & LUZ) == LUZ ){
      	if (m.Luz>400 || m.Luz <= 0){
           warningFlag = warningFlag | LUZ;    
         }	
      }
      
      if( (estatus_sensores & TEMPERATURA) == TEMPERATURA ){
        if (m.temp>50.00 || m.temp <= 0){
          if(DEBUG == 1)Serial.println("warning temperatura"); 
          warningFlag = warningFlag | TEMPERATURA;    
         }		
      }
      
      if( (estatus_sensores & PRESION) == PRESION ){
        if (m.pressureKPA>400 || m.pressureKPA <= 0){
           warningFlag = warningFlag | PRESION;    
         }	
      }
      
      if( (estatus_sensores & RUIDO) == RUIDO ){
        if (m.decibelios>400 || m.decibelios <= 0){
           warningFlag = warningFlag | RUIDO;    
         }	
      		
      }
      if( (estatus_sensores & GAS) == GAS){
         if (m.CO>400 || m.CO <= 0){
           warningFlag = warningFlag | GAS;    
         }		
      }
      
      if( (estatus_sensores & HUMEDAD) == HUMEDAD ){
            if (m.hum>400 || m.hum <= 0){
           warningFlag = warningFlag | HUMEDAD;    
         }	
      	
      }
     if( (estatus_sensores & POTENCIA) == POTENCIA ){
            if (m.current>30 || m.current <= 0){
           warningFlag = warningFlag | POTENCIA;    
         }	
      }
  
}

void check_sensors()

{
  estatus_anterior = estatus_sensores;
  estatus_sensores = 0;  
  if (getLuz()){
    estatus_sensores = estatus_sensores | LUZ; 
  }
    else {
      if(DEBUG == 1)Serial.println("Sensor de Luz desconectado");
  }
  if (getGas()){
     estatus_sensores = estatus_sensores | GAS;
  }
    else {
       if(DEBUG == 1)Serial.println("Sensor de Gas desconectado");
  }
  if (getPresion()){
     estatus_sensores = estatus_sensores | PRESION;
  }
     else {
        if(DEBUG == 1)Serial.println("Sensor de Presion desconectado");
  } 
  if (getRuido()){
     estatus_sensores = estatus_sensores | RUIDO;
  }
  else {
     if(DEBUG == 1)Serial.println("Sensor de Ruido desconectado");
  }
  if(getTempHum()){
    estatus_sensores = estatus_sensores | TEMPERATURA;
    estatus_sensores = estatus_sensores | HUMEDAD;
  }  
  else {
    if(DEBUG == 1)Serial.println("Sensor de Temperatura y Humedad desconectado");
  }
  if(getCurrent()){    
    estatus_sensores = estatus_sensores | POTENCIA;
  }  
  else {
    if(DEBUG == 1)Serial.println("Sensor de Temperatura y Humedad desconectado");
  }
}

void initTSLSensor(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);  
  delay(500);
  tsl.enableAutoRange(true);     //Sensor de Luz, ganania automatica
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS); 
}

void calibrateCurrentSensor()
{
	emon1.calcIrms(1480);
	emon1.calcIrms(1480);
	emon1.calcIrms(1480);
	emon1.calcIrms(1480);
	emon1.calcIrms(1480);
}

void setup(void) 
{
 
  Serial.begin(9600);
  //Sensores
  mpl115a2.begin();
  mutichannelGasSensor.begin(0x04);
//  mutichannelGasSensor.doCalibrate();
  mutichannelGasSensor.powerOn();
  tsl.begin();
  htu.begin();
  //Current
  //Pino, calibracao - Cur Const= Ratio/BurdenR. 1800/62 = 29. 
  pinMode(A1, INPUT);
  emon1.current(CURRENT_PIN, 29);
  calibrateCurrentSensor();
  initTSLSensor();
  
  pinMode(LED_ERROR, OUTPUT);
  digitalWrite(LED_ERROR,LOW);
  pinMode(LED_WARNING, OUTPUT);
  digitalWrite(LED_WARNING,LOW);
  pinMode(LED_OK, OUTPUT);    
  digitalWrite(LED_OK,LOW);
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


int cont_temp = 0;
int cont_press = 0;
int cont_light = 0;
int cont_noise = 0;
int cont_gas = 0;
int cont_hum = 0;
int cont_current = 0;
int cont_disconnected = 0;

  void loop(void) 
{  
  String tipoLectura = String();
  
  switch(state){
    
    case start:
    check_sensors();
    if(estatus_sensores == 0){
      state = error;
    }else{
      if(estatus_sensores != estatus_anterior){
        if(DEBUG == 1) Serial.println("Hubo cambios");
        state = sendMetadata;
      }else{
        if(DEBUG == 1) Serial.println("No hubo cambios");
        state = idle;
      }
    }
    
   
    break;
    
   case idle:
      if(DEBUG == 1) Serial.println("state:Duermo 1 min");
      
      if( (estatus_sensores & LUZ) == LUZ ){
      	cont_light++;	
      }else{
        cont_light = 0;
      }
      if( (estatus_sensores & TEMPERATURA) == TEMPERATURA ){
      	cont_temp++;	
      }else{
        cont_temp = 0;
      }
      if( (estatus_sensores & PRESION) == PRESION ){
      	cont_press++;	
      }else{
        cont_press = 0;
      }
      if( (estatus_sensores & RUIDO) == RUIDO ){
      	cont_noise++;	
      }else{
        cont_noise = 0;
      }
      if( (estatus_sensores & GAS) == GAS){
      	cont_gas++;	
      }else{
        cont_gas = 0;
      }
      if( (estatus_sensores & HUMEDAD) == HUMEDAD ){
      	cont_hum++;	
      }else{
        cont_hum = 0;
      }
      if( (estatus_sensores & POTENCIA) == POTENCIA ){
      	cont_current++;	
      }else{
        cont_current = 0;
      }

      if(DEBUG == 1){
        Serial.print("cont_temp = ");
        Serial.println(cont_temp); 
        Serial.print("cont_press = ");
        Serial.println(cont_press);
        Serial.print("cont_light = ");
        Serial.println(cont_light);
        Serial.print("cont_noise = ");
        Serial.println(cont_noise);
        Serial.print("cont_gas = ");
        Serial.println(cont_gas);
        Serial.print("cont_hum = ");
        Serial.println(cont_hum);
        Serial.print("cont_current = ");
        Serial.println(cont_current);
      }
      if(DEBUG == 1) Serial.println("state:Idle");      
      if(cont_temp >= TIME_TEMP){
        readingFlag = readingFlag | TEMPERATURA;        
      }
      if(cont_press >= TIME_PRESS){
        readingFlag = readingFlag | PRESION;
      }
      if(cont_light >= TIME_LIGHT){
        readingFlag = readingFlag | LUZ;
      }
      if(cont_noise >= TIME_NOISE){
        readingFlag = readingFlag | RUIDO;
      }
      if(cont_gas >= TIME_GAS){
        readingFlag = readingFlag | GAS;
      }
      if(cont_hum >= TIME_HUM){
        readingFlag = readingFlag | HUMEDAD;
      }
      if(cont_current >= TIME_POTENCIA){
        readingFlag = readingFlag | POTENCIA;
      }
      if(readingFlag != 0x00){
        state = readSensors;
      }      
      
    break;
    
    case sendMetadata:
      if(DEBUG == 1) Serial.println("state:sendMetadata");
      _sendMetaData("S");
      state = idle;
    break;
    
    case sendDataMQTT:
      if(DEBUG == 1) Serial.println("state:sendDataMQTT");
      
      if( (readingFlag & TEMPERATURA) == TEMPERATURA){
        if(DEBUG == 1)Serial.print("enviando Temperatura: ");
        if(DEBUG == 1)Serial.print(m.temp); if(DEBUG == 1)Serial.println(" *C");
        cont_temp = 0;
       _sendDataMQTT("temp",((warningFlag&TEMPERATURA)==TEMPERATURA)?"W":"K");
       turnOnSensorLeds(TEMPERATURA);
       turnOnStatusLeds(TEMPERATURA);
       resetLeds();
       readingFlag = readingFlag ^ TEMPERATURA;
      }
      if( (readingFlag & PRESION) == PRESION){
        if(DEBUG == 1)Serial.print("enviando Presion: ");
        if(DEBUG == 1)Serial.print(m.pressureKPA); if(DEBUG == 1)Serial.println(" KPA");
       _sendDataMQTT("press",((warningFlag&PRESION)==PRESION)?"W":"K");
       cont_press = 0;
       turnOnSensorLeds(PRESION);
       turnOnStatusLeds(PRESION);
       resetLeds();
       readingFlag = readingFlag ^ PRESION;
      }
      if( (readingFlag & LUZ) == LUZ){
       if(DEBUG == 1)Serial.print("enviando Luz: "); 
       if(DEBUG == 1)Serial.print(m.Luz); if(DEBUG == 1)Serial.println(" luxes");
       _sendDataMQTT("light",((warningFlag&LUZ)==LUZ)?"W":"K");
       cont_light = 0;
       turnOnSensorLeds(LUZ);
       turnOnStatusLeds(LUZ);
       resetLeds();
       readingFlag = readingFlag ^ LUZ;
      }
      if( (readingFlag & RUIDO) == RUIDO){
      if(DEBUG == 1)Serial.print("enviando Ruido: ");
      if(DEBUG == 1)Serial.print(m.decibelios); if(DEBUG == 1)Serial.println(" DB");
      _sendDataMQTT("noise",((warningFlag&RUIDO)==RUIDO)?"W":"K");
      cont_noise = 0;
       turnOnSensorLeds(RUIDO);
       turnOnStatusLeds(RUIDO);
       resetLeds();
       readingFlag = readingFlag ^ RUIDO;
      }
      if( (readingFlag & GAS) == GAS){
        if(DEBUG == 1){
      Serial.print("Enviando Amoniaco: ");
      Serial.print(m.NH3); Serial.println(" ppm");
      Serial.print("Enviando Monoxido de carbono: ");
      Serial.print(m.CO); Serial.println(" ppm");
      Serial.print("Enviando Bioxido de Nitrogeno: ");
      Serial.print(m.NO2); Serial.println(" ppm");
      Serial.print("Enviando Propano: ");
      Serial.print(m.C3H8); Serial.println(" ppm");
      Serial.print("Enviando Butano: ");
      Serial.print(m.C4H10); Serial.println(" ppm");
      Serial.print("Enviando Metano: ");
      Serial.print(m.CH4); Serial.println(" ppm");
      Serial.print("Enviando Hidrogeno: ");
      Serial.print(m.H2); Serial.println(" ppm");
      Serial.print("Enviando Ethanol: ");
      Serial.print(m.C2H5OH); Serial.println(" ppm");
        }
        _sendDataMQTT("gas",((warningFlag&GAS)==GAS)?"W":"K");
      cont_gas = 0;
       turnOnSensorLeds(GAS);
       turnOnStatusLeds(GAS);
       resetLeds();
       readingFlag = readingFlag ^ GAS;
      }
      
      if( (readingFlag & HUMEDAD) == HUMEDAD){
      if(DEBUG == 1)Serial.print("enviando Humedad: "); 
      if(DEBUG == 1)Serial.print(m.hum); if(DEBUG == 1)Serial.println(" %");
      _sendDataMQTT("hum",((warningFlag&HUMEDAD)==HUMEDAD)?"W":"K");
      cont_hum = 0;
       turnOnSensorLeds(HUMEDAD);
       turnOnStatusLeds(HUMEDAD);
       resetLeds();
       readingFlag = readingFlag ^ HUMEDAD;
      }
      
      if( (readingFlag & POTENCIA) == POTENCIA) {
       if(DEBUG == 1)Serial.print("enviando POTENCIA electrica: "); 
       if(DEBUG == 1)Serial.print(m.current * 127);
       if(DEBUG == 1)Serial.println(" W");
       if(DEBUG == 1)Serial.print("***CORRIENTE electrica: ");
       if(DEBUG == 1)Serial.print(m.current);
       if(DEBUG == 1)Serial.println(" A");
       _sendDataMQTT("power",((warningFlag&POTENCIA)==POTENCIA)?"W":"K");
       cont_current = 0;
       turnOnSensorLeds(POTENCIA);
       turnOnStatusLeds(POTENCIA);
       resetLeds();
       readingFlag = readingFlag ^ POTENCIA;
    
      }
      clearData();
      if(DEBUG == 1)Serial.print("SENSORS STATUS: ");
      if(DEBUG == 1)Serial.println(estatus_sensores);
      if(false){
        if(DEBUG == 1)Serial.println("CONNECT SENSOR");
        state = connectSensors;
      }else{
        state = start;
      } 
      
    break;
    
    case readSensors:
      if(DEBUG == 1) Serial.println("state:readSensors");
      _readSensors();
      state = checkRanges;
    break;
    
    case checkRanges:
    check_ranges();
       state = sendDataMQTT;
    break;
    
    case error:
      if(DEBUG == 1) Serial.println("state:Error");
      blinkERRORLed(10);
      cont_disconnected++;
      if(cont_disconnected == TIME_DISC){
        cont_disconnected = 0;
        state = start;
      }else{
        delay(CONNECTION_DELAY);
      }
      
    break;
          
    case connectSensors:
      blinkConnectionLEDS(5);
      cont_disconnected++;
      if(cont_disconnected == TIME_DISC){
        cont_disconnected = 0;
        state = start;
      }else{
        delay(CONNECTION_DELAY);
      }
    break;

    case idleIterator:

    break;
    
    default:
      if(DEBUG == 1) Serial.println("state:Default");
    break;  
  }

}

