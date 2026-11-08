#pragma once
#include <iostream>
#ifdef _WIN32
#include <WinSock2.h>		// Need to include before including "Windows.h", because it seems to include older version "winsock.h"
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <memory.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <string.h>
#include <cmath>
#include <float.h>

#include "externalThinkerMessages.hpp"
#include "main.hpp"
#include "think.hpp"
#include "messageGenerator.hpp"
#include "messageParser.hpp"
#include "TFHandler.hpp"
#include "State.hpp"
#include "Pv_mcts.action.hpp"
#include "Node.hpp"
#include "tensorflow\c\c_api.h"
