//
//  main.cpp
//  External thinker for othello version 2.00. 
//  

#include <iostream>
#ifdef _WIN32
#include <WinSock2.h>		// Need to include before including "Windows.h", because it seems to include older version "winsock.h"
#endif
#include "common.h"
#include "externalThinkerMessages.hpp"
#include "main.hpp"
#include "think.hpp"
#include "messageGenerator.hpp"
#include "messageParser.hpp"
//#include "TFHandler.hpp"
#include "PTHHandler.hpp"
#include "history.hpp"

#pragma warning(disable:4996 6031 6305)

// Global
Logging logging;
History history;
Thinker thinker;

//
//	Function Name: main
//	Summary: Check commandline parameters, prepare the socket, wait for message and then handle the received message.
//
int main(int argc, char **argv)
{
#ifdef _WIN32
    WSAData wsaData;
    int sockaddr_in_size = sizeof(struct sockaddr_in);
#else
    socklen_t sockaddr_in_size = sizeof(struct sockaddr_in);
#endif
    SOCKET sock;
    struct sockaddr_in addr;
    struct sockaddr_in from;
    int port = 60001;
    char buf[4096];
    int messageLen;
    int ret;
    double spTemperature = SP_TEMPERATURE;
    bool limitTemperaturePeriod = false;
    int numIterations = PV_EVALUATE_COUNT;
    bool isBreadthFirst = false;
    RunningMode runningMode = RunningMode::RUNNINGMODE_AUTO;
    int gpuid = -1;
    bool forceGPU = true;
    THINKARC thinkArc = THINKARC::THINKARC_DEEP;
    int numThreads = DEFAULT_NUM_THREADS;
    int depth = DEFAULT_SEARCH_DEPTH;

    // Logging
#ifdef _DEBUG
    LOGOUT_INIT(LOGLEVEL, "thinkerV3_log.txt");
#else
    LOGOUT_INIT(LOGLEVEL_WARNING, "thinkerV3_log.txt");
#endif

    // Parameter check
    try {
        if (argc >= 1) {
            for (int i = 1; i < argc; i++) {
                if (argv[i][0] == '-') {
                    switch (argv[i][1]) {
                    case 'p':
                        port = atoi(&argv[i][2]);
                        if (port <= 1024) {
                            throw - 1;
                        }
                        break;
                    case 'T':
                        spTemperature = atof(&argv[i][2]);
                        if (spTemperature < 0.0) throw - 2;
                        limitTemperaturePeriod = false;
                        break;
                    case 't':
                        spTemperature = atof(&argv[i][2]);
                        if (spTemperature < 0.0) throw - 2;
                        limitTemperaturePeriod = true;
                        break;
                    case 'i':
                        numIterations = atoi(&argv[i][2]);
                        if (numIterations <= 0) {
                            throw - 4;
                        }
                        break;
                    case 'b':
                        isBreadthFirst = true;
                        break;
                    case 'G':
                        runningMode = RunningMode::RUNNINGMODE_GPU;
                        if ('0' <= argv[i][2] && argv[i][2] <= '9') {
                            gpuid = atoi(&argv[i][2]);
                            if (gpuid < 0) throw - 5;
                        }
                        forceGPU = true;
                        break;
                    case 'g':
                        runningMode = RunningMode::RUNNINGMODE_GPU;
                        if ('0' <= argv[i][2] && argv[i][2] <= '9') {
                            gpuid = atoi(&argv[i][2]);
                            if (gpuid < 0) throw - 5;
                        }
                        forceGPU = false;
                        break;
                    case 'C':
                        runningMode = RunningMode::RUNNINGMODE_CPU;
                        break;
                    case 'M':
                        if (argv[i][2] == 'P') {
                            thinkArc = THINKARC::THINKARC_MINMAX_MP;
                            numThreads = atoi(&argv[i][3]);

                            if (numThreads == 0) numThreads = DEFAULT_NUM_THREADS;    // Set default
                        }
                        else {
                            thinkArc = THINKARC::THINKARC_MINMAX;
                        }
                        break;
                    case 'd':
                        depth = atoi(&argv[i][2]);
                        if (depth <= 0) {
                            depth = 6;
                        }
                        break;
                    case 'h':
                        throw - 6;
                        break;
                    }
                }
                else {
                    throw - 3;
                }
            }
        }
    }
    catch(int ret){
        printf("\n\nUsage: thinkerV3.exe [options]\n\n");
        printf(" options([]:mandatory parameter, ():Optional Parameter)\n\n");
        printf("  -p[port number]        : Port number to listen. ""port number"" must be larger than or equal to 1024.\n");
        printf("  -t[temperature]        : Temperature (>= 0.0).\n");
        printf("  -b                     : Breadth First search.\n");
        printf("  -C                     : Force to use CPU.\n");
        printf("  -G(GPU)                : Force to use GPU of GPUID=(GPU).\n");
        printf("  -g(GPU)                : Try to use GPU of GPUID=(GPU). If impossible, use CPU.\n");
        printf("  -M                     : Use Min-Max based thinker.\n");
        printf("  -MP(Number of threads) : Use Min-Max based with OpenMP thinker.\n");
        printf("  -d[depth]              : Search depth for Min-Max based thinker.\n");
        printf("\n\n");

        return ret;
    }

    printf("\n");
    printf("%s\n", TEXTINFO);
    printf("Temperature = %f(%s)\n", spTemperature, limitTemperaturePeriod == true ? "Only the first 10 moves; the rest are 0.0": "all moves");
    printf("numIterations = %d\n", numIterations);
    printf("isBreadthFirst = %s\n", isBreadthFirst ? "true" : "false");
    switch (runningMode) {
    case RunningMode::RUNNINGMODE_AUTO:
        printf("Running Mode = AUTO\n");
        break;
    case RunningMode::RUNNINGMODE_CPU:
        printf("Running Mode = CPU\n");
        break;
    case RunningMode::RUNNINGMODE_GPU:
        printf("Running Mode = GPU\n");
        gpuid < 0 ? printf("gpuid = (Not specify)\n") : printf("gpuid = %d\n", gpuid);
        printf("Force to use specified GPU = %s\n", forceGPU ? "true" : "false");
        break;
    }
    printf("Thinker Architecture = %s\n", thinkArc == THINKARC::THINKARC_DEEP ? "Deep Learning based" : thinkArc == THINKARC::THINKARC_MINMAX ? "Min-Max Based" : "Min-Max Based(Multi Processor)");
    if(thinkArc == THINKARC::THINKARC_MINMAX_MP) printf("Number of threads = %d\n", numThreads);
    if (thinkArc == THINKARC::THINKARC_MINMAX || thinkArc == THINKARC::THINKARC_MINMAX_MP) printf("Search Depth = %d\n", depth);
    printf("\n");

    logging.logout("***** Command Paramters ******");
    logging.logout("Port Number = %d", port);
    logging.logout("Temperature = %f(%s)", spTemperature, limitTemperaturePeriod == true ? "Only the first 10 moves; the rest are 0.0" : "all moves");
    logging.logout("numIterations = %d", numIterations);
    logging.logout("isBreadthFirst = %s", isBreadthFirst ? "true" : "false");
    switch (runningMode) {
    case RunningMode::RUNNINGMODE_AUTO:
        logging.logout("Running Mode = AUTO");
        break;
    case RunningMode::RUNNINGMODE_CPU:
        logging.logout("Running Mode = CPU");
        break;
    case RunningMode::RUNNINGMODE_GPU:
        logging.logout("Running Mode = GPU");
        gpuid < 0 ? logging.logout("gpuid = (Not specify)") : logging.logout("gpuid = %d", gpuid);
        logging.logout("Force to use specified GPU = %s", forceGPU ? "true" : "false");
        break;
    }
    logging.logout("Thinker Architecture = %s", thinkArc == THINKARC::THINKARC_DEEP ? "Deep Learning based" : thinkArc == THINKARC::THINKARC_MINMAX ? "Min-Max Based" : "Min-Max Based(Multi Processor)");
    if (thinkArc == THINKARC::THINKARC_MINMAX_MP) logging.logout("Number of threads = %d", numThreads);
    if (thinkArc == THINKARC::THINKARC_MINMAX || thinkArc == THINKARC::THINKARC_MINMAX_MP) logging.logout("Search Depth = %d", depth);
    logging.logout("******************************");

    // 乱数の初期化
    srand((unsigned)time(0));

    // Thinkerの初期化
    ThinkerInitParam thinkerInitParam;
    thinkerInitParam.runningMode = runningMode;
    thinkerInitParam.spTemperature = spTemperature;
    thinkerInitParam.numIterations = numIterations;
    thinkerInitParam.isBreadthFirst = isBreadthFirst;
    thinkerInitParam.limitTemperaturePeriod = limitTemperaturePeriod;
    thinkerInitParam.gpuid = gpuid;
    thinkerInitParam.forceGPU = forceGPU;
    thinkerInitParam.thinkArc = thinkArc;
    thinkerInitParam.numThreads = numThreads;
    thinkerInitParam.depth = depth;

    ret = thinker.init(&thinkerInitParam);
    if (ret < 0) {
        return -4;
    }
    else {
        printf("Model = %s\n", thinker.getModelInfo());
        logging.logout("Model = %s", thinker.getModelInfo());
    }

    // Historyの初期化
    history.init();

    // Initialize winsock
#ifdef _WIN32
    WSAStartup(MAKEWORD(2, 0), &wsaData);
#endif

	sock = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#ifdef _WIN32
    addr.sin_addr.S_un.S_addr = INADDR_ANY;
#else
    addr.sin_addr.s_addr = INADDR_ANY;
#endif

    bind(sock, (struct sockaddr*) & addr, sizeof(addr));

    // Start to wait receiving requests
    printf("\nWaiting requests at port = %d (listening on 0.0.0.0)...\n\n", port);

    // Receive and handle messages until QUIT message is received
    for (;;) {
        // 
        fd_set readfds;

        memset(buf, 0, sizeof(buf));

        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
#ifdef _WIN32
        ret = select(1, &readfds, NULL, NULL, NULL);
#else
        ret = select(sock + 1, &readfds, NULL, NULL, NULL);
#endif

        if (FD_ISSET(sock, &readfds)) {
            messageLen = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &sockaddr_in_size);

            // Message length check
            if (messageLen < sizeof(MESSAGEHEADER)) {
                printf("UDP Received (Too Small): %d bytes\n", messageLen);
                logging.logout("UDP Received (Too Small): %d bytes", messageLen);
                continue;
            }
            
            printf("UDP Received: %d bytes\n", messageLen);
            logging.logout("UDP Received: %d bytes", messageLen);

            // Parse the received message
            MessageParser messageParser;
            int parse_ret = messageParser.SetParam(buf, messageLen);
            if (parse_ret != 0) {
                printf("Message Parsing Failed: code=%d\n", parse_ret);
                logging.logout("Message Parsing Failed: code=%d", parse_ret);
                continue;
            }

            // Get the message type
            MESSAGETYPE messageType;
            if (messageParser.getMessageType(&messageType) != 0) continue;

            // Go out this loop if QUIT message is received
            if (messageType == MESSAGETYPE::QUIT) break;

            // Handle the received message according to the message type
            switch (messageType) {
            case MESSAGETYPE::INFORMATION_REQUEST:  // Information Request
                printf("Information Request Received.\n");
                HandleInformationRequest(messageParser, sock, from, sockaddr_in_size);
                break;
            case MESSAGETYPE::THINK_REQUEST:        // Think Request
                HandleThinkRequest(messageParser, sock, from, sockaddr_in_size);
                break;
            case MESSAGETYPE::THINK_STOP_REQUEST:   // Think Stop Request
                // Currently don't support this message.
                break;
            case MESSAGETYPE::GAME_FINISHED:        // Game Finished
                printf("Game Finished Received.\n");
                HandleGameFinished(messageParser, sock, from, sockaddr_in_size);
                break;
            default:
                break;
            }

            // Write to log
            LOGOUT_FLUSH();

            // Release the message
            messageParser.free();
        }
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    // Logging
    LOGOUT_END();

    return 0;
}

