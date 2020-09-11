# dht22-to-influxdb

A fairly crude program for Linux that lets you read DHT22/AM2302
sensor data (C° and rel. humidity) into InfluxDB.

## Installing

dht22-to-influxdb depends on the following packages:

- GNU Bison *(build)*
- flex *(build)*
- libwiringpi *(build, runtime)*
- libcurl *(build, runtime)*

```sh
# build and install:
$ make
$ sudo make DESTDIR=/usr/local install

# if you change your mind:
$ sudo make DESTDIR=/usr/local uninstall
```

## Usage

Example invocation:

```sh
# dht22-to-influxdb <config file>
```

## Configuration format

This program currently does not support more than a single mapping
between one sensor and one InfluxDB endpoint. Since this may change in
the future, the config file is still expected to contain a single
"superfluous" `stream` directive as shown below.

No other directives are implemented at this time.

```
stream \
       from wiringpi 4 \
       avg samples 10 \
       interval 1 \
       url "http://localhost:8086/write?db=sensors" \
       username "dht22-to-influxdb" \
       password "password" \
       measurement "dht22"
```

### `stream` options

- `from wiringpi <n>`<br>
  *Required: yes*<br>
  WiringPi GPIO pin number to poll sensor data on.

- `avg samples <n>`<br>
  *Required: yes*<br>
  How many sensor values to average into InfluxDB datapoints.<br>
  `1` to disable averaging and write raw sensor values.

- `interval <n>`<br>
  *Unit: seconds*<br>
  *Required: yes*<br>
  How frequently to write datapoints to InfluxDB.

- `url <s>`<br>
  *Required: yes*<br>
  InfluxDB endpoint URL to write sensor data to.

- `username <s>` / `password <s>`<br>
  *Required: no*<br>
  Login details for the InfluxDB endpoint.

- `measurement <s>`<br>
  *Required: yes*<br>
  InfluxDB measurement to write datapoints to.
