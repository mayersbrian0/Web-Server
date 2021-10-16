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

#define MAXREQSIZE 1024 //max length of the path request

//struct to hold the file data
typedef struct {
    long size;
    char *data;
} file_data;

//store the contents of the file into the buffer
file_data* add_fbuffer(char* filename) {
    long file_size;
    ssize_t bytes_read;

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


//extract info from http string
void parse_httpreq(char * http_req, char* method, char* url, char* version) {
    method = strtok(http_req, " ");
    url = strtok(NULL, " ");
    version = strtok(NULL, "\n");
}

//build a http response message
char *build_response(char* url, char* version) {

}

void send_response(int connection_fd) {
    char *method = NULL, *url = NULL, *version = NULL; //get important values
    char http_req[MAXREQSIZE];

    //read http req message
    read(connection_fd, http_req, MAXREQSIZE);
    printf("%s\n", http_req);
    parse_httpreq(http_req, method, url, version);

    //handle get requests
    if (strncmp(method, "GET", 3) == 0) {
        
    }


    //handle post requests
    else if (strncmp(method, "POST", 4) == 0) {

    }
    /*
    file_data *file = add_fbuffer("./www/index.html");
    int message_size = file->size + 1024; //size of the http message
    char http_msg[message_size]; //create buffer for message size
    char file_size_string[20];
    sprintf(file_size_string, "%ld", file->size);

    
    //construct the message
    char file_size_string[20];
    sprintf(file_size_string, "%ld", file->size);
    char *header = "HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:";
    strcpy(http_msg, header);
    strncat(http_msg, file_size_string, strlen(file_size_string));
    strcat(http_msg,"\r\n\r\n"); 
    strcat(http_msg, file->data);
    printf("%s\n", http_msg);

    write(connection_fd, http_msg, message_size); //send the data to the client
    */
}

//thread routine: handle connection with individual client
void *handle_connection(void *thread_args) {
    int connection_fd = *((int *)thread_args);
    pthread_detach(pthread_self()); //no need to call pthread_join()
    free(thread_args); //free space
    send_response(connection_fd); //send http response to the client
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