#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/sha.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>

// #define SERVER_IP "127.0.0.1"
// #define PORT 8080
#define SERVER_IP "119.192.210.203"
#define PORT 1255
#define BUF_SIZE 1024
#define NUM_THREADS 5          // 작업을 수행할 스레드의 수
#define RANGE_PER_THREAD 20000 // 한 번에 각 스레드가 담당할 nonce 값 범위
#define MAX_NONCE LONG_MAX     // 최대 nonce 값

char challenge[BUF_SIZE];                                // 메인 서버로부터 받는 challenge 문자열
int difficulty;                                          // PoW의 난이도
long found_nonce = -1;                                   // 유효한 해시를 찾았을 경우 nonce 값 저장 (-1은 찾지 못했음을 의미)
long found_thread_id = -1;                               // 유효한 해시를 찾은 스레드의 ID 저장 (-1은 찾지 못했음을 의미)
long start_nonce;                                        // 메인 서버로부터 받는 시작 nonce 값
pthread_mutex_t found_mutex = PTHREAD_MUTEX_INITIALIZER; // found_nonce와 found_thread_id 변수를 보호하기 위한 뮤텍스
bool stop_received = false;                              // 메인 서버로부터 stop 메시지를 받았는지 여부를 표시하는 플래그

// 주어진 데이터의 SHA-256 해시를 계산하는 함수
void compute_SHA256(unsigned char *hash, unsigned char *data, size_t length)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data, length);
    SHA256_Final(hash, &sha256);
}

// 주어진 위치에서 바이트의 첫 4비트가 모두 0인지 확인하는 함수
bool check_4bits(unsigned char byte, int position)
{
    return (byte & (0xF << position)) == 0;
}

// 주어진 해시가 난이도 요구사항을 충족하는지 확인하는 함수
int is_valid(unsigned char *hash, int difficulty)
{
    int full_bytes = difficulty / 2;
    int extra_bits = difficulty % 2;

    for (int i = 0; i < full_bytes; i++)
    {
        if (hash[i] != 0)
        {
            return 0;
        }
    }

    if (extra_bits > 0 && !check_4bits(hash[full_bytes], 4))
    {
        return 0;
    }

    return 1;
}

// 주어진 해시를 출력하는 함수
void print_hash(unsigned char *hash)
{
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

// 각 스레드가 PoW를 수행하는 작업자 함수
void *pow_worker(void *threadid)
{
    long tid = (long)threadid;
    unsigned char text[64];
    unsigned char hash[SHA256_DIGEST_LENGTH];

    for (long nonce = start_nonce + tid * RANGE_PER_THREAD; nonce < MAX_NONCE; nonce += NUM_THREADS * RANGE_PER_THREAD)
    {
        for (long my_nonce = nonce; my_nonce < nonce + RANGE_PER_THREAD; my_nonce++)
        {
            pthread_mutex_lock(&found_mutex);
            if (stop_received || found_nonce != -1)
            {
                pthread_mutex_unlock(&found_mutex);
                pthread_exit(NULL);
            }
            pthread_mutex_unlock(&found_mutex);

            sprintf(text, "%s%ld", challenge, my_nonce);
            compute_SHA256(hash, text, strlen(text));
            if (is_valid(hash, difficulty))
            {
                print_hash(hash);
                pthread_mutex_lock(&found_mutex);
                if (found_nonce == -1)
                { // double check inside the lock
                    found_nonce = my_nonce;
                    found_thread_id = tid; // Store the thread ID
                }
                pthread_mutex_unlock(&found_mutex);
                pthread_exit(NULL);
            }
        }
    }

    pthread_exit(NULL);
}

// 메인 서버로부터의 stop 메시지를 듣는 스레드 함수
void *message_listener(void *arg)
{
    int sock = *((int *)arg);
    char message[BUF_SIZE];
    while (1)
    {
        if (read(sock, message, BUF_SIZE - 1) > 0)
        {
            if (strcmp(message, "stop") == 0)
            {
                pthread_mutex_lock(&found_mutex);
                stop_received = true;
                printf("Stopped computation due to stop message\n");
                pthread_mutex_unlock(&found_mutex);
                break;
            }
        }
    }
    pthread_exit(NULL);
}

// 메인 함수
int main()
{
    int sock;
    struct sockaddr_in server_addr;
    char message[BUF_SIZE];
    int str_len;
    // 소켓 생성 후 메인 서버에 연결
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        printf("socket() error\n");
        exit(0);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        printf("connect() error\n");
        exit(0);
    }
    else
        printf("Connected to main server\n");

    // 메인 서버로부터 challenge, difficulty, start_nonce 읽기
    str_len = read(sock, message, BUF_SIZE - 1);
    message[str_len] = 0;
    sscanf(message, "%s %d %ld", challenge, &difficulty, &start_nonce);
    printf("challenge: %s, difficulty: %d, start_nonce: %ld\n", challenge, difficulty, start_nonce);

    // PoW를 수행할 작업자 스레드와 stop 메시지를 듣는 리스너 스레드 생성
    pthread_t threads[NUM_THREADS + 1]; // extra thread for message_listener

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&threads[i], NULL, pow_worker, (void *)(long)i);
    }

    pthread_create(&threads[NUM_THREADS], NULL, message_listener, (void *)&sock);

    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    pthread_cancel(threads[NUM_THREADS]); // cancel message_listener thread

    if ((!stop_received) && (found_nonce != -1))
    {
        sprintf(message, "Success! Nonce: %ld\n", found_nonce);
        printf("Thread #%ld found a valid nonce: %ld\n", found_thread_id, found_nonce);
        write(sock, message, sizeof(message));
    }
    else
    {
        printf("Fail....\n");
    }

    close(sock);

    return 0;
}