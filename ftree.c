#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <libgen.h>
#include "ftree.h"
#include "hash.h"

#define PERM_VAL 0777

int send_server(char *src_path, char *dest_path, int soc){
    struct fileinfo *root = malloc(sizeof(struct fileinfo));
    struct dirent *ent;
    struct stat file;
    char abs_path[PATH_MAX];
    mode_t mode;
    size_t size;
    int is_match;
    int transmit;
    const char *bname;
    char *cat_dest_path = malloc(MAXPATH*sizeof(char *));
    // check if entered path is valid
    if (lstat(src_path, &file) == -1){
        fprintf(stderr, "lstat: the name \'%s\' is not a file or directory\n", src_path);
        exit(1);
    }
    // get the absolute path of src_path
    realpath(src_path, abs_path);
    // copy abs_path into root->path
    strcpy(root->path, abs_path);
    root->mode = file.st_mode;
    // htonl root->mode so it can be sent over servers without messing with the values
    mode = htonl(root->mode);
    root->hash[0] = '\0';
    root->size = file.st_size;
    // same reason for mode
    size = htonl(root->size);
    bname = basename(src_path);
    snprintf(cat_dest_path, MAXPATH, "%s/%s", dest_path, bname);
    // if it's a file
    if(S_ISREG(file.st_mode)){
        // open the file
        FILE* fp;
        // check if there is permission for the file
        if((fp = fopen(src_path, "rb")) == NULL){
            fprintf(stderr, "insufficient permission for the file \'%s\' \n", src_path);
            exit(1);
        }
        // set the hash
        strncpy(root->hash, hash(fp), HASH_SIZE);
        fclose(fp);
    }
    // write everything to server
    write(soc, cat_dest_path, MAXPATH*sizeof(char));
    write(soc, &mode, sizeof(mode_t));
    write(soc, root->hash, HASH_SIZE*sizeof(char));
    write(soc, &size, sizeof(size_t));
    // check the returned match
    read(soc, &is_match, sizeof(int));
    // is_match returned an error
    if(is_match == MATCH_ERROR){
        fprintf(stderr, "match error with directory: %s\n", src_path);
        exit(1);
    }
    // mismatch should send contents to server
    if(is_match == MISMATCH){
        FILE* fp;
        int filelength;
        // check if there is permission for the file
        if((fp = fopen(src_path, "rb")) == NULL){
            fprintf(stderr, "insufficient permission for the file \'%s\' \n", src_path);
            exit(1);
        }
        char *buffer;
        // copying text
        fseek(fp, 0, SEEK_END);
        filelength = ftell(fp);
        rewind(fp);
        buffer = malloc((filelength + 1)*sizeof(char *));
        write(soc, &filelength, sizeof(int));
        for(int i = 0; i < filelength; i++) {
            if(fread((buffer + i), 1, 1, fp) < 0){
                perror("client: fread");
                exit(1);
            }
            if(write(soc, (buffer + i), 1) < 0){
                perror("client: write");
                exit(1);
            }
        }
        read(soc, &transmit, sizeof(int));
        if(transmit == TRANSMIT_ERROR){
            fprintf(stderr, "server transmit error with file: \'%s\'", src_path);
        }
        free(buffer);
        fclose(fp);
    }
    // if path is directory recursively call the subdirectories
    if (S_ISDIR(file.st_mode)){
        char path[PATH_MAX];
        DIR *dir;
        // if the directory has insufficient permission, then exit
        if(!(dir = opendir(src_path))){
                fprintf(stderr, "insufficient permission for the directory \'%s\' \n", src_path);
                exit(1);
        }
        // loop while there is still a subdirectory
        while((ent = readdir(dir)) != NULL){
            // get rid of subdirectories that start with a '.'
            if(ent->d_name[0] == '.' || S_ISLNK(file.st_mode)){
                continue;
            }
            // get the path to the subdirectory 
            snprintf(path, sizeof(path)-1, "%s/%s", src_path, ent->d_name);
            send_server(path, cat_dest_path, soc);
        }
        // close the directory
        closedir(dir);
    }
    free(root);
    return 0;
}