//
//	Function Name: HandleInformationRequest
//	Summary: Handle Information Request message
//	
//	In:
//		messageParser       MessageParser instance of the received Information Request message
//      sock                socket to send the response message
//      from                The parameter to set the 5th parameter in sendto function to transmit the response message.  
//      sockaddr_in_size    The parameter to set the 6th parameter in sendto function to transmit the response message.
//
//	Return:
//      None
//
void HandleInformationRequest(MessageParser messageParser, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size)
{
    char respMessage[MAX_MESSAGE_LENGTH];

    // Generate Information Response message
    int messageLength;
    MessageGenerator messageGenerator;
    messageGenerator.SetParams(respMessage, MAX_MESSAGE_LENGTH);
    messageGenerator.makeMessageHeader(MESSAGETYPE::INFORMATION_RESPONSE);
    messageGenerator.addTLVVersion(VERSION);

    char textInfo[1024];
    snprintf(textInfo, sizeof(textInfo), "%s(%s)", TEXTINFO, thinker.getModelInfo());
    messageGenerator.addTLVTextInfo(textInfo);

    // Check if building the message finished successfully
    if ((messageLength = messageGenerator.getSize()) < 0) return;

    // Send INFORMATION_RESPONSE to the peer
    sendto(sock, respMessage, messageLength, 0, (struct sockaddr*)&from, sockaddr_in_size);

    printf("Information Response sent.\n");
}

