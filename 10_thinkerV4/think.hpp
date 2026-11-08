#pragma once
#include "externalThinkerMessages.hpp"
#include "main.hpp"
//#include "TFHandler.hpp"
#include "PTHHandler.hpp"

#define MODELINFO	"(Unknown Model)"

#define SP_TEMPERATURE 1.0
//#define SP_TEMPERATURE 4.0
// #define PV_EVALUATE_COUNT 100
// #define PV_EVALUATE_COUNT 200
// #define PV_EVALUATE_COUNT 400
#define PV_EVALUATE_COUNT 2000

#define	DEFAULT_NUM_THREADS		8
#define DEFAULT_SEARCH_DEPTH	6

enum class THINKARC{
	THINKARC_DEEP = 0,
	THINKARC_MINMAX,
	THINKARC_MINMAX_MP
};

typedef int Action;

typedef struct _POLICY {
	int x;
	int y;
	float policy;
} Policy;

typedef struct _ThinkerInitParam {
	RunningMode runningMode;
	double spTemperature;
	int numIterations = PV_EVALUATE_COUNT;
	bool isBreadthFirst = false;
	bool limitTemperaturePeriod = false;
	int gpuid = -1;
	bool forceGPU = true;
	THINKARC thinkArc = THINKARC::THINKARC_DEEP;
	int numThreads = DEFAULT_NUM_THREADS;
	int depth = DEFAULT_SEARCH_DEPTH;
} ThinkerInitParam;

class Thinker {
public:
	int init(ThinkerInitParam *thinkerInitParam);
	int think(int turn, DISKCOLORS* board, int *place, GameId gameId);
	char* getModelInfo();
	~Thinker();

private:
	bool isInitialized = false;
	Model model;
	char modelInfo[1024];
	double spTemperature = SP_TEMPERATURE;
	int numIterations = PV_EVALUATE_COUNT;
	bool isBreadthFirst = false;
	RunningMode runningMode;
	bool limitTemperaturePeriod = false;
	int gpuid = -1;		// -1: not specify, 0-: gpuid
	THINKARC thinkArc;
	int numThreads = 8;
	int depth = 6;

	int CountDisk(DISKCOLORS color, DISKCOLORS _board[64]);
};