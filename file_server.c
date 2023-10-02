#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>


#define MAX_FILES 10
#define PORT 8080
#define BUFFER_SIZE 4096
#define DEBOUNCE_DELAY 2

#define OP_LIST    "LIST"
#define OP_GET     "GET"
#define OP_PUT     "PUT"
#define OP_DELETE  "DELETE"
#define OP_UPDATE  "UPDATE"
#define OP_SEARCH  "SEARCH"

pthread_t indexing_thread;
pthread_mutex_t indexing_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t indexing_cond = PTHREAD_COND_INITIALIZER;

typedef struct {
    char word[BUFFER_SIZE];
    int count;
} WordCount;

typedef struct {
    char filename[BUFFER_SIZE];
    WordCount top_words[10];
} FileIndex;

FileIndex files_index[MAX_FILES];

int compare_word_count(const void *a, const void *b) {
    WordCount *wordA = (WordCount *)a;
    WordCount *wordB = (WordCount *)b;
    return wordB->count - wordA->count;
}

void log_operation(const char* message) {
    pthread_mutex_lock(&log_mutex);

    FILE* log_file = fopen("server.log", "a");
    if (log_file) {
        fprintf(log_file, "%s\n", message);
        fclose(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

void log_operation_threadsafe(const char *message) {
    pthread_mutex_lock(&log_mutex);
    log_operation(message);
    pthread_mutex_unlock(&log_mutex);
}

void update_file_index(char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return;

    char word[BUFFER_SIZE];
    WordCount word_counts[1000];
    int total_words = 0;

    while (fscanf(file, "%s", word) != EOF) {
        for (int i = 0; i < strlen(word); i++) {
            word[i] = tolower(word[i]);
        }
        int found = 0;
        for (int i = 0; i < total_words; i++) {
            if (strcmp(word_counts[i].word, word) == 0) {
                word_counts[i].count++;
                found = 1;
                break;
            }
        }
        if (!found) {
            strcpy(word_counts[total_words].word, word);
            word_counts[total_words].count = 1;
            total_words++;
        }
    }

    qsort(word_counts, total_words, sizeof(WordCount), compare_word_count);

    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(files_index[i].filename, filename) == 0 || strlen(files_index[i].filename) == 0) {
            strcpy(files_index[i].filename, filename);
            for (int j = 0; j < 10 && j < total_words; j++) {
                files_index[i].top_words[j] = word_counts[j];
            }
            break;
        }
    }

    fclose(file);
}

void* indexing_thread_func(void* arg) {
    while (1) {
        pthread_mutex_lock(&indexing_mutex);
        pthread_cond_wait(&indexing_cond, &indexing_mutex);

        DIR *d;
        struct dirent *dir;
        d = opendir(".");
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (strstr(dir->d_name, ".txt")) {
                    update_file_index(dir->d_name);
                }
            }
            closedir(d);
        }

        pthread_mutex_unlock(&indexing_mutex);
    }

    return NULL;
}

void trigger_indexing() {
    pthread_cond_signal(&indexing_cond);
}


int server_socket;




void handle_list(int client_socket) {
    char* files[] = {"file1.txt", "file2.txt"};
    char response[BUFFER_SIZE] = {0};
    
    for (int i = 0; i < sizeof(files)/sizeof(files[0]); i++) {
        strcat(response, files[i]);
        strcat(response, "\n");
    }

    send(client_socket, response, strlen(response), 0);
    log_operation("LIST operation processed");
    trigger_indexing();
}

void handle_get(int client_socket, char* filename) {
    int file = open(filename, O_RDONLY);
    off_t offset = 0;
    struct stat file_stat;

    if (file == -1) {
        send(client_socket, "No file found", sizeof("No file found"), 0);
        return;
    }

    fstat(file, &file_stat);
    sendfile(client_socket, file, &offset, file_stat.st_size);
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "GET operation processed for file: %s", filename);
    log_operation(message);
    close(file);
    trigger_indexing();
}

