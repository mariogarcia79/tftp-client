/**
* Practica Tema 7: Cliente TFTP
*
* Garcia Carbonero, Mario
* Adan de la Fuente, Hugo
*
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define STRING_USAGE "Usage: %s <IP address> {-r|-w} <file>\n"
#define SERVICE_NAME "tftp"
#define SERVICE_PROTO "udp"
#define TFTP_MESSAGE_MODE "octet"
#define BLOCK_SIZE 512

struct arguments {
    char *program_name;
    char *ip_address;
    char *filename;
    int operation;
};

typedef enum {
    RRQ = 1,
    WRQ = 2,
    DATA = 3,
    ACK = 4,
    ERROR = 5
} OpCode;

typedef enum {
    FLAG_UNKNOWN,
    FLAG_READ,
    FLAG_WRITE
} FlagType;

FlagType
get_flag(const char *arg) {
    if (!strcmp(arg, "-r")) return FLAG_READ;
    if (!strcmp(arg, "-w")) return FLAG_WRITE;
    return FLAG_UNKNOWN;
}

int
parse_args(int argc, char *argv[], struct arguments *args)
{
    if (argc != 4) {
        errno = EINVAL;
        return -1;
    }

    args->program_name = argv[0];
    args->ip_address = argv[1];
    args->filename = argv[3];

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

int
setup_socket(
    struct arguments *args,
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
    addr->sin_port = serv->s_port;
    if (inet_aton(args->ip_address, &addr->sin_addr) == 0) {
        fprintf(stderr, "Invalid IP address: %s \n", args->ip_address);
        return -1;
    }

    myaddr->sin_family = AF_INET;
    myaddr->sin_port = serv->s_port;
    myaddr->sin_addr.s_addr = INADDR_ANY;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    if (bind(sockfd, (struct sockaddr *) myaddr, sizeof(*myaddr)) == -1) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

char
*serialize_req(uint16_t opcode, char *filename, char *mode)
{
    uint16_t network_opcode;
    char *msg_start = NULL;
    char *msg = (char *)malloc(
        sizeof(opcode)   +
        strlen(filename) +
        strlen(mode)   + 2 // Account for EOS
    );

    msg_start = msg; // save initial pointer

    if (!msg) return NULL;

    network_opcode = htons(opcode);  // Convert to network byte order
    memcpy(msg, &network_opcode, sizeof(network_opcode));
    msg += sizeof(network_opcode);
    memcpy(msg, filename, strlen(filename) + 1);
    msg += strlen(filename) + 1;
    memcpy(msg, mode, strlen(mode) + 1);

    return msg_start;
}

int
receive_file(int sockfd, struct sockaddr_in *addr, const char *filename)
{
    char *msg;
    char ack_msg[4];
    char buffer[BLOCK_SIZE + 4]; // Maximum size of received packet
    ssize_t   msg_len;
    socklen_t addr_len  = sizeof(*addr);
    uint16_t  opcode;
    uint16_t  block_num = 1; // Block number for comparison of order
    uint16_t  received_block_num;
    
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("fopen");
        close(sockfd);
        return -1;
    }

    msg = serialize_req(RRQ, (char *)filename, TFTP_MESSAGE_MODE);
    if (sendto(sockfd, msg, strlen(filename) + strlen(TFTP_MESSAGE_MODE) + 4, 0, 
            (struct sockaddr *)addr, sizeof(*addr)) == -1) {
        perror("sendto");
        close(sockfd);
        fclose(file);
        free(msg);
        return -1;
    }
    free(msg);
    printf("Solicitud de lectura de \"%s\" enviada al servidor tftp.\n", filename);

    while (1) {
        msg_len = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                (struct sockaddr *)addr, &addr_len);
        if (msg_len == -1) {
            perror("recvfrom");
            break;
        }

        memcpy(&opcode,             buffer + 0, 2); // Get opcode
        memcpy(&received_block_num, buffer + 2, 2); // Get block number
        
        // COnvert to host byte order
        opcode = ntohs(opcode);
        received_block_num = ntohs(received_block_num);

        if (opcode == ERROR) {
            // Reuse block number variable, actually reading errcode
            fprintf(stdout, "Error %02u: %s\n", received_block_num, buffer + 4);
            break;
        } 
        
        if (opcode != DATA) {
            fprintf(stderr, "El codigo de operacion recibido es erroneo: %d\n", opcode);
            break;
        }

        printf("Recibido bloque del servidor (numero de bloque %u)\n", received_block_num);

        if (received_block_num != block_num) {
            fprintf(stderr, "El numero de bloque no coincide: %d\n", received_block_num);
            break;
        }

        // Remove the first 4B of the packet, its the header.
        // Use bytes units of data, account for content size (packet - 4B).
        fwrite(buffer + 4, 1, msg_len - 4, file);
    
        // Prepare ACK fields
        opcode = ACK;
        opcode = htons(opcode);
        received_block_num = htons(received_block_num);

        memcpy(ack_msg + 0, &opcode,             2); // Set opcode
        memcpy(ack_msg + 2, &received_block_num, 2); // Set block number
        
        if (sendto(sockfd, ack_msg, 4, 0, (struct sockaddr *)addr, sizeof(*addr)) == -1) {
            perror("sendto");
            break;
        }
        printf("Enviado ACK del bloque %u.\n", block_num);

        if (msg_len - 4 < BLOCK_SIZE) {
            fprintf(stdout, "El bloque %u es el ultimo.\n", block_num);
            fprintf(stdout, "Cierre del fichero y del socket udp.\n");
            break;
        }
        block_num++;
    }

    fclose(file);
    close(sockfd);

    return 0;
}

int
send_file(int sockfd, struct sockaddr_in *addr, const char *filename)
{
    int last_ack = 0;
    char *msg;
    char buffer[BLOCK_SIZE + 4]; // Maximum size of the data packet
    ssize_t msg_len;
    socklen_t addr_len = sizeof(*addr);
    uint16_t opcode;
    uint16_t block_num = 0; // Block number to start from
    uint16_t received_block_num;

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("fopen");
        close(sockfd);
        return -1;
    }

    // Send WRQ to the server
    msg = serialize_req(WRQ, (char *)filename, TFTP_MESSAGE_MODE);
    if (sendto(sockfd, msg, strlen(filename) + strlen(TFTP_MESSAGE_MODE) + 4, 0, 
               (struct sockaddr *)addr, sizeof(*addr)) == -1) {
        perror("sendto");
        close(sockfd);
        fclose(file);
        free(msg);
        return -1;
    }
    free(msg);
    printf("Solicitud de escritura de \"%s\" enviada al servidor tftp.\n", filename);

    // Send data blocks
    while (1) {
        msg_len = recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                (struct sockaddr *)addr, &addr_len);
        if (msg_len == -1) {
            perror("recvfrom");
            fclose(file);
            return -1;
        }

        memcpy(&opcode, buffer + 0, 2);  // Get opcode
        memcpy(&received_block_num, buffer + 2, 2); // Get block number

        // Convert to host byte order
        opcode = ntohs(opcode);
        received_block_num = ntohs(received_block_num);

        if (opcode == ERROR) {
            fprintf(stdout, "Error %02u: %s\n", received_block_num, buffer + 4);
            fclose(file);
            return -1;
        }

        if (opcode != ACK) {
            fprintf(stderr, "Codigo de operación recibido incorrecto: %d\n", opcode);
            fclose(file);
            return -1;
        }

        fprintf(stdout, "Recibido ACK del servidor (numero de bloque %u)\n", received_block_num);
        if (last_ack) {
            fprintf(stdout, "El bloque %u es el ultimo.\n", block_num);
            printf("Cierre del fichero y del socket udp.\n");
            break;
        }
        
        if (received_block_num == 0) {
            received_block_num = 1;
            block_num = 1;
        }

        // Read the next block of data
        size_t bytes_read = fread(buffer + 4, 1, BLOCK_SIZE, file);
        if (bytes_read == 0) {
            break;
        }

        // Prepare the DATA packet
        opcode = DATA;
        opcode = htons(opcode); // Convert to network byte order
        block_num = htons(block_num); // Convert to network byte order

        // Copy opcode and block number into the buffer
        memcpy(buffer + 0, &opcode, 2);
        memcpy(buffer + 2, &block_num, 2);

        // reset block_num byte order for comparisons.
        block_num = ntohs(block_num);

        // Send the DATA packet
        if (sendto(sockfd, buffer, bytes_read + 4, 0, (struct sockaddr *)addr, sizeof(*addr)) == -1) {
            perror("sendto");
            fclose(file);
            return -1;
        }

        printf("Enviado bloque %u del fichero.\n", block_num);

        if (bytes_read < BLOCK_SIZE) last_ack = 1;

        block_num++;
    }

    fclose(file);
    close(sockfd);
    
    return 0;
}

int
main(int argc, char *argv[])
{
    int err;
    int sockfd;
    struct arguments args     = {0};
    struct sockaddr_in myaddr = {0};
    struct sockaddr_in addr   = {0};

    err = parse_args(argc, argv, &args);
    if (err == -1) {
        fprintf(stdout, STRING_USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }

    sockfd = setup_socket(&args, &myaddr, &addr);
    if (sockfd == -1)
        exit(EXIT_FAILURE);

    switch (args.operation) {
    case FLAG_READ:
        err = receive_file(sockfd, &addr, args.filename);
        if (err == -1)
            exit(EXIT_FAILURE);
        break;

    case FLAG_WRITE:
        err = send_file(sockfd, &addr, args.filename);
        if (err == -1)
            exit(EXIT_FAILURE);
        break;

    default:
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
