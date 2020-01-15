dht22
=====

A crude solution for reading DHT22/AM2302 sensor data (C° and
rel. humidity) into InfluxDB. I use this on Arch Linux ARM, but
Raspbian *should* work too.

You will need `libwiringPi` on your system to compile `dht22`, which
is packaged as `wiringpi` in Arch Linux ARM and comes preinstalled on
Raspbian if you didn't install the lite image.

If you installed the lite image, you'll have to find the right
WiringPi package yourself. It's probably `wiringpi` or `libwiringpi`,
but Raspbian doesn't have an online package browser and I don't feel
like looking into it any further. Nyee-nyeeh, puny Raspbian user!
You're on your own.
