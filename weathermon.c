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

int main() 
{
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
    char *data;
    data = (char *)malloc(128);
    snprintf(data, 128, "{\"temperature\": %.2f, \"humidity\": %d}", c_temp, new_hum);
    post_curl(ch, data);
  free(data);
}

void post_curl(int id, char *post_data) {
  CURL *curl;
  CURLcode res;

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Accept: application/json");
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "charsets: utf-8");

  printf("CURL_DATA: %s\n", post_data);

  curl_global_init(CURL_GLOBAL_ALL);

  curl = curl_easy_init();
  if(curl) {
    char *url;
    url = (char *)malloc(128);
    snprintf(url, 128, "http://192.168.1.22/thermometers/%d.json", id);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers); 

    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
          curl_easy_strerror(res));

    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
}
