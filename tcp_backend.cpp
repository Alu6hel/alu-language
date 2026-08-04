#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "alu_runtime.h" // Our standard string definitions

#pragma comment(lib, "Ws2_32.lib")

extern "C" {

    int32_t ws_init() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            std::cerr << "WSAStartup failed: " << result << "\n";
            return -1;
        }
        return 0;
    }

    int32_t ws_bind(int32_t port) {
        SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed.\n";
            return -1;
        }

        sockaddr_in service;
        service.sin_family = AF_INET;
        service.sin_addr.s_addr = INADDR_ANY;
        service.sin_port = htons(port);

        int result = bind(listenSocket, (SOCKADDR*)&service, sizeof(service));
        if (result == SOCKET_ERROR) {
            std::cerr << "Bind failed.\n";
            closesocket(listenSocket);
            return -1;
        }

        std::cout << "[Alu Engine] Successfully bound to port " << port << " natively.\n";
        return (int32_t)listenSocket;
    }

    int32_t ws_listen(int32_t socket_fd) {
        int result = listen((SOCKET)socket_fd, SOMAXCONN);
        if (result == SOCKET_ERROR) {
            std::cerr << "Listen failed.\n";
            return -1;
        }
        return 0;
    }

    int32_t ws_accept(int32_t socket_fd) {
        SOCKET clientSocket = accept((SOCKET)socket_fd, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed.\n";
            return -1;
        }
        return (int32_t)clientSocket;
    }

    int32_t ws_send(int32_t client_fd, AluString* data) {
        // Z3 guarantees data is not null via Alu ARC
        const char* buffer = data->data;
        int len = data->length;

        int result = send((SOCKET)client_fd, buffer, len, 0);
        if (result == SOCKET_ERROR) {
            std::cerr << "Send failed.\n";
            return -1;
        }
        return result;
    }

    AluString* ws_read(int32_t client_fd) {
        char recvbuf[4096];
        int res = recv((SOCKET)client_fd, recvbuf, 4096, 0);
        
        if (res > 0) {
            // Allocate a new AluString using the Alu native ARC allocator
            AluString* str = (AluString*)alu_alloc(sizeof(AluString));
            str->ref_count = 1;
            str->length = res;
            str->data = (char*)alu_alloc(res + 1);
            memcpy(str->data, recvbuf, res);
            str->data[res] = '\0';
            return str;
        } else {
            // Return empty AluString
            AluString* str = (AluString*)alu_alloc(sizeof(AluString));
            str->ref_count = 1;
            str->length = 0;
            str->data = (char*)alu_alloc(1);
            str->data[0] = '\0';
            return str;
        }
    }

    void ws_close(int32_t fd) {
        closesocket((SOCKET)fd);
    }
}
