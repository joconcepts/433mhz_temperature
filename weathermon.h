#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

#include <wiringPi.h>
#include <curl/curl.h>

int init();
void init_globals();

void read_signal();
void add_bit(char bit);
void record_sensor_data();
void post_curl(int id, char *data);
