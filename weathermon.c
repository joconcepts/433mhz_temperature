#include "weathermon.h"

static const int MAX_NUM_SENSORS   =    8;
static const int DEFAULT_VALUE     = 9999;
static const int RX_PIN            =    2;
static const int SHORT_DELAY       =  242;
static const int LONG_DELAY        =  484;
static const int NUM_HEADER_BITS   =   10;
static const int MAX_BYTES         =    7;

char      temp_bit;
bool      first_zero;
char      header_hits;
char      data_byte;
char      num_bits;
char      num_bytes;
char      manchester[7];
sigset_t  myset;

char *hostname;
int portno;
int sockfd;
int serverlen;
struct sockaddr_in serveraddr;
struct hostent *server;

int main(int argc, char **argv)
{
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    serverlen = sizeof(serveraddr);

    init();
    wiringPiISR(RX_PIN, INT_EDGE_BOTH, &read_signal);
    sigsuspend(&myset);
}

int init() 
{
    init_globals();
    for (int i = 0; i < 4; i++) {
        manchester[i] = i;
    }
    wiringPiSetup();
    pinMode(RX_PIN, INPUT);
}

void init_globals() 
{
    temp_bit = 1;
    first_zero = false;
    header_hits = 0;
    num_bits = 6;
    num_bytes = 0;
}

void read_signal()
{
    if(digitalRead(RX_PIN) != temp_bit) {
        return;
    }
    delayMicroseconds(SHORT_DELAY);
    if (digitalRead(RX_PIN) != temp_bit) {
        // Halfway through the bit pattern the RX_PIN should
        // be equal to tempBit, if not then error, restart!
        init_globals();
        return;
    }
    delayMicroseconds(LONG_DELAY);
    if(digitalRead(RX_PIN) == temp_bit) {
        temp_bit = temp_bit ^ 1;
    }
    char bit = temp_bit ^ 1;
    if (bit == 1) {
        if (!first_zero) {
            header_hits++;
        } else {
            add_bit(bit);
        }
    } else {
        if(header_hits < NUM_HEADER_BITS) {
            // Something went wrong, we should not be in the
            // header here, so restart!
            init_globals();
            return;
        }
        if (!first_zero) {
            first_zero = true;
        }
        add_bit(bit);
    }
    if (num_bytes == MAX_BYTES) {
        record_sensor_data(); 
        init_globals();
    }
}

void add_bit(char bit) 
{
    data_byte = (data_byte << 1) | bit;
    if (++num_bits == 8) {
        num_bits=0;
        manchester[num_bytes++] = data_byte;
    }
}

// checksum code from BaronVonSchnowzer at
// http://forum.arduino.cc/index.php?topic=214436.15
char checksum(int length, char *buff)
{
    char mask = 0x7C;
    char checksum = 0x64;
    char data;
    int byteCnt;

    for (byteCnt=0; byteCnt < length; byteCnt++) {
        int bitCnt;
        data = buff[byteCnt];
        for (bitCnt= 7; bitCnt >= 0 ; bitCnt--) {
            char bit;
            // Rotate mask right
            bit = mask & 1;
            mask = (mask >> 1) | (mask << 7);
            if (bit) {
              mask ^= 0x18;
            }

            // XOR mask into checksum if data bit is 1
            if(data & 0x80) {
              checksum ^= mask;
            }
            data <<= 1;
        }
    }
    return checksum;
}

void record_sensor_data()
{
    int ch = ((manchester[3] & 0x70) / 16) + 1;
    int data_type = manchester[1];
    int new_temp = ((manchester[3] & 0x7) * 256 + manchester[4]) - 400;
    int new_hum = manchester[5]; 
    int low_bat = manchester[3] & 0x80 / 128;
    char check_byte = manchester[MAX_BYTES-1];
    char check = checksum(MAX_BYTES-2, manchester+1);
    if (check != check_byte || data_type != 0x45 || ch < 1 ||
           ch > MAX_NUM_SENSORS || new_hum > 100 ||
           new_temp < -200 || new_temp > 1200) {
        return;
    }
    float c_temp = (new_temp/10.0 - 32) * (5.0/9.0);
    send_statsd(ch, c_temp, new_hum);
}

void error(char *msg) {
    perror(msg);
    exit(0);
}

void send_statsd(int id, float temperature, int humidity) {
    int n;
    char buf[1024];
    char *data_temp, *data_hum;

    data_temp = (char *)malloc(128);
    snprintf(data_temp, 128, "thermometer.%d.temperature:%.2f|g", id, temperature);

    data_hum = (char *)malloc(128);
    snprintf(data_hum, 128, "thermometer.%d.humidity:%d|g", id, humidity);

    printf("Received: %s | %s\n", data_temp, data_hum);
    n = sendto(sockfd, data_temp, strlen(data_temp), 0, (struct sockaddr *)&serveraddr, serverlen);
    if (n < 0)
      error("ERROR in sendto");
    n = sendto(sockfd, data_hum, strlen(data_hum), 0, (struct sockaddr *)&serveraddr, serverlen);
    if (n < 0)
      error("ERROR in sendto");
}
