#pragma once
#include <vector>
//#include "TFHandler.hpp"
#include "PTHHandler.hpp"
#include "Node.hpp"

#define	MAX_NUM_TRANSAC_HIST	10
#define FORMAT_VERSION			200

typedef struct _HISTORY_DATA {
	DISKCOLORS diskcolor;
	DISKCOLORS board[64];
	double probability[DN_OUTPUT_SIZE];
	float value;
} HistoryData;

typedef struct _HISTORY_DATA_LIST {
	GameId gameId;
	DISKCOLORS diskcolor;
	std::vector<HistoryData> historyData;
} HistoryDataList;

class History {
public:
	int init();
	int add(GameId _gameId, DISKCOLORS _diskcolor, DISKCOLORS *_board, std::vector<Score> scores);
	int finish(GameId _gameId, DISKCOLORS _diskcolor, float _value);
private:
	int outputFile(GameId _gameId, DISKCOLORS diskcolor);
	std::vector<HistoryDataList> historyDataList;
	GameId gameId;
	bool isGameIdValid = false;
	// ★要追加 トランザクションID
};

