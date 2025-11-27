#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

#define STRING_USAGE "Usage: %s <IP address> {-r|-w} <file>\n"

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

int
main (int argc, char *argv[])
{
    struct arguments args;
    int err;
    
    err = parse_args(argc, argv, &args);
    if (err == -1) {
        fprintf(stdout, STRING_USAGE, argv[0]);
        exit(1);
    }

    // IP con inet_aton()
    // Puerto con getservbyname()

    return 0;
}