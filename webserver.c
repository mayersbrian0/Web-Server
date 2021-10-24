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
#define MAXFILENAMELENGTH 50

enum METHOD{
    GET,
    POST
};

//hold different error messages
enum ERRORS {
    FILENOTFOUND,
    INTERNALSERVER
};

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
    printf("Server Listening on Port: %d\n", port);

    return serverfd;
}

//send error message to client
void send_error(int connection_fd, char* version) {
    char http_error[PACKETSIZE];
    memset(http_error, 0, PACKETSIZE);
    sprintf(http_error, "%s 500 Internal Server Error", version);
    write(connection_fd, http_error, strlen(http_error));
    return;
}

//send the packets for GET req
void send_packets_get(FILE* fp, int connection_fd, int req_size, int file_size, char* version, char* content_type) {
    char http_res[PACKETSIZE]; //send header info first
    char file_contents[PACKETSIZE]; //write other info after
    size_t n;

    //send the header message
    memset(http_res, 0, PACKETSIZE);
    sprintf(http_res, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%ld\r\n\r\n", version, content_type, file_size);
    write(connection_fd, http_res, strlen(http_res));

    //start writting the file in chuncks
    do {
        memset(file_contents, 0, PACKETSIZE);
        n = fread(file_contents, 1, PACKETSIZE, fp);
        write(connection_fd, file_contents, n);
    } while (n == PACKETSIZE); 

    return;
}

//read data from a POST request
char* send_packets_post(FILE* fp, char* http_req, int connection_fd, int req_size, int file_size, char* version, char* content_type) {
    char http_res[PACKETSIZE];
    char file_contents[PACKETSIZE];
    char post_data[500];
    int post_data_start = 0, count = 0, i = 0;
    size_t n;

    //find where post data is and copy it to a buffer
    for (i = 0; i < req_size; i++) {
        if (http_req[i] == '\r' && http_req[i +1] == '\n' && http_req[i+2] == '\r' && http_req[i+3] == '\n') {
            post_data_start = i+2; 
            break;
        }
    }

    memset(post_data, 0, MAXREQSIZE);
    sprintf(post_data, "<html><pre><h1>%s</h1></pre>", http_req+post_data_start);
    file_size += strlen(post_data);
    sprintf(http_res, "%s 200 Document Follows\r\nContent-Type:%s\r\nContent-Length:%ld\r\n\r\n%s", version, content_type, file_size, post_data);
    write(connection_fd, http_res, strlen(http_res));

    //start writting the file in chuncks
    do {
        memset(file_contents, 0, PACKETSIZE);
        n = fread(file_contents, 1, PACKETSIZE, fp);
        write(connection_fd, file_contents, n);
    } while (n == PACKETSIZE); 

    return;
}

//build a http response message
void parse_response(int connection_fd) {
    char method[10], url[MAXFILENAMELENGTH], *version = NULL, *content_type = NULL; 
    char content_choice[50], send_version[10], v[10], http_req[MAXREQSIZE], tmp_filename[MAXFILENAMELENGTH + 5], post_data[400];
    FILE* fp;
    int file_size = 0;
    struct stat sb;
    enum METHOD http_method;
    
    //parse the request and extract useful info
    memset(http_req, 0, MAXREQSIZE);
    int n = read(connection_fd, http_req, MAXREQSIZE);
    sscanf(http_req, "%s %s %s", method, url, v); //get all the relevant values

    //check the method, only support GET and POST currently
    if (strncmp(method, "GET", 3) == 0) {
        http_method = GET;
    }

    else if (strncmp(method, "POST", 4) == 0) {
        http_method = POST;
    }

    else {
        send_error(connection_fd, v);
    }

    //send default webpage if appropriate ( / or /inside/ )
    if (strcmp(url, "/") == 0 || strcmp(url, "/inside/") == 0) {
        strcpy(tmp_filename, "./www/index.html");
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
        
        strcpy(tmp_filename, "./www/");
        strcat(tmp_filename, url);
    }

    //check if the file is valid (exists and is not a directory)
    if (stat(tmp_filename, &sb) != 0 || S_ISDIR(sb.st_mode)) { send_error(connection_fd, v); return NULL; };
    fp = fopen(tmp_filename, "rb"); //open the file
    if (fp == NULL) { send_error(connection_fd, v); NULL;}; 
    
    //get the size of the file
    fseek(fp, 0L, SEEK_END); 
    file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    //get the content type if the filename is valid
    if (strtok(tmp_filename, ".") == NULL) {send_error(connection_fd, tmp_filename); return;};
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

    //call the proper handler
    switch (http_method) {
        case GET: send_packets_get(fp, connection_fd, n, file_size, v, content_choice); break;
        case POST: send_packets_post(fp, http_req, connection_fd, n, file_size, v, content_choice); break;
        default: send_error(connection_fd, v); break;
    }

    fclose(fp);
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