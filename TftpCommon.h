// TftpCommon.h
#ifndef TFTP_COMMON_H
#define TFTP_COMMON_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <thread>
#include <chrono>
#include "fstream"
#include "TftpError.h"
#include "TftpOpcode.h"
#include "TftpConstant.h"

// To track how retransmit/retry has occurred.
static int retryCount = 0;
static bool timeoutOccurred = false;

// Helper function to print the first len bytes of the buffer in Hex
static void printBuffer(const char *buffer, unsigned int len);

// Increment retry count when timeout occurs.
void handleTimeout(int signum);
int registerTimeoutHandler();

// Structure representing the TFTP request packet
struct TftpRequestPacket
{
    uint16_t opcode; // opcode for RRQ or WRQ
    char filename[512];
    char mode[10];

    TftpRequestPacket() : mode("octet") {}
};

void handleRequestPacket(char *filename, int opcode, int sockfd, struct sockaddr_in serv_addr);

// Structure representing the TFTP data packet
struct TftpDataPacket
{
    uint16_t opcode;
    uint16_t blockNumber;
    char data[512] = {}; // data payload with a maximum data length of 512, initialized to null characters

    TftpDataPacket() : opcode(htons(TFTP_DATA)) {}
};

void sendDataPacket(int sockfd, TftpDataPacket &dataPacket, uint16_t blockNumber, struct sockaddr_in &dest_addr, socklen_t _len);
ssize_t receiveDataPacket(int sockfd, TftpDataPacket &dataPacket, struct sockaddr_in &source_addr, socklen_t &_len);

// Structure representing the TFTP acknowledgment packet
struct TftpAckPacket
{
    uint16_t opcode;
    uint16_t blockNumber;

    TftpAckPacket() : opcode(htons(TFTP_ACK)) {}
};

void sendAckPacket(int sockfd, uint16_t blockNumber, struct sockaddr_in &dest_addr, socklen_t _len);
ssize_t receiveAckPacket(int sockfd, TftpAckPacket &ackPacket, struct sockaddr_in &source_addr, socklen_t &_len);

// Structure representing the TFTP error packet
struct TftpErrorPacket
{
    uint16_t opcode;
    uint16_t errorCode;
    std::string errorMessage;

    TftpErrorPacket() : opcode(htons(TFTP_ERROR)), errorMessage("") {}
};

void handleErrorPacket(int errorCode, std::string errorMsg, int sockfd, struct sockaddr_in _addr, socklen_t _len);

// Structure representing the general TFTP packet
struct TftpPacket
{
    uint16_t opcode;

    TftpPacket() : opcode(0) {}
};
union TftpPacketUnion
{
    TftpPacket packet;
    TftpErrorPacket errorPacket;
    TftpDataPacket dataPacket;
    TftpAckPacket ackPacket;
    TftpRequestPacket requestPacket;

    TftpPacketUnion() { new (&packet) TftpPacket(); }
    ~TftpPacketUnion() { packet.~TftpPacket(); }
};
#endif