int fcopy_client(char *src_path, char *dest_path, char *host, int port){
    int retv;
    // create the socket
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc < 0) {
        perror("socket");
        exit(1);
    }

    // set up the sockaddr_in struct for connecting
    struct sockaddr_in peer;
    peer.sin_family = AF_INET;
    peer.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &peer.sin_addr) < 1) {
        perror("inet_pton");
        close(soc);
        exit(1);
    }

    // connect the socket
    if (connect(soc, (struct sockaddr *)&peer, sizeof(peer)) < -1) {
        perror("connect");
        close(soc);
        exit(1);
    }
    retv = send_server(src_path, dest_path, soc);
    close(soc);
    return retv; 
}



int setup(int port){
    int on = 1, status;
    struct sockaddr_in self;
    int listenfd;
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("socket");
      exit(1);
    }

    // Make sure we can reuse the port immediately after the
    // server terminates.
    status = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                        (const char *) &on, sizeof(on));
    if(status == -1) {
      perror("setsockopt -- REUSEADDR");
    }

    self.sin_family = AF_INET;
    self.sin_addr.s_addr = INADDR_ANY;
    self.sin_port = htons(PORT);
    memset(&self.sin_zero, 0, sizeof(self.sin_zero));  // Initialize sin_zero to 0

    printf("Listening on %d\n", PORT);
 
    if (bind(listenfd, (struct sockaddr *)&self, sizeof(self)) == -1) {
      perror("bind"); // probably means port is in use
    exit(1);
    }

    if (listen(listenfd, 5) == -1) {
      perror("listen");
      exit(1);
    }
    return listenfd;  
}
int make_dir(int destpermission, int cpypermission, struct stat dest_file, struct fileinfo *root){
    mode_t promask = umask(0);
        if(mkdir(root->path, cpypermission)== -1){
            if (lstat(root->path, &dest_file) == -1){
                fprintf(stderr, "lstat: the name \'%s\' is not a file or directory\n", root->path);
                return MATCH_ERROR;
            }
            if(S_ISDIR(dest_file.st_mode)){
                destpermission = dest_file.st_mode & PERM_VAL;
                if(cpypermission != destpermission){
                    if(chmod(root->path, cpypermission) == -1){
                        fprintf(stderr, "chmod error for file \'%s\' \n", root->path);
                        return MATCH_ERROR;
                    }
                }

            }
            else{
                fprintf(stderr, "file name already exits: \'%s\' \n", root->path);
                return MATCH_ERROR;
            }
        }
    umask(promask);
    return MATCH;
}
int make_file(int destpermission, int cpypermission, struct stat dest_file, char dest_hash[HASH_SIZE], struct fileinfo *root){
    FILE* fp;
    // make the file/open the file for reading
    if((fp = fopen(root->path, "ab+")) == NULL){
        if (lstat(root->path, &dest_file) == -1){
            fprintf(stderr, "lstat: the name \'%s\' is not a file or directory\n", root->path);
            return MATCH_ERROR;
        }
        // case where file trying to replace directory with same name
        if(S_ISDIR(dest_file.st_mode)){
            fprintf(stderr, "directory name already exists: %s \n", root->path);
            return MATCH_ERROR;
        }
        // case where dest file has no permission
        else{
            fprintf(stderr, "insufficient permission for the destpath file \'%s\' \n", root->path);
            return MATCH_ERROR;
        }
    }
    if (lstat(root->path, &dest_file) == -1){
        fprintf(stderr, "lstat: the name \'%s\' is not a file or directory\n", root->path);
        return MATCH_ERROR;
    }
    destpermission = dest_file.st_mode & PERM_VAL;
    if(cpypermission != destpermission){
        if(chmod(root->path, cpypermission) == -1){
            fprintf(stderr, "chmod error for file \'%s\' \n", root->path);
            return MATCH_ERROR;
        }
    }
    // different sizes
    if((root->size == dest_file.st_size)){
        // need to read destpath
        strcpy(dest_hash, hash(fp));
        if(strcmp(root->hash, dest_hash)){
            return MISMATCH;
        }
    }
    else{
        return MISMATCH;
    }
    fclose(fp);
    return MATCH;
}
int read_struct(struct fileinfo *root, int fd, int retv){
    char path[MAXPATH];
    char mode[sizeof(mode_t)];
    char size[sizeof(size_t)];
    char hash[HASH_SIZE];
    int counter = 0;
    int i = 0;
    int nbytes;
    // Receive structs
    while (counter < MAXPATH){
      nbytes = read(fd, path + i, 1);
      if (nbytes == 0) {
        close(fd);
        return 1;
      } else if (nbytes < 0) {
        perror("server: path read");
        retv = MATCH_ERROR;
        write(fd, &retv, sizeof(int));
        return 1;
      }
      i++;
      counter += nbytes;
    }
    strcpy(root->path, path);

    i = 0;
    counter = 0;
    while (counter < sizeof(mode_t)){
      nbytes = read(fd, mode + i, 1);
        if (nbytes < 0) {
        perror("server: mode read");
        retv = MATCH_ERROR;
        write(fd, &retv, sizeof(int));
        return 1;
      }
      i++;
      counter += nbytes;
    }

    i = 0;
    counter = 0;
    while (counter < HASH_SIZE){
      nbytes = read(fd, hash + i, 1);
        if (nbytes < 0) {
        perror("server: hash read");
        retv = MATCH_ERROR;
        write(fd, &retv, sizeof(int));
        return 1;
      }
      i++;
      counter += nbytes;
    }
    for (int i = 0; i < HASH_SIZE; i++) {
      root->hash[i] = hash[i];
    }

    i = 0;
    counter = 0;
    while (counter < sizeof(size_t)){
      nbytes = read(fd, size + i, 1);
        if (nbytes < 0) {
        perror("server: size read");
        retv = MATCH_ERROR;
        write(fd, &retv, sizeof(int));
        return 1;
      }
      i++;
      counter += nbytes;
    }
    root->mode = ntohl(* (int *)mode);
    root->size = ntohl(* (off_t *)size);
    return 0;
}
void fcopy_server(int port){
    int listenfd, fd;
    socklen_t socklen;
    struct sockaddr_in peer;
    listenfd = setup(port);
    socklen = sizeof(peer);

    int retv = MATCH;
    int transmit = TRANSMIT_OK; 
    int checker;

    int destpermission = 0;
    int cpypermission;
    int filelength;
    char cpypath[MAXPATH];

    char dest_hash[HASH_SIZE];
    struct fileinfo *root = malloc(sizeof(struct fileinfo));
    struct stat dest_file;
    while(1){
        if ((fd = accept(listenfd, (struct sockaddr *)&peer, &socklen)) < 0) {
            perror("accept");
        } else {
        printf("New connection on port %d\n", ntohs(peer.sin_port));
        // keep reading dest_path until there are no more files/directories to copy
        while(1){
            checker = read_struct(root, fd, retv);
            if(checker == 1){
                break;
            }
            retv = MATCH;
            transmit = TRANSMIT_OK;
            dest_hash[0] = '\0';
            strcpy(cpypath, root->path);
            dirname(root->path);
            // error checker for when dest_path leads to a file (which is not allowed)
            if (lstat(root->path, &dest_file) != -1){
                if(S_ISREG(dest_file.st_mode)){
                    printf("%s cannot be a file \n", root->path);
                    retv = MATCH_ERROR;
                    write(fd, &retv, sizeof(int));
                    continue;
                }
            }
            strcpy(root->path, cpypath);
            cpypermission = root->mode & PERM_VAL;
            if(S_ISREG(root->mode)){
                // check if the file errors/matches/mismatches
                retv = make_file(destpermission, cpypermission, dest_file, dest_hash, root);
                // tell the client what's happening
                write(fd, &retv, sizeof(int));
                // if the retv was error stop this loop
                if(retv == MATCH_ERROR){
                    continue;
                }
                // if mismatch read the file contents and copy it over
                else if(retv == MISMATCH){
                    FILE *fp;
                    char buffer = '\0';
                    if((fp = fopen(root->path, "wb")) == NULL){
                        fprintf(stderr, "insufficient permission for the destpath file \'%s\' \n", root->path);
                        retv = MATCH_ERROR;
                        write(fd, &retv, sizeof(int));
                        continue;
                    }
                    read(fd, &filelength, sizeof(int));
                    for(int i = 0; i < filelength; i++){
                        // transmit errors when reading/writing
                        if(read(fd, &buffer, 1) < 0){
                            transmit = TRANSMIT_ERROR;
                            break;
                        }
                        if(fwrite(&buffer, 1, 1, fp) < 0){
                            transmit = TRANSMIT_ERROR;
                            break;
                        }
                    }
                    write(fd, &transmit, sizeof(int));
                    fclose(fp);
                }   
            }

            else if (S_ISDIR(root->mode)){
                // directory has no checking just make make the directory or don't
                retv = make_dir(destpermission, cpypermission, dest_file, root);
                write(fd, &retv, sizeof(int));
            }
        }
    }      
    close(fd);
    }
}