#ifndef DHT22_H
#define DHT22_H

#include <curl/curl.h>

struct stream_ctx {
	int		 pin;
	size_t		 avg_samples;
	unsigned int	 interval;

	size_t		 hist_index;
	float		 temp;
	float		*temp_hist;
	float		 hum;
	float		*hum_hist;

	CURL		*curl;
	char		*influx_url;
	char		*influx_username;
	char		*influx_password;
	char		*influx_measurement;
};

void parse_config(struct stream_ctx *, FILE *);

#endif
