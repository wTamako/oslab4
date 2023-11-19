#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winsock.h>
#pragma comment(lib, "wsock32.lib")

#define MAX_CLIENTS 10 // 根据实际需求设置最大客户端数量

SOCKET clientSockets[MAX_CLIENTS];
int numClients = 0; // 当前连接的客户端数量

// 定义线程参数结构体
struct ThreadParam {
	SOCKET socket_receive;// 接收消息的套接字
	SOCKADDR_IN Client_add;// 客户端地址
};

// 客户端处理线程
DWORD WINAPI ClientThread(LPVOID lpParam) {
    struct ThreadParam* param = (struct ThreadParam*)lpParam;
    SOCKET socket_receive = param->socket_receive;
    SOCKADDR_IN Client_add = param->Client_add;

    printf("客户端 (%s:%d) 已连接\n", inet_ntoa(Client_add.sin_addr), htons(Client_add.sin_port));

    char Sendbuf[100];
    char Receivebuf[100];
    int ReceiveLen;

    while (1) {
        // 接收数据
        ReceiveLen = recv(socket_receive, Receivebuf, sizeof(Receivebuf), 0);
        if (ReceiveLen <= 0) {
            printf("客户端 (%s:%d) 断开连接\n", inet_ntoa(Client_add.sin_addr), htons(Client_add.sin_port));
            break;
        }
		/*
            使用 recv 函数接收客户端发送的消息，消息存储在 Receivebuf 缓冲区中。
        如果接收到的字节数小于等于 0，表示连接断开，跳出循环，执行连接断开的清理工作。
        */

        else {
            // 使用一个字符串保存消息发生者的ip、端口号、发送的消息
            char message[100];
            sprintf_s(message, "(%s:%d): %s", inet_ntoa(Client_add.sin_addr), htons(Client_add.sin_port), Receivebuf);
            printf("%s\n", message);

            // 将消息发生者的消息发送给所有客户端
            for (int i = 0; i < numClients; i++) {
                if (clientSockets[i] != socket_receive) {
                    send(clientSockets[i], message, strlen(message) + 1, 0);
                }
            }
            /*
            使用循环遍历所有连接的客户端套接字，如果不是当前处理线程的套接字，就发送消息给其他客户端。
            send 函数用于发送消息。
            */
        }
    }

    // 关闭套接字
    closesocket(socket_receive);

    // 移除客户端套接字
    for (int i = 0; i < numClients; i++) {
        if (clientSockets[i] == socket_receive) {
            // 将最后一个元素移到当前位置，然后减少连接数
            clientSockets[i] = clientSockets[numClients - 1];
            numClients--;
            break;
        }
    }

    free(lpParam);
    return 0;
}

int main(void) {
    char Sendbuf[100];   //发送数据缓冲区
    char Receivebuf[100];//接收数据缓冲区
    int SendLen;         //发送数据的长度
    int ReceiveLen;      //接收数据的长度
    int Length;          //表示SOCKETADDR的大小

    SOCKET socket_server; //定义服务器套接字
    SOCKET socket_receive;//定义用于连接套接字

    SOCKADDR_IN Server_add; //服务器地址信息结构
    SOCKADDR_IN Client_add; //客户端地址信息结构

    WORD wVersionRequested;
    WSADATA wsaData;
    int error;

    //初始化套接字库
    wVersionRequested = MAKEWORD(2, 2);
    error = WSAStartup(wVersionRequested, &wsaData);

    if (error != 0) {
        printf("加载套接字失败！\n");
        return 0;
    }
    /*
    WSAStartup 函数用于初始化 Winsock 库，MAKEWORD(2, 2) 表示请求使用 Winsock 2.2 版本。
    如果初始化成功，WSADATA 结构中会保存相关信息。
    */

	
    //设置连接地址
    Server_add.sin_family = AF_INET;
    Server_add.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    Server_add.sin_port = htons(5000);
    /*
    Server_add 是一个 SOCKADDR_IN 结构，用于表示服务器的地址信息。
    sin_family 设置为 AF_INET 表示使用 IPv4 地址。
    sin_addr.S_un.S_addr 设置为 htonl(INADDR_ANY) 表示服务器可以接受任意地址的连接。
    sin_port 设置为 htons(5000) 表示服务器将监听端口号为 5000。
    */
	
    //创建套接字
    socket_server = socket(AF_INET, SOCK_STREAM, 0);
    /*
    socket 函数创建一个套接字，指定地址族为 IPv4 (AF_INET)，
    类型为流式套接字 (SOCK_STREAM)，协议为默认协议 (0)。
    */
	

    //绑定套接字到本地的某个地址和端口上
    if (bind(socket_server, (SOCKADDR*)&Server_add, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        printf("绑定失败\n");
    }
	/*
    bind 函数将套接字和服务器地址进行绑定，确保服务器监听指定的地址和端口。
    */

    //设置套接字为监听状态
    if (listen(socket_server, 5) < 0) {
        printf("监听失败\n");
    }
    /*
    listen 函数将套接字设置为监听状态，
    参数 5 表示最多可以同时处理 5 个连接请求。
    */


    while (1) {
        Length = sizeof(SOCKADDR);

        // 接受连接
        socket_receive = accept(socket_server, (SOCKADDR*)&Client_add, &Length);
        if (socket_receive == SOCKET_ERROR) {
            printf("接收失败\n");
        }
        /*
         accept 函数用于接受客户端的连接请求，返回一个新的套接字 socket_receive 用于与客户端通信。
         Client_add 结构中存储了客户端的地址信息。
        */
        else {
            // 创建一个新线程处理客户端消息
            struct ThreadParam* param = (struct ThreadParam*)malloc(sizeof(struct ThreadParam));
            param->socket_receive = socket_receive;
            param->Client_add = Client_add;
            /*
            创建一个 ThreadParam 结构体，用于传递给新线程的参数。
            将新接受的套接字 socket_receive 和客户端地址信息 Client_add 存储在参数结构体中。
            */
			
            if (numClients < MAX_CLIENTS) {
                clientSockets[numClients++] = socket_receive;
            }
            else {
                printf("连接数已达到上限，拒绝新连接\n");
                closesocket(socket_receive);
                free(param);
                continue;
            }
            /*
            检查当前连接的客户端数量是否已达到上限 (MAX_CLIENTS)。
            如果未达到上限，将新接受的套接字存储在 clientSockets 数组中，并增加连接数。
            如果已达到上限，拒绝新连接，关闭套接字，释放参数结构体，并继续下一次循环。
            */
			
            HANDLE hThread = CreateThread(NULL, 0, ClientThread, param, 0, NULL);
            if (hThread == NULL) {
                printf("创建线程失败\n");
                closesocket(socket_receive);
            }
            else {
                // 关闭线程句柄，防止资源泄漏
                CloseHandle(hThread);
            }
			/*
            使用 CreateThread 函数创建一个新线程，新线程将执行 ClientThread 函数。
            将参数结构体传递给新线程，使得新线程可以获取与客户端通信的套接字和客户端地址信息。
            使用 CloseHandle 函数关闭线程句柄，以防止资源泄漏。
            */
        }
    }
    // 释放套接字 关闭动态库
    closesocket(socket_receive);
    closesocket(socket_server);
    WSACleanup();

    system("pause");
    return 0;
}
