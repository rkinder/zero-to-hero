#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
    printf("Usage: %s -n -f <filepath>\n",argv[0]);
}

int main(int argc, char *argv[]) { 

    int c = 0;
    char *filepath = NULL;
    char *portarg = NULL;
    bool newfile = false;
    bool userlist = false;
    unsigned short port = 0;

    int dbfd = -1;

    if (argc == 1){
        print_usage(argv);
        return 0;
    }

    while((c = getopt(argc, argv, "nf:")) != -1) {
        switch(c) {
            case 'n':
                /* support creating a new file */
                newfile = true;
                break;
            case 'f':
                /* support a custom path*/
                filepath = optarg;
                break;
            case '?':
                printf("Unsupported option -%c\n",c);
                print_usage(argv);
                break;
            default:
                return -1;
        }
    }

    if (filepath == NULL) {
        printf("Filepath is a required argument.\n");
        print_usage(argv);

        return 0;
    }

    if (newfile) {
        // create a new database file
        dbfd = create_db_file(filepath);
    }

    printf("New file: %d\n", newfile);
    printf("File path: %s\n", filepath);
	
}
