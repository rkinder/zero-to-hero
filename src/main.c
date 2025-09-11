#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
    printf("Usage: %s -n -f <filepath>\n",argv[0]);
}

int main(int argc, char *argv[]) { 
	int c = 0;
    bool newfile = false;
    bool list = false;
    char *filepath = NULL;
    char *portarg = NULL;
    char *addstring = NULL;
    unsigned short port = 0;

    int dbfd = -1;
    struct dbheader_t *dbheader = NULL;
    struct employee_t *employees = NULL;

    if (argc == 1 ) {
        print_usage(argv);
        return 0;
    }

    while ((c = getopt(argc, argv, "nf:a:l")) != -1) {
        switch (c) {
            case 'f':
                /* support custom file paths */
                filepath = optarg;
                break;
            case 'n':
                /* support creating a new file */
                newfile = true;
                break;
            case 'p':
                /* support listening on a custom port */
                portarg = optarg;
                break;
            case 'l':
                /* support listing all contents*/
                list = true;
                break;
            case 'a':
                /* add a record from the cli */
                addstring = optarg;
                break;
            case '?':
                /* undefined argument flags*/
                printf("Unknown option -%c\n", c);
                break;
            default:
                printf("Cannot parse options.\n");
                return -1;
        }
    }

    if (filepath == NULL) {
        printf("Filepath is a required argument\n");
        print_usage(argv);

        return 0;
    }

    if (newfile) {
        dbfd = create_db_file(filepath);
        if (dbfd == STATUS_ERROR ) {
            printf("Could not create database file: %s\n", filepath);
            return -1;
        }

        if (create_db_header(&dbheader) == STATUS_ERROR){
            printf("Failed to create database header.\n");
            return -1;
        }

        if (output_file(dbfd, dbheader, employees) == STATUS_ERROR) {
            printf("Error writing database to disk.\n");
            return -1;
        } 
    } else {
        dbfd = open_db_file(filepath);
        if (dbfd == STATUS_ERROR ) {
            printf("Could not open database file: %s\n", filepath);
            return -1;
        }
        
        // Load existing database (header + employees) in host endianness
        if (load_database(dbfd, &dbheader, &employees) != STATUS_SUCCESS) {
            printf("Failed to load database.\n");
            return -1;
        }
    }

    if (addstring) {
        // modify the header and reallocate the size of the new list of employees
        // prior to calling the function add_employee
        dbheader->count++;
        employees = realloc(employees, dbheader->count*(sizeof(struct employee_t)));
	if (employees == NULL) {
		printf("Failed to realloc memory for employees\n");
		return -1;
	}
        
        add_employee(dbheader, employees, addstring);
    }
    dbheader->filesize = sizeof(struct dbheader_t) + (dbheader->count * sizeof(struct employee_t));
    // closing routine -- flush to disk
    output_file(dbfd, dbheader, employees);

    struct stat st;
    fstat(dbfd, &st);
    if (st.st_size != dbheader->filesize) {
	    printf("File size mismatch after write\n");
	    return STATUS_ERROR;
    }

    dbfd = close_db_file(dbfd);

    return 0;
}
