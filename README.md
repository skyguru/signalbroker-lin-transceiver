# Lin bus reader and writer


ESP32-POE + Custom made PCB with MCP2004A. Read/write, act as a master or one or many slaves.

## Setup

### ESP32-POE

The schematics for this solution can be found in the KiCad folder under the appropriate card/project.
 
#### PCB

Assemble the PCB. The BOM(Bill of materials) is located under doc/PCB/KiCad. 
Use the jumper for master mode if the module shoulde be a master node.
![PCB](https://github.com/volvo-cars/signalbroker-lin-transceiver/raw/esp32/doc/PCB.jpg)

Connect the ESP32-POE to the PCB.
![Final module](/doc/Final-module.JPG)

The PCB is designed to be able to have several modules in a case/box if needed.
![Multiple modules](/doc/multiple-cards.JPG)

## Configuration

### ESP32-POE

#### Install software

* Install VSCode https://code.visualstudio.com/
* Install extension PlatformIO IDE https://platformio.org/platformio-ide
* Open the folder named linbus in PlatformIO

### Upload software to ESP32

Upload the application to your ESP32. Make sure you don't have the ESP powered by POE (if you don't have the isolated one), if you have it powerd by POE and the USB at same time, it is a big risc that you burn the ESP32 or the connected USB-port on your computer.

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
}
```

To run the ESP32 as a Slave, set the properties like this ones below.

```json
{
    "node_mode": "master",
    "schedule_autostart": false
}
```

And to run it as a Master, set the properties like this ones below.

```json
{
    "node_mode": "master",
    "schedule_autostart": true
}
```

## Starting
Once the ESP32 is powered on it will start by fetching configuration from the signal server. (It will keep trying feting configuration until it succeeds)

## Debugging
The arduino will by default output it's logs on port 3000, again, make sure to unblock that port in you firewall (check top of ino file to switch port).

Logging can the be traced on ubunto 16.04 by issuing

```bash
sudo tcpdump udp port 3000 -vv -X
```

## Nice to know
In slave mode the esp32 keeps a write buffer which it writes every time the master scheduler queries it. This buffer is never cleared (the same reasoning goes for the signal server), so in practice, if you tampered with the wrong signal you should restart the signal server which will reset the esp32.

## References

* [MCP2004A documentation](http://ww1.microchip.com/downloads/en/DeviceDoc/20002230G.pdf)
