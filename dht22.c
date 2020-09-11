/* -*- indent-tabs-mode: t; -*- */
#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>

#include <wiringPi.h>
#include <curl/curl.h>
#include "dht22.h"

#define MAXTIMINGS 85
#define POSTFIELDS_BUF_SIZE 512

#ifdef DEBUG
static void dump_hist(struct stream_ctx *ctx)
{
	puts("--");
	printf("humhist\t");

	for (size_t i = 0; i < ctx->hist_index; i++) {
		printf("\t%.2f", ctx->hum_hist[i]);
	}

	puts("");
	printf("temphist");

	for (size_t i = 0; i < ctx->hist_index; i++) {
		printf("\t%.2f", ctx->temp_hist[i]);
	}

	puts("");
}
#endif

/*
 * read a value with wiringPi's digitalRead, safely casting the result
 * to an unsigned byte
 */
static uint8_t digital_read_uint8(int pin)
{
	int i = digitalRead(pin);

	if (0 > i || i > 255) {
		error(
			EXIT_FAILURE, 0,
			"digitalRead(%d) = %d, out of range for uint8",
			pin, i
		);
	}

	return (uint8_t) i;
}

/*
 * poll sensor for data and update our state fields
 */
static int poll_sensor(struct stream_ctx *ctx)
{
	/* poke the sensor, letting it know we want to read
	 *
	 * reference documentation says to hold our signal for "at
	 * least 1ms", so let's hold it for 10 ms
	 */
	pinMode(ctx->pin, OUTPUT);
	digitalWrite(ctx->pin, HIGH);
	delay(10);
	digitalWrite(ctx->pin, LOW);

	/* sensor should respond after about 20-40 ms */
	delay(20);
	pinMode(ctx->pin, INPUT);

	int buf[5] = { 0 };
	int last_state = HIGH;
	int bits_read = 0;

	for (int i = 0; i < MAXTIMINGS; i++) {
		int counter = 0;

		while (digital_read_uint8(ctx->pin) == last_state) {
			counter++;
			delayMicroseconds(2);

			if (counter == 255) {
				goto endfor;
			}
		}

		// XXX: why is this called twice ?
		last_state = digital_read_uint8(ctx->pin);

		// ignore first 3 transitions
		if ((i >= 4) && (i % 2 == 0)) {
			// shove each bit into the storage bytes
			buf[bits_read / 8] <<= 1;

			if (counter > 16) {
				buf[bits_read / 8] |= 1;
			}

			bits_read++;
		}
	}

endfor:

	if (bits_read < 40) {
		return -1;
	}

	// this check fails very frequently in my tests and i'm unsure whether that's normal
	int chk = (buf[0] + buf[1] + buf[2] + buf[3]) & 0xff;
	if (chk != buf[4]) {
		return -1;
	}

	/* TODO: wtf is going on here?! */

	ctx->hum  = (float) buf[0];
	ctx->hum *= 256.0f;
	ctx->hum += (float) buf[1];
	ctx->hum /= 10.0f;

	ctx->temp  = (float) (buf[2] & 0x7f);
	ctx->temp *= 256.0f;
	ctx->temp += (float) buf[3];
	ctx->temp /= 10.0f;

	if ((buf[2] & 0x80) != 0) {
		ctx->temp *= -1;
	}

	return 0;
}

static int average_values(struct stream_ctx *ctx)
{
	if (-41.0f > ctx->temp || ctx->temp > 81.0f) {
		/* probably a measuring error, skip this datapoint */
		fprintf(stderr, "garbage read: temp = %f\n", ctx->temp);
		return -1;
	}

	ctx->temp_hist[ctx->hist_index] = ctx->temp;
	ctx->hum_hist[ctx->hist_index] = ctx->hum;

	ctx->hist_index += 1;

#ifdef DEBUG
	dump_hist(ctx);
#endif

	if (ctx->hist_index != ctx->avg_samples) {
		return -1;
	}

	ctx->hum = 0.0f;
	ctx->temp = 0.0f;

	for (size_t i = 0; i < ctx->avg_samples; i++) {
		ctx->hum += ctx->hum_hist[i];
		ctx->temp += ctx->temp_hist[i];
	}

	ctx->hum /= (float) ctx->avg_samples;
	ctx->temp /= (float) ctx->avg_samples;

	ctx->hist_index = 0;

	return 0;
}

