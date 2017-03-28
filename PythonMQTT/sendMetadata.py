#!/usr/bin/env python

'''
        * Description: This code is a bridge between Arduino and Linux in an Intel Galileo Gen 1 board
                used for send data via MQTT. The user must specify the host and topic in order to send data.
        * Author:       Gustavo Adrián Jiménez González (Ruxaxup)
        * Date:         03 - 27 - 2017
        * Version:      1.0.0
        * Contact:      gustavo.jim.gonz@gmail.com
'''

import sys
import os
import paho.mqtt.publish as publish
import errno
from socket import error as socket_error

host = ""
topic = ""
idSensor = {4:"temperature",5:"pressure",6:"light",3:"noise",0:"power",2:"gas",1:"humidity"}
readings = {"temperature":"C",
			"pressure":"kPa",
			"light":"lumens",
			"noise":"dB",
			"power":"W",
			"gas":"ppm",
			"humidity":"%"}
gases = {0:"NH3",1:"CO",2:"NO2",3:"C3H8",4:"C4H10",5:"CH4",6:"H2",7:"C2H5OH"}
statusDict = {"U":"update","S":"start"}
deli = ';'

def buildMetaData():
	binary = open('binary.txt','w')
        binario = str(bin(int(sys.argv[2]))[2:])
	binary.write(binario)
	binary.close()
        binSize = len(binario)
        diferencia = 7 - binSize
        #Llena la cadena con ceros
        for i in range(0,diferencia):
                binario = '0' + binario
	print "Binary string: " + binario
	sensorsString = ""
        for x in range(0,7):
		print str(x) + " " + idSensor[x] + " -- " + binario[x]
                if binario[x] != '0':
			if idSensor[x] == "gas":
				sensorsString = sensorsString + "setIni" + deli
				for gas in range(0,8):
					sensorsString = sensorsString + gases[gas] + deli + readings[idSensor[x]] + deli
				sensorsString = sensorsString + "setEnd" + deli
			else:
                        	sensorsString = sensorsString + idSensor[x] + deli + readings[idSensor[x]] + deli
        return sensorsString

if len(sys.argv)  == 3:    
        mensaje = buildMetaData()
        f = open('/sys/class/net/eth0/address','r')
	mac = f.readline()
        mensaje = mensaje + "macAddress" + deli + mac.strip('\n') + deli + "status" + deli + statusDict[sys.argv[1]]
        print mensaje
        f.close()
        try:
                publish.single(topic, mensaje, hostname=host)
        except socket_error as serr:
                print "No internet connection."
else:
        print "3 arguments are needed."
