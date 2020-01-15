/* -*- tab-width: 8; indent-tabs-mode: t; -*- */
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <wiringPi.h>
#include <curl/curl.h>

#define AVG_SAMPLE 3
#define MAXTIMINGS 85

static uint8_t sizecvt(int read) {
	/* digitalRead() and friends from wiringpi are defined as
	   returning a value < 256. However, they are returned as
	   int() types. This is a safety function */

	if (read > 255 || read < 0) {
		fprintf(stderr, "Invalid data from wiringPi library\n");
		exit(EXIT_FAILURE);
	}

	return (uint8_t) read;
}

static int read_dht22_dat(int pin, float *temperature, float *humidity) {
	int dht22_dat[5] = { 0 };

	pinMode(pin, OUTPUT);	/* pull pin down for 18 ms */
	digitalWrite(pin, HIGH);
	delay(500);
	digitalWrite(pin, LOW);
	delay(20);
	pinMode(pin, INPUT);	/* prepare to read the pin */

	int laststate = HIGH;
	int counter;
	int j = 0;
	// detect change and read data
	for (int i = 0; i < MAXTIMINGS; i++) {
		counter = 0;
		while (sizecvt(digitalRead(pin)) == laststate) {
			counter++;
			delayMicroseconds(2);
			if (counter == 255)
				break;
		}
		laststate = sizecvt(digitalRead(pin));

		if (counter == 255)
			break;

		// ignore first 3 transitions
		if ((i >= 4) && (i%2 == 0)) {
			// shove each bit into the storage bytes
			dht22_dat[j/8] <<= 1;
			if (counter > 16)
				dht22_dat[j/8] |= 1;
			j++;
		}
	}

	// check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
	// this check fails very frequently in my tests and i'm unsure whether that's normal
	if ((j < 40) ||
	    (dht22_dat[4] != ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xff))) {
		return -1;
	}

	float h = (float) dht22_dat[0] * 256 + (float) dht22_dat[1];
	h /= 10.0;

	float t = (float) (dht22_dat[2] & 0x7F) * 256 + (float) dht22_dat[3];
	t /= 10.0;

	if ((dht22_dat[2] & 0x80) != 0)
		t *= -1;

	*temperature = t;
	*humidity = h;

	return 0;
}

static int average_values(float *temp, float *hum) {
	static float temp_hist[AVG_SAMPLE], hum_hist[AVG_SAMPLE];
	static size_t hist_index = 0;

	if (-75.0f > *temp || *temp > 75.0f) {
		/* probably a measuring error, toss the data and
		   restart averaging */
		fprintf(stderr, "garbage read: temp = %f\n", *temp);
		hist_index = 0;
		return -1;
	}

	temp_hist[hist_index] = *temp;
	hum_hist[hist_index] = *hum;

	hist_index += 1;
	if (hist_index != AVG_SAMPLE) {
		return -1;
	}

	*hum  = ( hum_hist[0] +  hum_hist[1] +  hum_hist[2]) / 3.0f;
	*temp = (temp_hist[0] + temp_hist[1] + temp_hist[2]) / 3.0f;

	hist_index = 0;
	return 0;
}

int main (int argc, char *argv[]) {
	char	 postdata[512] = { 0 };
	CURL	*curl;
	int	 pin;
	float	 temperature;
	float	 humidity;
	time_t	 now;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <pin>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (wiringPiSetup() == -1) {
		perror("wiringPiSetup failed");
		exit(EXIT_FAILURE);
	}

	curl		= curl_easy_init();
	pin		= atoi(argv[1]);

	puts("Entering read loop...");

	while (1) {
		delay(1000);

		temperature = 0.0f;
		humidity    = 0.0f;

		if (read_dht22_dat(pin, &temperature, &humidity) == -1 ||
		    average_values(&temperature, &humidity) == -1) {
			continue;
		}

		now = time(NULL);
		sprintf(postdata,
			"am2302,type=temperature value=%.2f %ld000000000\n"
			"am2302,type=humidity value=%.2f %ld000000000",
			temperature, now, humidity, now);

		curl_easy_setopt(curl, CURLOPT_URL, "http://turing.flisk.xyz:8086/write?db=sensors");
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);

		if (curl_easy_perform(curl) != CURLE_OK) {
			fputs("fuck", stderr);
		}
	}

	curl_easy_cleanup(curl);
	exit(EXIT_SUCCESS);
}