void handle_put(int client_socket, char* filename) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    int file = open(filename, O_WRONLY | O_CREAT, 0644);
    if (file == -1) {
        send(client_socket, "File upload failed: Couldn't open file.", 40, 0);
        return;
    }

    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        write(file, buffer, bytes_received);
    }

    char message[BUFFER_SIZE];
    send(client_socket, "File uploaded successfully", sizeof("File uploaded successfully"), 0);
    snprintf(message, sizeof(message), "PUT operation processed for file: %s", filename);
    log_operation_threadsafe(message);

    close(file);
    sleep(DEBOUNCE_DELAY);
    trigger_indexing();
}

void handle_delete(int client_socket, char* filename) {
    if (unlink(filename) == -1) {
        send(client_socket, "File deletion failed: File not found or permission denied.", 62, 0);
        return;
    }

    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "DELETE operation processed for file: %s", filename);
    send(client_socket, "File deleted successfully", sizeof("File deleted successfully"), 0);
    log_operation_threadsafe(message);

    sleep(DEBOUNCE_DELAY);
    trigger_indexing();
}

void handle_update(int client_socket, char* filename) {
    handle_put(client_socket, filename);
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "UPDATE operation processed for file: %s", filename);
    send(client_socket, "File updated successfully", sizeof("File updated successfully"), 0);
    log_operation_threadsafe(message);

    sleep(DEBOUNCE_DELAY);
    trigger_indexing();
}

void handle_search(int client_socket, char* keyword) {
    char response[BUFFER_SIZE] = {0};
    for (int i = 0; i < MAX_FILES && strlen(files_index[i].filename) > 0; i++) {
        for (int j = 0; j < 10; j++) {
            if (strcmp(files_index[i].top_words[j].word, keyword) == 0) {
                strcat(response, files_index[i].filename);
                strcat(response, "\n");
                break;
            }
        }
    }
    send(client_socket, response, strlen(response), 0);
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "SEARCH operation processed for keyword: %s", keyword);
    log_operation(message);
    trigger_indexing();
}

void process_operation(int client_socket, char* buffer) {
    char op_buffer[BUFFER_SIZE];
    strncpy(op_buffer, buffer, BUFFER_SIZE);

    char* operation = strtok(op_buffer, " ");
    char* filename = strtok(NULL, " ");

    if (strncmp(operation, OP_LIST, strlen(OP_LIST)) == 0) {
         handle_list(client_socket);
    } else if (strncmp(operation, OP_GET, strlen(OP_GET)) == 0) {
         handle_get(client_socket, filename);
    } else if (strncmp(operation, OP_PUT, strlen(OP_PUT)) == 0) {
         handle_put(client_socket, filename);
    } else if (strncmp(operation, OP_DELETE, strlen(OP_DELETE)) == 0) {
         handle_delete(client_socket, filename);
    } else if (strncmp(operation, OP_UPDATE, strlen(OP_UPDATE)) == 0) {
         handle_update(client_socket, filename);
    } else if (strncmp(operation, OP_SEARCH, strlen(OP_SEARCH)) == 0) {
         handle_search(client_socket, filename);
    } else {
         printf("Received unknown operation: %s\n", operation);
         char response[] = "Error: Unknown operation received.\n";
         send(client_socket, response, strlen(response), 0);  
    }
}




void init_server() {
    struct sockaddr_in server_address;
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Server socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
}

void* handle_client(void* arg) {
    int client_socket = *((int*) arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytes_received] = '\0';
        process_operation(client_socket, buffer);
    }

    close(client_socket);
    pthread_exit(NULL);
}

void start_server() {
    init_server();

    printf("Server started on port %d\n", PORT);
    log_operation("Server started");
    while (1) {
        int* client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, NULL, NULL);
        log_operation("New client connected");


        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, (void*) client_socket);
    }
}




void graceful_shutdown(int signum) {
    printf("\nReceived signal %d. Closing server...\n", signum);
    log_operation("Server shutting down gracefully");
    close(server_socket);
    exit(EXIT_SUCCESS);
}


int main() {
    signal(SIGTERM, graceful_shutdown);
    signal(SIGINT, graceful_shutdown);

    pthread_create(&indexing_thread, NULL, indexing_thread_func, NULL);
    start_server();
    return 0;
}
