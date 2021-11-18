# GPS Logger with ESP8266 and u-blox NEO-M8N onto Micro-SD card

This [GPS logger device](https://en.wikipedia.org/wiki/GPS_tracking_unit#Data_loggers)
can be used to build a small and compact device to log GPS location data and
timestamps to an SD card in regular intervals. Possible use cases include:

* tracking routes of vehicles
  * car, motorcycles, trucks, boats, drones, model aircrafts, etc.
* personal exercise
  * hiking trails, runs, city walks, etc.
* etc.

This project uses a [Wemos D1 mini](https://www.wemos.cc/en/latest/d1/d1_mini.html)
module with an ESP8266 and an [u-blox NEO-M8N](https://www.u-blox.com/en/product/neo-m8-series)
GPS module to log GPS coordinates to a [Micro-SD card D1 mini shield](https://www.wemos.cc/en/latest/d1_mini_shield/micro_sd.html).

There are multiple versions of the GPS module: with integrated / on-board
antenna, with external antenna connector, with u-blox M6, M7, M8, M9 chip
generations. They all should work and behave almost identical. I chose a fully
integrated antenna-on-board design with a small footprint and the M8N chip.
These GPS modules are also often referred to as "6M/7M/8M".

While we refer to it as GPS, we usually mean satellite-based global positioning
systems in general, such as the (original) GPS constellation by the USA. Other
constellations that most GPS receivers also are capable of receiving and using
for improving their location accuracy are the GLONASS system by Russia, Galileo
by the European Union, and possible others.

The GPS data is only stored on the Micro-SD card and optionally uploaded over
WiFi if a network is in range. It does not provide real-time tracking or
monitoring features. Please respect personal privacy and your local laws and
regulation when working with such devices and information.

## Hardware

All three boards can be nicely stacked on top of each other. Use some padding
foam or 3D-printed spacers to separate the GPS module from the Wemos stack.
Everything fits into a 30x30x50mm box. Align all Micro-USB sockets to the same
side. The GPS antenna must face outwards to have a clear unobstructed view of
the sky. You can use Kapton-like tape to hold everything together. The antenna
can "look through" plastic tapes and parts.

### Wiring

The Wemos D1 mini is connected to the Micro-SD card shield via SPI. All pins
should match directly the footprint of both boards are stacked on top of each
other: 

* SS directly to SS on the shield
* MOSI directly to MOSI on the shield
* MISO directly to MISO on the shield
* CLK directly to CLK on the shield
* 3.3V directly to 3.3V on the shield
* GND directly to GND on the shield

The GPS module uses a UART/serial connection:

* GPS RXD pin to the D1 pin on the Wemos module
* GPS TXD pin to the D2 pin on the Wemos module
* GPS VCC pin to 3.3V on the Wemos module
* GPS GND pin to GND on the Wemos module
* GPS PPS pin leave unconnected if present

### GPS Module Modification

My M8N GPS module came with a tiny rechargeable battery to keep the real time
clock and memory data intact - but it turned out this battery was discharging
withing minutes. I replaced this tiny battery with a CR2032 coin cell (typical
PC motherboard BIOS battery with pre-attached leads). I cut of the small
connector and soldered the positive and negative leads to the same pads where
the on-board battery was. There was a USB-charging circuit which I disabled by
desoldering and removing a SMD diode next to the original battery location. The
new CR2032 battery can be nicely folded over and put onto the stack of boards
with appropriate insulation.

The module also came with a bulky SMA antenna connector. I desoldered it because
the on-board antenna is sufficient for my use case. The connector soaks up the
heat easily, so desoldering is tricky and has to be done with care to avoid
cooking the other components on the module.

![GPS module front side](/gps-module-front.jpeg)

![GPS module back side](/gps-module-back.jpeg)

All red circles are components to be desoldered. The top left is the old tiny
rechargeable battery that was replaced with a CR2032 soldered directly to the
same PCB pads.

![New CR2032 battery](/cr2032.jpeg)

## Software on the ESP8266

The ESP8266 can be flashed via the included PlatformIO project. All dependencies
and libraries are defined in `platformio.ini`:

* SPI - included in the Arduino framework for the ESP8266
* SDFS - included in the Arduino framework for the ESP8266
* [mikalhart/TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)
* (optional) [arduino-libraries/NTPClient](https://github.com/arduino-libraries/NTPClient)

See [PlatformIO](https://platformio.org/) for more information on how to get started.

Quick development commands:

* compile and verify the code: `platformio run`
* compile and upload the code: `platformio run --target upload`
* get a serial console of the device: `platformio device monitor`

### Configuration

There are two files with available configuration values:

* `include/wifi_credentials.h`
* `include/customizations.h`

Both files have a `*_template.h` sample file in the same folder. Simply copy it
to the correct filename and make your changes. If you don't want your changes to
be committed or end up being published, you can symlink them from a different
folder on your computer.

### GPS Log Data Storage

Insert a FAT32-formatted Micro-SD card into the attached shield. If no SD card
is found, or it fails to initialize, the system will end up in a reboot loop.
Some very large SD cards might cause issues, please try again with a smaller
card -- 4GB or 8GB usually work fine.

All GPS data will be logged and appended to a `GPS.TXT` file in the root of the
SD card. If there is data (GPS location points) stored, it will be uploaded via
WiFi during start-up. Once the system is booted and storing new location points
to this file, no new uploads will happen. You can power-cycle it to trigger an
upload of previously stored GPS location data. See the [Software on an Upload
Server](#software-upload-server) section for the server component to receive
these uploaded files. After a successful upload, the file is deleted from the SD
card, and a new empty file will be created.

### Lost & Found Owner Information

During start-up, the system will write an `OWNER.TXT` to the SD card. You can
define the content as string in `OWNER_CONTENT`. You can put your name, phone
number, or email in there, so that a potential finder of your device can contact
you.

### Connecting to WiFi networks

You can configure multiple WiFi networks and their credentials. On start-up, the
system will scan for available networks and try to connect to them in the order
you have defined them in `include/wifi_credentials.h` with SSID and passphrase.

Once the start-up sequence is completed, it will not try to scan or connect to
any networks - if you want to trigger a certain action, simply power-cycle the
system.

### Accelerated GPS fix acquistion with u-blox AssistNow

See the product page for [u-blox AssistNow](https://www.u-blox.com/en/product/assistnow).

This online service by the manufacturer of the GPS receiver (the actual chip,
not the PCB module maker) allows you to download current up-to-date satellite
information via the internet to speed-up the signal acquisition. During a cold
start-up the GPS receiver does not know where the satellites are and which one
to listen for at a give time. It also doesn't know the current time, if there is
not RTC included or the battery is dead. Such a cold start can result in 10+
minutes before the first location fix is acquired.

The AssistNow service provides an API via HTTP endpoints to query for this
satellite data, download a current dataset, and feed it into a u-blox GPS
receiver. The receiver will then almost immediately know were the satellites
should be and can give a first GPS location fix within a few seconds after
start-up. The service provides two endpoints: one for *online* queries, where
the device needs to know its rough location in the world and a data validity
time of 2-4 hours; and the *offline* service which can give multiple weeks of
pre-calculated satellite information. 

During start-up of the ESP8266, and if a WiFi connection is available, it will
query the AssistNow online and offline HTTP APIs to download the necessary data.
These endpoints require and authentication token, which must be configured in
the `include/customizations.h` file, as well as a rough current GPS location. If
you always leave from your home on a journey, you can use this location.
Otherwise you could save the last location during a proper shutdown routine. 

The autentication token can be requested via this
[evaluation token request form](https://www.u-blox.com/en/assistnow-service-evaluation-token-request-form),
or via a 
[Thingstream IoT-Location-as-a-Service](https://thingstream.io/)
plan, which includes a generous [free quota](https://portal.thingstream.io/pricing)
for AssistNow Online and AssistNow Offline requests per month.

The downloaded AssistNow data is stored on the SD card to be reused on
subsequent start-ups without WiFi. Make sure to have a valid RTC time with a
working RTC battery to keep the time information during power cycles, otherwise
the AssistNow data becomes useless without time information. The system will
only keep the most recent data files on the SD card and delete outdated ones.
They all are named as `ASSISTNOW-{ONLINE,OFFLINE}-{timestamp}.bin`. These files
contain UBX message, a proprietary messaging format that u-blox GPS receivers
understand. For our purposes we are simply uploading them as binary blobs from
the ESP8266 SD card to the GPS receiver via its built-in serial connection (same
connection as we are reading in the GPS location data).

### Debugging and Event Logging

During normal operation the system will log events and key data as text data to
`LOG.TXT`. It contains the same data as is printed to the Serial output when
connected to a computer. While testing outdoors (to get good GPS signal
reception) this can be helpful to analyse what the system is doing. Make sure
this file does not fill up the SD card, so regularly check it and delete
unneeded content (or the whole file). You can disable this log file writting
completely by setting `DEBUG_LOG_ENABLED` to `0`.

## Software on the Upload Server
<a name="software-upload-server"></a>

The upload server will accept the `GPS.TXT` from the ESP8266 and store it to the
server's disk. The upload server is a small Python-based application and can be
run as a Docker container or systemd service unit. It opens a new HTTP/HTTPS web
server and waits for devices to connect and upload files. Each incoming file is
timestamped, stored to disk, and converted to GPX format while retaining the
original CSV from the ESP8266's Micro-SD card.

Due to the limited resources on the ESP8266, a full HTTPS certificate chain
validation is not possible. Instead, only the certificate fingerprint the server
presents to the client (the ESP8266) is verified.

You can create a self-signed long-lived certificate with `openssl`:

```bash
# 10 years of validity
# 4096 bit RSA key
$ openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 3650 -nodes
# you can leave most input blank or default
# Common Name not verified by the client, but needed during creation
Common Name: gps-logger.example.com
```

And get the certificate fingerprint to put it into the ESP8266 software
`include/customizations.h` file:

```bash
$ openssl x509 -fingerprint -in cert.pem -noout                
SHA1 Fingerprint=12:34:56:78:90:AB:CD:12:34:56:78:90:AB:CD:12:34:56:78:90:AB
```

Store the resulting `key.pem` and `cert.pem` files in a safe way. Once you have
programmed your ESP8266 devices, this certificate is the only one being accepted for
uploading of GPS files. Changing the certificate requires a re-flashing of all
your GPS loggers!

### Configuration

The upload server will read a INI-style config file from either
`$CWD/config/config.ini` or `$CWD/config.ini` during start-up.

```ini
[gps-logger-upload-server]
listen_host=0.0.0.0
listen_port=8274
cert_file=/some/path/to/cert.pem
key_file=/some/path/to/key.pem
http_path=/upload
magic_header=X-MAGIC-AUTHORIZER
magic_header_value=some-security-through-obscurity-spam-block
output_path=/some/path/to/store/output/
```

You can omit individual lines and their default will be used:

* `listen_host` defaults to empty or `0.0.0.0`, it will listen to all
  interfaces on IPv4.
* `listen_port` defaults to 8472.
* `cert_file` and `key_file` do not have default values. If you omit them, a
  non-secure HTTP server will be started. Can be relative to the current working
  directory.
* `http_path` defaults to `/upload`.
* `magic_header` and `magic_header_value` do not have default values. If you
  omit them, no pseudo-authentication / spam block will be performed.
* `output_path` defaults to `output/`. Can be relative to the current working
  directory.

### Hosting as Docker container

You can use these instructions to build and run the upload server as Docker container:

```bash
IMAGE_NAME=gps-upload-server

docker build -t ${IMAGE_NAME} .
docker run \
  -d \
  -p 8472:8472 \
  -v /home/my_user/gps-logger/config:/app/config:ro \
  -v /home/my_user/gps-logger/output:/app/output \
  --restart unless-stopped \
  --name ${IMAGE_NAME} \
  ${IMAGE_NAME}
```

Make sure to place the config file and the certificate files (cert + private
key, if needed) into the correct folders. You can use the `config/` folder to
hold the `config.ini` and certificate PEM files. You can mount the config folder
as read-only.

## Related Reading

https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/
https://www.u-blox.com/sites/default/files/products/documents/AssistNow_ProductSummary_UBX-13003352.pdf
https://www.u-blox.com/sites/default/files/products/documents/MultiGNSS-Assistance_UserGuide_%28UBX-13004360%29.pdf

## License

This project is made available under the MIT License. For more details, see the
``LICENSE`` file in the repository.

## Author

This project was created by Thomas Kriechbaumer.
