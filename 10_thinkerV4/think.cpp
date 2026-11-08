// Othello Thinker Version 3.00
// Copyright (C) 2023 T.Sashihara
// This program goes with Othello for Windows Ver 3.00
#include <stdio.h>
#include <memory.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include "main.hpp"
#include "think.hpp"
//#include "TFHandler.hpp"
#include "PTHHandler.hpp"
#include "State.hpp"
#include "Pv_mcts.action.hpp"
#include "thinkV1.hpp"
#include "history.hpp"

extern History history;
extern Logging logging;

int Thinker::init(ThinkerInitParam *thinkerInitParam)
{
	int ret;

	// RunningModeのセット
	runningMode = thinkerInitParam->runningMode;
	gpuid = thinkerInitParam->gpuid;

	// メイン部分相当をここに実装
	// モデルのロード
	ret = load_model(&model, runningMode, gpuid);

	if (ret < 0) {
		if (thinkerInitParam->forceGPU == false && runningMode == RunningMode::RUNNINGMODE_GPU) {
			ret = load_model(&model, RunningMode::RUNNINGMODE_CPU);

			if (ret < 0) return -1;
		}
		else {
			return -1;
		}
	}

	// モデル名の取得
	FILE* f;
#ifdef _WIN32
	if (fopen_s(&f, "04_OthelloDeepModel\\MODELINFO.txt", "r") == 0) {
		fscanf_s(f, "%s", modelInfo, (unsigned int)sizeof(modelInfo));
		fclose(f);
	}
#else
	f = fopen("04_OthelloDeepModel/MODELINFO.txt", "r");
	if (f != nullptr) {
		fscanf(f, "%127s", modelInfo);
		fclose(f);
	}
#endif
	else {
#ifdef _WIN32
		strcpy_s(modelInfo, sizeof(modelInfo), MODELINFO);
#else
		strncpy(modelInfo, MODELINFO, sizeof(modelInfo) - 1);
		modelInfo[sizeof(modelInfo) - 1] = '\0';
#endif
	}

	// 温度パラメータ､繰り返し回数､探索方法をセット
	spTemperature = thinkerInitParam->spTemperature;
	numIterations = thinkerInitParam->numIterations;
	isBreadthFirst = thinkerInitParam->isBreadthFirst;
	limitTemperaturePeriod = thinkerInitParam->limitTemperaturePeriod;
	thinkArc = thinkerInitParam->thinkArc;
	numThreads = thinkerInitParam->numThreads;
	depth = thinkerInitParam->depth;

	// リターン
	isInitialized = true;

	return 0;
}

Thinker::~Thinker()
{
	if (isInitialized == true) {
		free_model(&model);
	}
}

//
//	Function Name: think
//	Summary: Set the search depth and then call findBestPlaceForCurrentPlayer to find the best place for the current player.
//	
//	In:
//		No parameters.
//
//	Return:
//		Position for the disk. If the position is (x, y), the result value is x * 10 + y.
//
int Thinker::think(int turn, DISKCOLORS *board, int *place, GameId gameId)
{
	ThinkerV1 thinkerV1;
	int ret = 0;

	// Decide thinking mode.
	int numSpaceLeft;
	numSpaceLeft = CountDisk(DISKCOLORS::COLOR_NONE, board);
	LOGOUT(LOGLEVEL_TRACE, "残り石数 = %d.", numSpaceLeft);

	// Temperature値の修正
	if (limitTemperaturePeriod == true && numSpaceLeft <= 50) {
		spTemperature = 0.0;
	}

	int endThreshold = (thinkArc == THINKARC::THINKARC_MINMAX_MP) ? NUM_FOR_GAMESTATE_END_MP : NUM_FOR_GAMESTATE_END;
	if (thinkArc == THINKARC::THINKARC_MINMAX || numSpaceLeft <= endThreshold) {
		// thinkerV1(=完全読みモード)で思考
		// thinkerV1の初期化
		ret = thinkerV1.SetParams(turn, board, thinkArc, numThreads, depth);
		if (ret < 0) return -1;

		ret = thinkerV1.think();
		if (ret == -11) return -3; // MinMax mode PASS -> return -3 for Think Reject
		if (ret < 0) return -2;

		// 最善手をplaceにセット
		*place = ret;

		// Add to History
		DISKCOLORS boardToHistory[64];
#ifdef _WIN32
		memmove_s(boardToHistory, sizeof(boardToHistory), board, sizeof(DISKCOLORS) * 64);
#else
		memmove(boardToHistory, board, sizeof(DISKCOLORS) * 64);
#endif

		std::vector<Score> scoresList;
		Score score;
		score.x = *place / 10;
		score.y = *place % 10;
		score.n = 0;
		score.probability = 1.0;
		try {
			scoresList.push_back(score);
		}
		catch (...) {
			return -2;
		}

		ret = history.add(gameId, CURRENTPLAYER(turn), boardToHistory, scoresList);
		if (ret < 0) return -3;
	}
	else {
		// Deep Learningベースで思考
		// Stateオブジェクトの生成
		State* state;

		try {
			state = new State;
		}
		catch (...) {
			return -4;
		}

		state->init(board, CURRENTPLAYER(turn));

		// pv_mcts_actionオブジェクトの生成
		Pv_mcts_action* next_action;

		try {
			next_action = new Pv_mcts_action(&model, spTemperature, numIterations, isBreadthFirst);
		}
		catch (...) {
			return -5;
		}

		printf("Thinking(Deep Learning)...\n");

		// 次の手を得る
		Action action;
		ret = next_action->run(state, &action, gameId);
		if (ret < 0) return -6;

		if (action == 80) return -3; // Deep Learning mode PASS -> return -3 for Think Reject

		// Set the best place to "place"
		*place = (int)action;

		// Delete next_action
		delete(next_action);

		// Delete state
		delete state;
	}

	return 0;
}

char* Thinker::getModelInfo()
{
	return modelInfo;
}

int Thinker::CountDisk(DISKCOLORS color, DISKCOLORS _board[64])
{
	int c = 0, i;

	for (i = 0; i < 64; i++) {
		if (_board[i] == color) c++;
	}
	return c;
}