static void write_datapoint(const struct stream_ctx *ctx)
{
	time_t now = time(NULL);
	char postfields[POSTFIELDS_BUF_SIZE] = { 0 };
	
	int n = snprintf(
		postfields,
		POSTFIELDS_BUF_SIZE,
		"%s,type=temperature value=%.2f %ld000000000\n"
		"%s,type=humidity value=%.2f %ld000000000",
		ctx->influx_measurement, ctx->temp, now,
		ctx->influx_measurement, ctx->hum, now
	);

	if (n < 0) {
		fprintf(stderr, "send_datapoint: snprintf returned %d\n", n);
		return;
	} else if (n >= POSTFIELDS_BUF_SIZE) {
		fprintf(stderr, "send_datapoint: request data truncated, write dropped\n");
		return;
	}

	if (ctx->influx_username) {
		curl_easy_setopt(ctx->curl, CURLOPT_USERNAME, ctx->influx_username);
		curl_easy_setopt(ctx->curl, CURLOPT_PASSWORD, ctx->influx_password);
	}

	curl_easy_setopt(ctx->curl, CURLOPT_URL, ctx->influx_url);
	curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
	curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, postfields);

	CURLcode c = curl_easy_perform(ctx->curl);

	if (c != CURLE_OK) {
		fprintf(
			stderr,
			"curl error while writing datapoint: %s\n",
			curl_easy_strerror(c)
		);
	}
}

static const char *getenv_or_fail(const char *name) {
	const char *s = getenv(name);
	const char *err = NULL;

	if (s == NULL) {
		err = "unset";
	} else if (*s == '\0') {
		err = "empty";
	}

	if (err) {
		error(EXIT_FAILURE, 0, "required env var %s is %s", name, err);
	}

	return s;
}

static void config_value_require(const char *name, int result)
{
	if (!result) {
		error(1, 0, "bad config, missing or invalid value: %s", name);
	}
}

static void read_config(const char *path, struct stream_ctx *ctx)
{
	FILE *f = fopen(path, "r");

	if (f == NULL) {
		error(EXIT_FAILURE, errno, "couldn't open %s for reading:", path);
	}

	parse_config(ctx, f);

	if (fclose(f) == EOF) {
		error(EXIT_FAILURE, errno, "couldn't close %s:", path);
	}

	config_value_require("gpio pin", ctx->pin > 0);
	config_value_require("avg samples", ctx->avg_samples > 0);
	config_value_require("interval", ctx->interval > 0);
	config_value_require("url", ctx->influx_url != NULL);
	config_value_require("measurement", ctx->influx_measurement != NULL);

	if (ctx->influx_username != NULL || ctx->influx_password != NULL) {
		config_value_require("username", ctx->influx_username != NULL);
		config_value_require("password", ctx->influx_password != NULL);
	}
}

int main (int argc, char *argv[]) {
	if (argc != 2) {
		error(EXIT_FAILURE, 0, "usage: %s CONFIG_FILE", argv[0]);
	}

	if (wiringPiSetup() == -1) {
		error(EXIT_FAILURE, 0, "wiringPiSetup failed");
	}

	struct stream_ctx ctx = { 0 };

	read_config(argv[1], &ctx);

	ctx.temp_hist = (float *) calloc(ctx.avg_samples, sizeof(float));
	ctx.hum_hist = (float *) calloc(ctx.avg_samples, sizeof(float));

	if (ctx.temp_hist == NULL || ctx.hum_hist == NULL) {
		error(EXIT_FAILURE, 0, "ran out of memory during init (wtf?)");
	}

	ctx.curl = curl_easy_init();

	if (ctx.curl == NULL) {
		error(EXIT_FAILURE, 0, "curl_easy_init failed");
	}
	
	int interval_msec = ctx.interval * 1000;

	while (1) {
		if (poll_sensor(&ctx) == 0 && average_values(&ctx) == 0) {
		    write_datapoint(&ctx);

#if DEBUG
		    printf(
			    "--\nt: %.2f\nh: %.2f\n\n",
			    ctx.temp,
			    ctx.hum
		    );
#endif
		}

		delay(interval_msec);
	}
}
