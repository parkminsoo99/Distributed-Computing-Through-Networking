#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>

#define PORT 1255     // 서버가 사용할 포트 번호 정의
#define BUF_SIZE 1024 // 버퍼의 크기 정의
#define MAX_CLIENTS 2 // 최대 클라이언트 수 정의

int main()
{
    int server_sock, client_sock[MAX_CLIENTS];   // 서버와 클라이언트 소켓 정의
    struct sockaddr_in server_addr, client_addr; // 서버와 클라이언트 주소 정보를 담을 구조체 정의
    socklen_t client_addr_size;                  // 클라이언트 주소의 크기 저장 변수 정의

    char message[BUF_SIZE]; // 메시지를 저장할 문자 배열 정의
    int str_len, i;
    struct timeval start, end;                                // 시작 시간과 종료 시간을 저장할 구조체 정의
    long start_nonce_values[MAX_CLIENTS] = {0, LONG_MAX / 2}; // 시작 Nonce 값 배열 정의

    server_sock = socket(PF_INET, SOCK_STREAM, 0); // 서버 소켓 생성
    if (server_sock == -1)
        printf("socket() error\n"); // 소켓 생성 실패시 오류 메시지 출력

    memset(&server_addr, 0, sizeof(server_addr));    // server_addr 구조체 메모리 초기화
    server_addr.sin_family = AF_INET;                // 주소 체계 설정
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 모든 IP 허용
    server_addr.sin_port = htons(PORT);              // 포트 설정

    // 소켓에 주소 할당. 실패시 오류 메시지 출력
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        printf("bind() error\n");
    // 소켓으로 연결 요청을 수신할 수 있도록 설정. 실패시 오류 메시지 출력
    if (listen(server_sock, MAX_CLIENTS) == -1)
        printf("listen() error\n");

    client_addr_size = sizeof(client_addr);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        // 클라이언트의 연결 요청을 수락. 실패시 오류 메시지 출력
        client_sock[i] = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_sock[i] == -1)
            printf("accept() error\n");
        else
            printf("Connected client %d\n", i); // 연결 성공시 메시지 출력
    }

    char challenge[BUF_SIZE];
    int difficulty;
    printf("Enter Challenge: "); // 사용자로부터 Challenge 값을 입력 받음
    scanf("%s", challenge);

    printf("Enter Difficulty: "); // 사용자로부터 Difficulty 값을 입력 받음
    scanf("%d", &difficulty);

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        // Challenge, Difficulty, start_nonce를 문자열로 변환하여 메시지를 구성
        sprintf(message, "%s %d %ld", challenge, difficulty, start_nonce_values[i]);

        // 메시지를 Working Server에게 전송
        gettimeofday(&start, NULL);
        write(client_sock[i], message, sizeof(message));
    }

    // 파일 디스크립터 집합을 초기화
    fd_set read_fds;
    FD_ZERO(&read_fds);
    int max_sock = -1;

    // 연결된 모든 클라이언트에 대해 파일 디스크립터를 설정하고 가장 큰 값을 찾음
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        FD_SET(client_sock[i], &read_fds);
        if (client_sock[i] > max_sock)
        {
            max_sock = client_sock[i];
        }
    }

    // 파일 디스크립터 집합에서 읽기를 위한 변경사항을 확인. 실패시 오류 메시지 출력하고 프로그램 종료
    if (select(max_sock + 1, &read_fds, NULL, NULL, NULL) == -1)
    {
        perror("select() error");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        // 클라이언트가 데이터를 보냈는지 확인
        if (FD_ISSET(client_sock[i], &read_fds))
        {
            // Working Server로부터 PoW 결과를 수신
            str_len = read(client_sock[i], message, BUF_SIZE - 1);
            gettimeofday(&end, NULL);
            message[str_len] = 0;
            printf("Message from client %d: %s\n", i, message); // 결과 출력

            double time_elapsed = ((double)end.tv_sec - start.tv_sec) + ((double)end.tv_usec - start.tv_usec) * 1e-6;
            printf("Time for PoW: %f seconds\n", time_elapsed); // 소요 시간 출력

            // 다른 클라이언트에게 중지 신호를 전송
            for (int j = 0; j < MAX_CLIENTS; j++)
            {
                if (j != i)
                {
                    write(client_sock[j], "stop", sizeof("stop"));
                }
            }

            // 이 클라이언트의 소켓을 닫음
            close(client_sock[i]);
            break;
        }
    }

    // 남아 있는 열린 소켓들을 닫음
    for (i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sock[i] != -1)
        {
            close(client_sock[i]);
        }
    }

    close(server_sock); // 서버 소켓 닫음

    return 0;
}
