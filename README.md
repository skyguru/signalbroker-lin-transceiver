# Lin bus reader and writer


ESP32-POE + Custom made PCB with MCP2004A. Read/write, act as a master or one or many slaves.

## Setup

### ESP32-POE

The schematics for this solution can be found in the KiCad folder under the appropriate card/project.
 
#### PCB

Assemble the PCB. The BOM(Bill of materials) is located under doc/PCB/KiCad. 
Use the jumper for master mode if the module shoulde be a master node.
![PCB](/doc/https://github.com/volvo-cars/signalbroker-lin-transceiver/raw/esp32/doc/PCB.jpg)

Connect the ESP32-POE to the PCB.
![Final module](/doc/Final-module.JPG)

The PCB is designed to be able to have several modules in a case/box if needed.
![Multiple modules](/doc/multiple-cards.JPG)

## Configuration

### Arduino

#### Install software

* Install arduino IDE https://www.arduino.cc/en/Main/Software

* Clone the repository and copy lin.cpp and lin.h https://github.com/AleksandarFilipov/LIN to you arduino library (pick your library folder name) folder. (Inspration: ~/arduino/arduino-1.8.5/libraries/[your libaray])

* Open the linbus/linbus.ino file in arduino ide. Select propriate port from the tools menu and the upload the software.

### Adafruit Feather 32u4

If you are using an **Adafruit Feather 32u4** you need to add Adafruit board definitions to the Arduino IDE.

`File/Preferences`:
`Additional Boards Manager URLs: https://adafruit.github.io/arduino-board-index/package_adafruit_index.json`

`Tools/Board:/Boads Manager...`
Install "`Adafruit AVR boards`"

Now "`Adafruit Feather 32u4`" should be selectable under `Tools/Board:`

### Upload software to arduino

Once it's configured use Arduino Studio to upload your software. When uploading the software make sure RX is discsonnect, othervise the upload will likely fail.

### Signal Server

Signal sever will configure the node automatically.

https://github.com/volvo-cars/signalbroker-server

Make sure to ports are open on the linux machine hosting the signalserver.

For ubuntu 16.04 you would need to open some port...

```bash
sudo ufw allow 2013
sudo ufw allow 2014
sudo ufw allow 4000
```

And configure your interfaces.json accordingly

```json
{
      "namespace": "LinSlave",
      "type": "lin",
      "config": {
        "device_identifier": 1,
        "server_port": 2014,
        "target_host": null,
        "target_port": 2013
      },
      "device_name": "lin",
      "node_mode": "slave",
      "ldf_file": "configuration/ldf_files/linone.ldf",
      "schedule_file": "configuration/ldf_files/linone.ldf",
      "schedule_table_name": "linoneSchedule",
      "schedule_autostart": false
    },s
```

In the example above the arduino is set up as a slave. by setting:

```json
{
    "node_mode": "master",
}
```
arduino will act as a master. In this case it also make sense to activate the automatic schedule

```json
{
    "node_mode": "master",
    "schedule_autostart": true,
}
```

## Starting
Once the arduino is powered on it will start by fetching configuration from the signal server.
Once the onboard led goes on, the board has fetched its configuration successfully from the signal server. (It will keep trying feting configuration until it succeeds)

To reload the configuration press the reset button on the top left (see picture above)

## Debugging
The arduino will by default output it's logs on port 3000, again, make sure to unblock that port in you firewall (check top of ino file to switch port).

Logging can the be traced on ubunto 16.04 by issuing

```bash
sudo tcpdump udp port 3000 -vv -X
```

## Nice to know
In slave mode the arduino keeps a write buffer which it writes every time the master scheduler queries it. This buffer is never cleared (the same reasoning goes for the signal server), so in practice, if you tampered with the wrong signal you should restart the signal server which will reset the arduino.

## References

* [MCP2004A documentation](http://ww1.microchip.com/downloads/en/DeviceDoc/20002230G.pdf)
