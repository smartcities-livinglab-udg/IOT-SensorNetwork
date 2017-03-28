#!/usr/bin/env python

'''
	* Description: This code is a bridge between Arduino and Linux in an Intel Galileo Gen 1 board
		used for send data via MQTT. The user must specify the host and topic in order to send data.
	* Author: 	Gustavo Adrián Jiménez González (Ruxaxup)
	* Date:		03 - 27 - 2017
	* Version:	1.0.0
	* Contact:	gustavo.jim.gonz@gmail.com
'''

import sys
import os
import paho.mqtt.publish as publish
import errno
from socket import error as socket_error

host = ""
topic = ""
lecturas = {"temp":"temperature",
			"press":"pressure",
			"light":"light",
			"noise":"noise",
			"power":"power",
			"NO2":"NO2",
			"NH3":"NH3",
			"C3H8":"C3H8",
			"C4H10":"C4H10",
			"CH4":"CH4",
			"H2":"H2",
			"C2H5OH":"C2H5OH",
			"CO":"CO",
			"hum":"humidity"}
statusDict = {"K":"okay","W":"warning","E":"error"}
gases = {0:"NH3",1:"CO",2:"NO2",3:"C3H8",4:"C4H10",5:"CH4",6:"H2",7:"C2H5OH"}
deli = ';'
if len(sys.argv)  == 3:
        status = statusDict[sys.argv[2]]
	if sys.argv[1] == "gas":
		i = 0
		readingType = ""
		valueFile = open('/media/mmcblk0p1/values.txt')
		value = valueFile.readline()
		while value != '':
			readingType = readingType + gases[i] + deli + str(value.strip('\n')) + deli
			i = i + 1
			value = valueFile.readline()
	else:
		readingType = lecturas[sys.argv[1]]
		valueFile = open ('/media/mmcblk0p1/values.txt')
		value = valueFile.readline()
		readingType = readingType + deli + str(value.strip('\n')) + deli
	f = open('/sys/class/net/eth0/address','r')
	mac = f.readline()
	message = readingType  + "macAddress" + deli + mac.strip('\n') + deli +  "status" + deli + status
	print message
	f.close()
	valueFile.close()
	os.remove('/media/mmcblk0p1/values.txt')
	try:
		publish.single(topic, message, hostname=host)
		publish.single("board/" + mac.strip('\n'), message, hostname=host)
	except socket_error as serr:
		print "No internet connection."
