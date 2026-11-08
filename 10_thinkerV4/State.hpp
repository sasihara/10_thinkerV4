#pragma once
#include "externalThinkerMessages.hpp"
#include "logging.h"

enum GAMERESULT {
	GAMERESULT_ERROR = -1,
	GAMERESULT_EVEN = 0,
	GAMERESULT_WIN = 1,
	GAMERESULT_LOSE = 2
};

class State {
public:
	DISKCOLORS board[64];
	DISKCOLORS currentPlayer;
	DISKCOLORS opponent;

	State();
	int init(DISKCOLORS* from, DISKCOLORS _currentPlayer);
	int copyTo(State *to);
	int check(int xPos, int yPos, DISKCOLORS diceColor);
	int turnDisk(int xPos, int yPos, DISKCOLORS diceColor, int flag);
	int IsPlayerMustPass(bool* result);
	int IsNextPlayerMustPass(bool* result);
	int IsGameOver(bool* result);
	int getGameResult(GAMERESULT* result);
	int logout(Logging logging);
	//int logoutBoard(Logging* logging);
private:
	bool initialized = false;

	int checkOneDir(int xPos, int yPos, DISKCOLORS diceColor, int xStep, int yStep);
	int turnDiskOneDir(int xPos, int yPos, DISKCOLORS diceColor, int xStep, int yStep);
};

//int logoutBoard(Logging logging, DISKCOLORS* _board);
