#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "parse.h"

void list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {

}

int add_employee(struct dbheader_t *dbhdr, struct employee_t *employees, char *addstring) {

    char *name = strtok(addstring,",");
    char *addr = strtok(NULL, ",");
    char *hours = strtok(NULL, ",");

    strncpy(employees[dbhdr->count-1].name, name, sizeof(employees[dbhdr->count-1].name));
    strncpy(employees[dbhdr->count-1].address, addr, sizeof(employees[dbhdr->count-1].address));
    employees[dbhdr->count-1].hours = atoi(hours);
    
    return STATUS_SUCCESS;

}

int read_employees(int fd, struct dbheader_t *dbhdr, struct employee_t **employeesOut) {
    if (fd < 0) {
        printf("Database FD is not valid\n");
        return STATUS_ERROR;
    }
    
    int count = dbhdr->count;

    struct employee_t *employees = calloc(count, sizeof(struct employee_t));
    if (employees == -1) {
        printf("Malloc failed to create employees structure.\n");
        return STATUS_ERROR;
    }
    
    read(fd, employees, count*sizeof(struct employee_t));
    for(int i = 0; i < count; i++) {
        employees[i].hours = ntohl(employees[i].hours);
    }

    *employeesOut = employees;
    return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *dbhdr, struct employee_t *employees) {
    if (fd < 0) {
        printf("Database FD is not valid\n");
        return STATUS_ERROR;
    }

    // pack to network byte order
    dbhdr->magic = htonl(dbhdr->magic);
    dbhdr->version = htons(dbhdr->version);
    dbhdr->count = htons(dbhdr->count);
    dbhdr->filesize = htonl(dbhdr->filesize);

    lseek(fd, 0, SEEK_SET);

    write(fd, dbhdr, sizeof(struct dbheader_t));
    return STATUS_SUCCESS;
}	

int validate_db_header(int fd, struct dbheader_t **headerOut) {
    if (fd < 0) {
        printf("Database FD is not valid\n");
        return STATUS_ERROR;
    }

    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == -1) {
        printf("Malloc failed to create a db header\n");
        return STATUS_ERROR;
    }   

    if (read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        perror("bad header read");
        free(header);
        return STATUS_ERROR;
    }

    // unpack to host (little) endian -- data always stored in network (big) endian
    header->magic = ntohl(header->magic);
    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->filesize = ntohl(header->filesize);

    if (header->version != 1) {
        printf("Bad database version. (%d)\n",header->version);
        free(header);
        return STATUS_ERROR;
    }

    if (header->magic != HEADER_MAGIC) {
        printf("Invalid db magic bytes.\n");
        free(header);
        return STATUS_ERROR;
    }

    struct stat dbstat = {0};
    fstat(fd, &dbstat);
    if (header->filesize != dbstat.st_size) {
        printf("Corrupted database file detected.\n");
        free(header);
        return STATUS_ERROR;
    }

    *headerOut = header;
    return STATUS_SUCCESS;

}

int create_db_header(int fd, struct dbheader_t **headerOut) {
    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == -1) {
        printf("Malloc failed to create db header\n");
        return STATUS_ERROR;
    }

    header->version = 0x1;
    header->count = 0;
    header->magic = HEADER_MAGIC;
    header->filesize = sizeof(struct dbheader_t);

    *headerOut = header;
    return STATUS_SUCCESS;
}


