#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include "hash.h"
/**
* This function creates an eight bit hash value from a file by reading each character 
*/
char *hash(FILE *f) {
    char hash_val[HASH_SIZE] = {'\0'};
    char *hash_value = malloc(sizeof(char *) * HASH_SIZE + 1);
    int j = 0;
    int c;
    while((c = getc(f)) != EOF){
        *(hash_val + j) = *(hash_val + j) ^ c;
        if(j != (HASH_SIZE - 1)){
            j++;
        }
            else{
                j = 0;
        }
    }
    char temp[5];
        for(int i = 0; i < HASH_SIZE; i++) {
        sprintf(temp, (i != (HASH_SIZE - 1)) ? "%.2hhx " : "%.2hhx", hash_val[i]);
        strcat(hash_value, temp);
    }
    free(hash_value);
    return hash_value;
}