#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BUF 1024
#define PORT 6543

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;

void *clientCommunication(void *data);
void signalHandler(int sig);

int main(void){
    socklen_t addrlen;
    struct sockaddr_in address, cliaddress;
    int reuseValue = 1;

    //Signal handler
    //Interrupt: ctrl + c
    if(signal(SIGINT, signalHandler) == SIG_ERR){
        perror("signal cannot be registered");
        return EXIT_FAILURE;
    }

    //Create socket
    if((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Socket error");
        return EXIT_FAILURE;
    }

    //Set socket options
    if(setsockopt(
        create_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuseValue,
        sizeof(reuseValue) == -1
    )){
        perror("set socket options - reuseAddr");
        return EXIT_FAILURE;
    }

    if(setsockopt(
        create_socket,
        SOL_SOCKET,
        SO_REUSEPORT,
        &reuseValue,
        sizeof(reuseValue) == -1
    )){
        perror("set socket options - reusePort");
        return EXIT_FAILURE;
    }

    //Init Address with big endian
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //Assign address with port to socket
    if(bind(create_socket, (struct sockaddr *)&address, sizeof(address))){
        perror("bind error");
        return EXIT_FAILURE;
    }

    //Allow connection establishing
    if(listen(create_socket, 5) == -1){
        perror("listen error");
        return EXIT_FAILURE;
    }

    while(!abortRequested){
        //Ignore errors
        printf("Waiting for connections...\n");

        //Accept connection setup
        addrlen = sizeof(struct sockaddr_in);
        if((new_socket = accept(
            create_socket,
            (struct sockaddr *)&cliaddress,
            &addrlen)) == -1)
        {
            if(abortRequested){
                perror("accept error after aborted");
            }
            else{
                perror("accept error");
            }
            break;
        }

        //Start client
        printf("Client connectet from %s:%d...\n",
        inet_ntoa(cliaddress.sin_addr),
        ntohs(cliaddress.sin_port));
        clientCommunication(&new_socket);
        new_socket = -1;
    }

    //Free the descriptor
    if(create_socket != -1){
        if(shutdown(create_socket, SHUT_RDWR) == -1){
            perror("shutdown create_socket");
        }
        if(close(create_socket) == -1){
            perror("close create_socket");
        }
        create_socket = -1;
    }
    
    return EXIT_SUCCESS;
}

void *clienCommunication(void *data){
    char buffer[BUF];
    int size;
    int *current_socket = (int *)data;

    //Send welcome message
    strcpy(buffer, "Welcome to the Server!\r\nPlease enter commands...\r\n");
    if(send(*current_socket, buffer, strlen(buffer), 0) == -1){
        perror("send failed");
        return NULL;
    }

    do{
        //Receive
        size = recv(*current_socket, buffer, BUF - 1, 0);
        if(size == -1){
            if(abortRequested){
                perror("recv error after aborted");
            }
            else{
                perror("recv error");
            }
            break;
        }

        if(size == 0){
            printf("Client closed remote socket\n");
            break;
        }

        if(buffer[size - 2] == '\r' && buffer[size - 1] == '\n'){
            size -= 2;
        }
        else if(buffer[size - 1] == '\n'){
            --size;
        }

        buffer[size] = '\0';
        printf("Message received: %s\n", buffer);

        if(send(*current_socket, "OK", 3, 0) == -1){
            perror("send answer failed");
            return NULL;
        }
    }
    while(strcmp(buffer, "quit") != 0 && !abortRequested);

    //Close descriptor
    if(*current_socket != -1){
        if(shutdown(*current_socket, SHUT_RDWR) == -1){
            perror("shutdown new socket");
        }
        if(close(*current_socket) == -1){
            perror("close new socket");
        }
        *current_socket = -1;
    }

    return NULL;
}

void signalHandler(int sig){
    if(sig == SIGINT){
        printf("Abort requested...");
        abortRequested = 1;

        if(new_socket != -1){
            if(shutdown(new_socket, SHUT_RDWR) == -1){
                perror("shutdown new_socket");
            }
            if(close(new_socket) == -1){
                perror("close new_socket");
            }
            new_socket = -1;
        }

        if(create_socket != -1){
            if(shutdown(create_socket, SHUT_RDWR) == -1){
                perror("shutdown create_socket");
            }
            if(close(create_socket) == -1){
                perror("close create_socket");
            }
            create_socket = -1;
        }
    }
    else{
        exit(sig);
    }
}