//
//	Function Name: HandleThinkRequest
//	Summary: Handle Think Request message
//	
//	In:
//		messageParser       MessageParser instance of the received Information Request message
//      sock                socket to send the response message
//      from                The parameter to set the 5th parameter in sendto function to transmit the response message.  
//      sockaddr_in_size    The parameter to set the 6th parameter in sendto function to transmit the response message.
//
//	Return:
//      None
//
void HandleThinkRequest(MessageParser messageParser, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size)
{
    int tlvHead = sizeof(MESSAGEHEADER);       // tlvHead: TLV head pos to be processed
    DISKCOLORS board[64];
    int turn = 0;
    unsigned int id = 0;
    static GameId gameId;
    static bool prevGameIdAvail = false;
    int ret;

    // Get data from received message
    if (messageParser.getTLVParamsBoard(board) != 0) {
        printf("Error: Missing or invalid Board TLV\n");
        logging.logout("Error: Missing or invalid Board TLV");
        return;
    }
    if (messageParser.getTLVParamsTurn(&turn) != 0) {
        printf("Error: Missing or invalid Turn TLV\n");
        logging.logout("Error: Missing or invalid Turn TLV");
        return;
    }
    if (messageParser.getTLVParamsID(&id) != 0) {
        printf("Error: Missing or invalid ID TLV\n");
        logging.logout("Error: Missing or invalid ID TLV");
        return;
    }
    if (messageParser.getTLVParamsGameId(&gameId)) {
        printf("Error: Missing or invalid GameId TLV\n");
        logging.logout("Error: Missing or invalid GameId TLV");
        return;
    }

    printf("Think Request Received. ID = %d.\n", id);
    
    // Print the received board for debugging
    printf("Received Board:\n");
    for (int y = 0; y < 8; y++) {
        printf("  ");
        for (int x = 0; x < 8; x++) {
            DISKCOLORS c = board[x * 8 + y];
            if (c == DISKCOLORS::COLOR_BLACK) printf("X ");
            else if (c == DISKCOLORS::COLOR_WHITE) printf("O ");
            else printf(". ");
        }
        printf("\n");
    }
    printf("Turn: %d\n", turn);

    // Send Think Accept message
    ret = SendThinkAccept(id, sock, from, sockaddr_in_size);

    printf("Think Accept sent. ID = %d. ", id);

    // Think
    int place;
    ret = thinker.think(turn, board, &place, gameId);

        // printf("  [DEBUG] Thinker sending place=%d (x=%d, y=%d)\n", place, place/10, place%10);
        fflush(stdout);
    if (ret == 0) {
        // Send Think Response message
        ret = SendThinkResponse(id, (unsigned char)place / 10, (unsigned char)place % 10, sock, from, sockaddr_in_size);

        printf("Think Response sent. ID = %d.\n", id);
    }
    else {
        ret = SendThinkReject(id, sock, from, sockaddr_in_size);

        printf("Think Reject sent. ID = %d.\n", id);
    }
}

