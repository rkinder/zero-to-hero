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

/* 
void list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {

}
*/

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
    if (employees == NULL) {
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

    // Calculate the projected filesize
    unsigned int filesize = sizeof(struct dbheader_t) + (dbhdr->count * sizeof(struct employee_t));

    // Create copies for network endianness conversion (don't modify originals)
    struct dbheader_t header_copy = *dbhdr;
    header_copy.filesize = filesize;
    
    // Convert header copy to network byte order
    header_copy.magic = htonl(header_copy.magic);
    header_copy.version = htons(header_copy.version);
    header_copy.count = htons(header_copy.count);
    header_copy.filesize = htonl(header_copy.filesize);

    lseek(fd, 0, SEEK_SET);

    // Write header
    if (write(fd, &header_copy, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        printf("Failed to write database header.\n");
        return STATUS_ERROR;
    }

    // Write employees if any exist
    if (dbhdr->count > 0 && employees != NULL) {
        // Create a copy of employees for endianness conversion
        struct employee_t *employees_copy = calloc(dbhdr->count, sizeof(struct employee_t));
        if (employees_copy == NULL) {
            printf("Failed to allocate memory for employees copy.\n");
            return STATUS_ERROR;
        }

        // Copy employees and convert to network endianness
        for (int i = 0; i < dbhdr->count; i++) {
            employees_copy[i] = employees[i];  // Copy name and address
            employees_copy[i].hours = htonl(employees[i].hours);  // Convert hours
        }

        if (write(fd, employees_copy, dbhdr->count * sizeof(struct employee_t)) != 
            dbhdr->count * sizeof(struct employee_t)) {
            printf("Failed to write employees to database.\n");
            free(employees_copy);
            return STATUS_ERROR;
        }

        free(employees_copy);
    }

    return STATUS_SUCCESS;
}	

int validate_db_header(int fd, struct dbheader_t **headerOut) {
    if (fd < 0) {
        printf("Database FD is not valid\n");
        return STATUS_ERROR;
    }

    // We have a good file descriptor, but need to always read
    // the beginning of the file
    lseek(fd, 0, SEEK_SET);

    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
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

int load_database(int fd, struct dbheader_t **headerOut, struct employee_t **employeesOut) {
    if (fd < 0) {
        printf("Database FD is not valid\n");
        return STATUS_ERROR;
    }

    // Read and validate header
    lseek(fd, 0, SEEK_SET);
    
    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        printf("Malloc failed to create a db header\n");
        return STATUS_ERROR;
    }

    if (read(fd, header, sizeof(struct dbheader_t)) != sizeof(struct dbheader_t)) {
        perror("bad header read");
        free(header);
        return STATUS_ERROR;
    }

    // Convert header from network to host endianness
    header->magic = ntohl(header->magic);
    header->version = ntohs(header->version);
    header->count = ntohs(header->count);
    header->filesize = ntohl(header->filesize);

    // Validate header
    if (header->version != 1) {
        printf("Bad database version. (%d)\n", header->version);
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

    // Read employees if any exist
    struct employee_t *employees = NULL;
    if (header->count > 0) {
        employees = calloc(header->count, sizeof(struct employee_t));
        if (employees == NULL) {
            printf("Malloc failed to create employees structure.\n");
            free(header);
            return STATUS_ERROR;
        }

        if (read(fd, employees, header->count * sizeof(struct employee_t)) != 
            header->count * sizeof(struct employee_t)) {
            printf("Failed to read employees from database.\n");
            free(header);
            free(employees);
            return STATUS_ERROR;
        }

        // Convert employees from network to host endianness
        for (int i = 0; i < header->count; i++) {
            employees[i].hours = ntohl(employees[i].hours);
        }
    }

    *headerOut = header;
    *employeesOut = employees;
    return STATUS_SUCCESS;
}

int create_db_header(struct dbheader_t **headerOut) {
    struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
    if (header == NULL) {
        printf("Malloc failed to create db header\n");
        return STATUS_ERROR;
    }

    if (headerOut == NULL) {
	printf("Header passed in was NULL\n");
	return STATUS_ERROR;
    }

    header->version = 0x1;
    header->count = 0;
    header->magic = HEADER_MAGIC;
    header->filesize = sizeof(struct dbheader_t);

    *headerOut = header;
    return STATUS_SUCCESS;
}


