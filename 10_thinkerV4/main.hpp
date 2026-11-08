#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

#include "messageParser.hpp"
//#include "TFHandler.hpp"


#define	VERSION		320
#define	TEXTINFO	"Othello Thinker V3.20(Alpha Go Based)"

void HandleInformationRequest(MessageParser messageParser, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size);
void HandleThinkRequest(MessageParser messageParser, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size);
void HandleGameFinished(MessageParser messageParser, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size);
int SendThinkAccept(int id, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size);
int SendThinkResponse(int id, unsigned char x, unsigned char y, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size);
int SendThinkReject(int id, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size);