//
//	Function Name: HandleGameFinished
//	Summary: Handle Game Finished message
//	
//	In:
//		messageParser       MessageParser instance of the received Information Request message
//      sock                socket to send the response message
//      from                The parameter to set the 5th parameter in sendto function to transmit the response message.  
//      sockaddr_in_size    The parameter to set the 6th parameter in sendto function to transmit the response message.
//
//	Return:
//      None
//
void HandleGameFinished(MessageParser messageParser, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size)
{
    GameId gameId;
    RESULT result;
    DISKCOLORS diskcolor;
    int ret;

    // Get data from received message
    if (messageParser.getTLVParamsGameId(&gameId)) return;          // Get the game Id
    if (messageParser.getTLVParamsResult(&result)) return;          // Get the winner
    if (messageParser.getTLVParamsDiskColor(&diskcolor)) return;          // Get the winner

    // value値をセット & 学習データのファイル出力を行う
    // resultが自身なら1.0、相手なら-1.0, 引き分けなら0.0
    switch (result) {
    case RESULT::WIN :
        ret = history.finish(gameId, diskcolor, 1.0);
        break;
    case RESULT::LOSE :
        ret = history.finish(gameId, diskcolor, -1.0);
        break;
    case RESULT::EVEN :
        ret = history.finish(gameId, diskcolor, 0.0);
        break;
    default:
        break;
    }
    return;
}

