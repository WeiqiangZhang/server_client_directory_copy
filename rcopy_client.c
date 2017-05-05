#include <stdio.h>
#include "ftree.h"

#ifndef PORT
  #define PORT 30000
#endif
int main(int argc, char** argv){
    // The return value to check if the copy was successful
	int retv;
    // If the user did not enter 3 arguments, display error
    if (argc != 4) {
        printf("Usage:\n\tfcopy SRC DEST IPAD\n");
        return -1;
    }
    // Call fcopy_client method from ftree.c
    // argv[1] is path of the client, argv[2] is the path of the server, argv[3] is the IP adress (tested with 127.0.0.1)
    retv = fcopy_client(argv[1], argv[2], argv[3], PORT);
    if(retv == 0){
    	printf("copy successful\n");
    }
    return 0;
}