# IOT-SensorNetwork

## This project has only been tested with Intel Galileo Gen 1

### Setting up Intel Galileo

First of all you need to assure that your board has the latest firmware installed, if not, you might check this link to [Intel Drivers & Software Download page](https://downloadcenter.intel.com/download/26417/Intel-Galileo-Firmware-Updater-and-Drivers).

Then make sure you have an SD with at least 4 GB of capacity to install the latest Linux Operating System image. You can follow these steps in this [link](https://software.intel.com/en-us/get-started-galileo-linux-step1). This OS has installed a Python 2.7 but if it doesn't have it for any reason, make sure you can install it. **If you don't have an SD card, stop at this point and get one.**

To access the board there are many ways, via SSH or Serial Cable. The next [page](https://communities.intel.com/thread/46335) has different methods to access Galileo. If you are in Windows you can use software like Putty and WinSCP for file transfer. If you are on OSX or Linux, you can use the terminal and access via SSH or Screen for serial cable. We tested everything using a OSX with the command `screen -L /dev/cu.SLAB_USBtoUART 115200 -L` where */dev/cu.SLAB_USBtoUART* is the serial port that our computer recognized.

One you are inside you have to install the next packages to start working:

1. [Pip](https://pypi.python.org/pypi/pip) if you want to install everything faster.
2. [paho-mqtt](https://pypi.python.org/pypi/paho-mqtt/1.1) for MQTT communication protol.

Now you have to transfer or script the files of the python directory. Take note of where are you locating these files because you will need the absolute path to execute the scripts from Arduino. We created a directory called `PythonMQTT` and put the files inside, so our directoy tree looked like this:

```
home
|__root
	|__PythonMQTT
		|__sendDataMQTT.py
		|__sendMetadata.py
```

**In order to stablish a connection with MQTT, we need to specify the `host` and the `topic` inside of each script, so make sure you set up this first before moving the next step.**

Now we are ready to set up the Arduino environment.

### Setting up Arduino

We first have to download the [Arduino's software](http://www.intel.la/content/www/xl/es/support/boards-and-kits/intel-galileo-boards/000021501.html) released by Intel to work with their boards like Galileo and Edison. You can choose the latest version of the IDE and also you can find useful software inside for the board.

Now you have to [install](https://www.arduino.cc/en/Guide/Libraries) all the libraries, included in this repository, of the sensors used. If anything went wrong you can find the most updated libraries, but we are providing the ones that we tested.

The sensors are the following:
* TSL2561 - Light sensor
* MPL115A - Temperature and pressure sensor
* MAX4466 - Noise sensor
* HTU21DF - Humidity sensor
* Grove multichanel gas sensor
* SCT-013-000 - Electric current sensor

Once we installed the libraries, now we have to configure the `MQTT_PlugPlay.ino` file. There are different constants used for timing the readings of sensors, but the last release doesn't use them because we integrated the energy sensor which requires the less possible delay. So, in order to make them work, we would have to adjust these variables to **seconds**. If you wish to light be read in one minute, `TIME_LIGHT` should be *60*.
If you are testing this script for first time in Arduino's IDE, make sure that `DEBUG` is set to 1 so you can watch all information in console. But is quite **IMPORTANT** that you set this constant to 0 if you are about to release the board, so the Serial communication will not crash.
The LED constants just tell you which digital pins are you going to use.
The `CURRENT_PIN` is the Analog pin for the energy sensor.
The `NOISE_PIN` is the Analog pin for the noise sensor.
All the other sensors use SDA and SCL for I2C communication.

Now we have to specify the scripts locations. If you used the same path as we, then you don't need to change anything, but if not, take a look at line **110** and line **119** where you have to specify the absolute path to your scripts.