//
//	Function Name: SendThinkAccept
//	Summary: Send Think Accept message
//	
//	In:
//		id                  Transaction ID received in Think Request message
//      sock                socket to send the response message
//      from                The parameter to set the 5th parameter in sendto function to transmit the response message.  
//      sockaddr_in_size    The parameter to set the 6th parameter in sendto function to transmit the response message.
//
//	Return:
//      0                   Succeed
//      -1                  Failed to build Think Accept message
//
int SendThinkAccept(int id, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size)
{
    char message[MAX_MESSAGE_LENGTH];
    MessageGenerator messageGenerator;

    // Build Think Accpest message
    messageGenerator.SetParams(message, sizeof(message));
    messageGenerator.makeMessageHeader(MESSAGETYPE::THINK_ACCEPT);
    messageGenerator.addTLVID(id);

    // Check if building the message has succeeded or not
    if (messageGenerator.getSize() <= 0) return -1;

    // Send the message
    sendto(sock, message, messageGenerator.getSize(), 0, (struct sockaddr*)&from, sockaddr_in_size);

    return 0;
}

//
//	Function Name: SendThinkResponse
//	Summary: Send Think Response message
//	
//	In:
//		id                  Transaction ID received in Think Request message
//      x                   The place to put the disk in x-axis
//      y                   The place to put the disk in y-axis
//      sock                socket to send the response message
//      from                The parameter to set the 5th parameter in sendto function to transmit the response message.  
//      sockaddr_in_size    The parameter to set the 6th parameter in sendto function to transmit the response message.
//
//	Return:
//      0                   Succeed
//      -1                  Failed to build Think Response message
//
int SendThinkResponse(int id, unsigned char x, unsigned char y, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size)
{
    char message[MAX_MESSAGE_LENGTH];
    MessageGenerator messageGenerator;

    // Build Think Response message
    messageGenerator.SetParams(message, sizeof(message));
    messageGenerator.makeMessageHeader(MESSAGETYPE::THINK_RESPONSE);
    messageGenerator.addTLVID(id);
    messageGenerator.addTLVPlace(x, y);

    // Check if building the message has succeeded or not
    if (messageGenerator.getSize() <= 0) return -1;

    // Send message
    sendto(sock, message, messageGenerator.getSize(), 0, (struct sockaddr*)&from, sockaddr_in_size);

    return 0;
}

//
//	Function Name: SendThinkReject
//	Summary: Send Think Reject message
//	
//	In:
//		id                  Transaction ID received in Think Request message
//      sock                socket to send the response message
//      from                The parameter to set the 5th parameter in sendto function to transmit the response message.  
//      sockaddr_in_size    The parameter to set the 6th parameter in sendto function to transmit the response message.
//
//	Return:
//      0                   Succeed
//      -1                  Failed to build Think Accept message
//
int SendThinkReject(int id, SOCKET sock, struct sockaddr_in from, int sockaddr_in_size)
{
    char message[MAX_MESSAGE_LENGTH];
    MessageGenerator messageGenerator;

    // Build Think Response message
    messageGenerator.SetParams(message, sizeof(message));
    messageGenerator.makeMessageHeader(MESSAGETYPE::THINK_REJECT);
    messageGenerator.addTLVID(id);

    // Check if building the message has succeeded or not
    if (messageGenerator.getSize() <= 0) return -1;

    // Send message
    sendto(sock, message, messageGenerator.getSize(), 0, (struct sockaddr*)&from, sockaddr_in_size);

    return 0;
}