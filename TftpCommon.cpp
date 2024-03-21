//
// Created by B Pan on 1/15/24.
//

#include <csignal>
#include <chrono>
#include <thread>
#include "TftpCommon.h"

// Helper function to print the first len bytes of the buffer in Hex
static void printBuffer(const char *buffer, unsigned int len)
{
    for (int i = 0; i < len; i++)
    {
        printf("%x,", buffer[i]);
    }
    printf("\n");
}

// Increment retry count when timeout occurs
void handleTimeout(int signum)
{
    retryCount++;
    printf("timeout occurred! count %d\n", retryCount);
}

int registerTimeoutHandler()
{
    signal(SIGALRM, handleTimeout);

    /* disable the restart of system call on signal. otherwise the OS will be stuck in
     * the system call
     */
    if (siginterrupt(SIGALRM, 1) == -1)
    {
        printf("invalid sig number.\n");
        return -1;
    }
    return 0;
}

/*
 * Useful things:
 * alarm(1) // set timer for 1 sec
 * alarm(0) // clear timer
 * std::this_thread::sleep_for(std::chrono::milliseconds(200)); // slow down transmission
 */

/*
 * Common code that is shared between your server and your client here. For example: helper functions for
 * sending bytes, receiving bytes, parse opcode from a tftp packet, parse data block/ack number from a tftp packet,
 * create a data block/ack packet, and the common "process the file transfer" logic.
 */
void handleRequestPacket(char *filename, int opcode, int sockfd, struct sockaddr_in serv_addr)
{
    // Create a TFTP request packet
    TftpRequestPacket requestPkt;
    requestPkt.opcode = htons(opcode); // set opcode to RRQ or WRQ
    strcpy(requestPkt.filename, filename);

    // Send the packet to the server
    ssize_t sentBytes = sendto(sockfd, &requestPkt, sizeof(requestPkt), 0,
                               (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (sentBytes == -1)
    {
        perror("Error sending TFTP request packet");
        return;
    }

    std::cout << "TFTP request packet sent successfully." << std::endl;
}

void sendDataPacket(int sockfd, TftpDataPacket &dataPacket, uint16_t blockNumber, struct sockaddr_in &dest_addr, socklen_t _len)
{
    // Set the block number in the data packet
    dataPacket.blockNumber = htons(blockNumber);

    // Send the Data Packet to the client
    ssize_t bytesSent = sendto(sockfd, &dataPacket, sizeof(TftpDataPacket), 0, (struct sockaddr *)&dest_addr, _len);
    if (bytesSent < 0)
    {
        perror("sendto error");
        exit(4);
    }
}

ssize_t receiveDataPacket(int sockfd, TftpDataPacket &dataPacket, struct sockaddr_in &source_addr, socklen_t &_len)
{
    // Wait for data packets from the sender
    ssize_t bytesReceived = recvfrom(sockfd, &dataPacket, sizeof(TftpDataPacket), 0, (struct sockaddr *)&source_addr, &_len);
    if (bytesReceived < 0)
    {
        perror("recvfrom error");
        exit(5);
    }

    return bytesReceived;
}

void sendAckPacket(int sockfd, uint16_t blockNumber, struct sockaddr_in &dest_addr, socklen_t _len)
{
    // Create an ACK packet
    TftpAckPacket ackPacket;
    ackPacket.blockNumber = htons(blockNumber);

    // Send the ACK packet to the receiver
    ssize_t bytesSent = sendto(sockfd, &ackPacket, sizeof(TftpAckPacket), 0, (struct sockaddr *)&dest_addr, _len);
    if (bytesSent < 0)
    {
        perror("sendto error");
        exit(4);
    }
}

ssize_t receiveAckPacket(int sockfd, TftpAckPacket &ackPacket, struct sockaddr_in &source_addr, socklen_t &_len)
{
    // Wait for acknowledgment (ACK) from the sender
    ssize_t bytesReceived = recvfrom(sockfd, &ackPacket, sizeof(TftpAckPacket), 0, (struct sockaddr *)&source_addr, &_len);
    if (bytesReceived < 0)
    {
        perror("recvfrom error");
        exit(5);
    }
    return bytesReceived;
}

void handleErrorPacket(int errorCode, std::string errorMsg, int sockfd, struct sockaddr_in _addr, socklen_t _len)
{
    // Construct an error packet
    TftpErrorPacket errorPacket;
    errorPacket.errorCode = htons(errorCode);
    errorPacket.errorMessage = errorMsg;

    // Send the error packet to the client
    ssize_t bytesSent = sendto(sockfd, &errorPacket, sizeof(TftpErrorPacket), 0, (struct sockaddr *)&_addr, _len);
    if (bytesSent < 0)
    {
        perror("sendto error");
        exit(4);
    }
}