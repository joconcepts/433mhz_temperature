#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

#include <wiringPi.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int init();
void init_globals();

void read_signal();
void add_bit(char bit);
void record_sensor_data();
void send_statsd(int id, float temperature, int humidity);
void error(char *msg);
