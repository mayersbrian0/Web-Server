/*
HTTP Web Server
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>      
#include <strings.h>    
#include <unistd.h>      
#include <sys/socket.h>  
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>

#define MAXREQSIZE 1024 //max length of the path request
#define PACKETSIZE 1024 //send 1024 bits at a time

//struct to hold the file data
typedef struct {
    long size;
    char *data;
} file_data;

//store the contents of the file into the buffer
file_data* add_fbuffer(char* filename) {
    long file_size;
    ssize_t bytes_read;
    struct stat sb;
    
    //check if the file is invalid or a directory
    if (stat(filename, &sb) != 0 || S_ISDIR(sb.st_mode)) return NULL;
    
    FILE* fp = fopen(filename, "rb"); //open the file
    if (fp == NULL) return NULL; 
    
    //get the size of the file
    fseek(fp, 0L, SEEK_END); 
    file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    
    file_data* file = malloc(sizeof( file_data* ));
    file->size = file_size;
    file->data = malloc(file_size);

    bytes_read = fread(file->data, 1, file_size, fp);
    file->data[bytes_read] = '\0';
    fclose(fp);

    //printf("(add_fbuffer) Total Bytes Read in file %s: %d\n", filename, bytes_read);
    if (bytes_read != file_size) return NULL; //problem reading entire file

    return file; 
}

//free the file contents from the file buffer
void free_fbuffer(file_data* file) {
    free(file->data);
    free(file);
    return;
}

int open_serverfd(int port) {

    int serverfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    //Get rid of "already in use" error
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR,  (const void *)&optval , sizeof(int)) < 0) return -1;

    //bulild serveraddr
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(serverfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) return -1;

    //reading to accept connection requests
    if (listen(serverfd, 1024) < 0) return -1;

    return serverfd;
}

//send error message to client
void send_error(int connection_fd) {

}

//send the packets, 
void send_packets(int connection_fd, char* version, char* content_type, char* filename) {
    char http_res[PACKETSIZE]; //send header info first
    char file_contents[PACKETSIZE]; //write other info after
    file_data* file;
    size_t n;
    int file_size = 0;
    struct stat sb;
    
    if (stat(filename, &sb) != 0 || S_ISDIR(sb.st_mode)) return NULL;
    FILE* fp = fopen(filename, "rb"); //open the file
    if (fp == NULL) {send_error(connection_fd); NULL;}; 

    //get the size of the file
    fseek(fp, 0L, SEEK_END); 
    file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    //send the header message
    printf("%s\n", version);
    memset(http_res, 0, PACKETSIZE);
    sprintf(http_res, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%ld\r\n\r\n", version, content_type, file_size);
    write(connection_fd, http_res, strlen(http_res));

    //start writting the file in chuncks
    do {
        bzero(file_contents, PACKETSIZE);
        n = fread(file_contents, 1, PACKETSIZE, fp);
        write(connection_fd, file_contents, n);
    } while (n == PACKETSIZE); 

    fclose(fp);
    return;
}

//build a http response message
void parse_response(int connection_fd) {
    char *method = NULL, *url = NULL, *version = NULL, http_req[MAXREQSIZE], tmp_filename[50], filename[50], *content_type = NULL, content_choice[50], send_version[10], v[10];
    file_data *file;
    
    //parse the request and extract useful info
    memset(http_req, 0, MAXREQSIZE);
    read(connection_fd, http_req, MAXREQSIZE);
    method = strtok(http_req, " ");
    url = strtok(NULL, " ");
    version = strtok(NULL, "\n");
    strcpy(v, version);

    //send default webpage if appropriate ( / or /inside/ )
    if (strcmp(url, "/") == 0 || strcmp(url, "/inside/") == 0) {
        strcpy(filename, "index.html");
    }

    //handle all other requests
    else {
        //strip away url path so we have a valid relative path
        if (strncmp(url, "/", 1) == 0 ) {
            memcpy(url, url+1, strlen(url)); //move over to get valid filename
        }

        if(strncmp(url, "inside/", 7) == 0) {
            memcpy(url, url+7, strlen(url));
        }
        
        strcpy(filename, url);
    }

    //get the content type
    strcpy(tmp_filename, filename);
    strtok(filename, ".");
    content_type = strtok(NULL, "");
    if (strncmp(content_type, "html", 4) == 0) {
        strcpy(content_choice, "text/html");
    }
    else if (strncmp(content_type, "css", 3) == 0) {
        strcpy(content_choice, "text/css");
    }
    else if (strncmp(content_type, "txt", 3) == 0) {
        strcpy(content_choice, "text/plain");
    }
    else if (strncmp(content_type, "png", 3) == 0) {
        strcpy(content_choice, "image/png");
    }
    else if (strncmp(content_type, "gif", 3) == 0) {
        strcpy(content_choice, "image/gif");
    }
    else if (strncmp(content_type, "jpg", 3) == 0) {
        strcpy(content_choice, "image/jpg");
    }
    else if (strncmp(content_type, "js",2) == 0) {
        strcpy(content_choice, "application/javascript");
    }
    else {
        return;
    }

    //after parsing req, send a res message
    send_packets(connection_fd, v, content_choice, tmp_filename);
    return;
}

//thread routine: handle connection with individual client
void *handle_connection(void *thread_args) {
    int connection_fd = *((int *)thread_args);
    pthread_detach(pthread_self()); //no need to call pthread_join()
    free(thread_args); //free space
    parse_response(connection_fd); //send http response to the client
    close(connection_fd); //client can now stop waiting
    return NULL;
}

int main(int argc, char** argv) {
    
    int serverfd, *connect_fd, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    char *ptr;
    pthread_t thread_id;

    if (argc != 2) {
        fprintf(stderr, "Usage %s <port>\n", argv[0]);
        exit(0);
    }

    port = strtol(argv[1], &ptr, 10);
    if (*ptr != '\0' || port <= 1024) { printf("Invalid Port Number\n"); exit(0); } //check for errors

    serverfd = open_serverfd(port);
    if (serverfd < 0) { printf("Error connecting to port %d\n", port); exit(0); }
    //server terminates on ctl-c
    while (1) {
        connect_fd = malloc(sizeof(int)); //allocate space for pointer
        *connect_fd = accept(serverfd, (struct sockaddr *)&clientaddr, &clientlen); //start accepting requests
        pthread_create(&thread_id, NULL, handle_connection, connect_fd); //pass new file descripotr to thread routine
    }
    return 0;
}