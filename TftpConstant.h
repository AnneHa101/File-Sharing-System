/*
 * Add constant variables in this file
 */
static const unsigned int MAX_PACKET_LEN = 516; // data 512 + opcode 2 + block num 2
static const unsigned int MAX_DATA_LEN = 512;
static const int TIME_OUT = 1;
static const int MAX_RETRY_COUNT = 10;
static const char *SERVER_FOLDER = "server-files/"; // DO NOT CHANGE
static const char *CLIENT_FOLDER = "client-files/"; // DO NOT CHANGE