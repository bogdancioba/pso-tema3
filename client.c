#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void send_request(int socket, char *message) {
    send(socket, message, strlen(message), 0);
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(socket, buffer, sizeof(buffer), 0);
    buffer[bytes_received] = '\0';
    printf("Server Response:\n%s\n", buffer);
}

int main() {
    struct sockaddr_in server_address;
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        printf("Connection failed\n");
        return 1;
    }

    send_request(client_socket, "LIST");
    send_request(client_socket, "GET file1.txt");
    send_request(client_socket, "SEARCH example");
    send_request(client_socket, "PUT newfile.txt");
    send_request(client_socket, "PUT newfile1.txt");
    send_request(client_socket, "DELETE newfile.txt");
    send_request(client_socket, "UPDATE file1.txt");


    close(client_socket);
    return 0;
}
