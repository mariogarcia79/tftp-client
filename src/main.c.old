#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define STRING_USAGE      "Usage: %s <IP address> {-r|-w} <file>\n"

#define SERVICE_NAME      "tftp"
#define SERVICE_PROTO     "udp"

#define TFTP_MESSAGE_MODE "octet"
#define BLOCK_SIZE 512

/* Struct for retrieving values from parse_args function*/
struct arguments {
    char *program_name;
    char *ip_address;
    char *filename;
    int  operation;
};

/* TFTP message opcode enumeration */
typedef enum {
    RRQ   = 1,
    WRQ   = 2,
    DATA  = 3,
    ACK   = 4,
    ERROR = 5
} OpCode;

typedef enum {
    FLAG_UNKNOWN,
    FLAG_READ,
    FLAG_WRITE
} FlagType;

/* Flag number resolution from argument string */
FlagType
get_flag(const char *arg)
{
    if (!strcmp(arg, "-r"))        return FLAG_READ;
    if (!strcmp(arg, "-w"))        return FLAG_WRITE;
    return FLAG_UNKNOWN;
}

int
parse_args(int argc, char *argv[], struct arguments *args)
{    
    if (argc != 4) {
        errno = EINVAL;
        return -1;
    }

    // Initialize already known fields
    args->program_name = argv[0];
    args->ip_address   = argv[1];
    args->filename     = argv[3];
    
    // Check for type of operation
    if (!strcmp(argv[2], "-r"))
        args->operation = FLAG_READ;
    else if (!strcmp(argv[2], "-w"))
        args->operation = FLAG_WRITE;
    else {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

/*
 * Get servent structure by name of service.
 * After that, execute socket syscall and bind port to it.
 * Returns the socket file descriptor
 */
int
setup_socket(
    struct arguments   *args,
    struct sockaddr_in *myaddr,
    struct sockaddr_in *addr
) {
    struct servent *serv;
    int sockfd;

    serv = getservbyname(SERVICE_NAME, SERVICE_PROTO);
    if (!serv) {
        errno = ENOENT;
        return -1;
    }

    addr->sin_family = AF_INET;
    addr->sin_port   = serv->s_port;
    if (inet_aton(args->ip_address, &addr->sin_addr) == 0) {
        fprintf(stderr, "Invalid IP address: %s \n", args->ip_address);
        return -1;
    }

    myaddr->sin_family = AF_INET;
    myaddr->sin_port   = serv->s_port;
    myaddr->sin_addr.s_addr = INADDR_ANY;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (!sockfd) {
        perror("socket");
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *) myaddr, (socklen_t) sizeof(*myaddr)) == -1) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

char *
serialize_req(
    uint16_t opcode,
    char     *filename,
    char     *mode
) {
    char *msg_start = NULL;
    char *msg = (char *) malloc(
        sizeof(opcode)   +
        strlen(filename) +
        strlen(mode)   + 2 // Account for EOS
    );

    msg_start = msg;

    if (!msg) return NULL;

    memcpy(msg, &opcode, sizeof(opcode));
    msg += sizeof(opcode);
    memcpy(msg, filename, strlen(filename) + 1);
    msg += strlen(filename) + 1;
    memcpy(msg, mode, strlen(mode) + 1);

    return msg_start; // Has to be deallocated.
}

/* Receives a file from a TFTP server using RRQ (Read Request) and ACKs each DATA packet received */
void receive_file(int sockfd, struct sockaddr_in *addr, const char *filename) {
    char *msg;
    uint16_t block_num = 1;
    char buffer[BLOCK_SIZE + 4];  // Max data packet size (512 bytes data + 2B opcode + 2B block#)
    ssize_t len;
    socklen_t addr_len = sizeof(*addr);

    // Step 1: Send RRQ (Read Request) message to the server
    msg = serialize_req(RRQ, (char *)filename, TFTP_MESSAGE_MODE);
    sendto(sockfd, msg, strlen(filename) + strlen(TFTP_MESSAGE_MODE) + 4, 0, (struct sockaddr *)addr, sizeof(*addr));
    free(msg);

    // Step 2: Receive DATA packets in a loop until the last packet is received
    while (1) {
        len = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)addr, &addr_len);
        if (len == -1) {
            perror("recvfrom");
            break;
        }

        // Verify if the message is a DATA packet (opcode = 3)
        uint16_t opcode = (buffer[0] << 8) | buffer[1];
        uint16_t received_block_num = (buffer[2] << 8) | buffer[3];
        
        if (opcode != DATA) {
            fprintf(stderr, "Received unexpected opcode: %d\n", opcode);
            break;
        }

        // If the block number doesn't match expected, ignore the packet
        if (received_block_num != block_num) {
            fprintf(stderr, "Received unexpected block number: %d\n", received_block_num);
            continue;
        }

        // Step 3: Write data to a file (start of file, if first block)
        FILE *file = fopen(filename, "ab");  // Open the file for appending
        if (file == NULL) {
            perror("fopen");
            break;
        }

        // Write data (excluding opcode and block number)
        fwrite(buffer + 4, 1, len - 4, file);
        fclose(file);

        // Step 4: Send ACK for the received block
        char ack_msg[4] = {0};  // ACK message: 2 bytes opcode (4), 2 bytes block number
        ack_msg[0] = (char)(ACK >> 8);
        ack_msg[1] = (char)(ACK & 0xFF);
        ack_msg[2] = (char)(block_num >> 8);
        ack_msg[3] = (char)(block_num & 0xFF);
        sendto(sockfd, ack_msg, 4, 0, (struct sockaddr *)addr, sizeof(*addr));

        // If the data length is less than 512 bytes, we received the last block
        if (len - 4 < BLOCK_SIZE) {
            break;
        }

        // Increment the block number for the next packet
        block_num++;
    }

    printf("File received and saved successfully.\n");
}

int
main (int argc, char *argv[])
{
    int err;
    int sockfd;
    struct arguments   *args   = {0};
    struct sockaddr_in *myaddr = {0};
    struct sockaddr_in *addr   = {0};
    
    err = parse_args(argc, argv, args);
    if (err == -1) {
        fprintf(stdout, STRING_USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }

    sockfd = setup_socket(args, myaddr, addr);
    if ( sockfd == -1)
        exit(EXIT_FAILURE);

    switch (args->operation) {
    case FLAG_READ:
        receive_file(sockfd, addr, args->filename);
        break;

    default:
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}