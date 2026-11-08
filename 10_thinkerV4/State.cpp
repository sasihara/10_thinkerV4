#include <string.h>
#include "State.hpp"
#include "logging.h"
#include "thinkV1.hpp"

State::State()
{
	initialized = false;
}

int State::init(DISKCOLORS* from, DISKCOLORS _currentPlayer)
{
	memmove(board, from, sizeof(board));
	currentPlayer = _currentPlayer;
	opponent = OPPONENT(currentPlayer);
	initialized = true;
	return 0;
}

int State::copyTo(State *to)
{
	memmove(to->board, board, sizeof(board));
	to->currentPlayer = currentPlayer;
	to->opponent = opponent;
	to->initialized = initialized;

	return 0;
}

//
//	Function Name: check
//	Summary: Check if the specified color's disk can put the specified place or not.
//	
//	In:
//		board:		The pointer to the board array.
//		xPos, yPos:	The position to put the disk.
//		color:		The disk color to put.
//
//	Return:
//		The flag which direction's disks will turn by putting the specified disk at the specified place.
//			bit 1:	Upper.
//			bit 2:	Upper right.
//			bit 3:	Right.
//			bit 4:	Lower right.
//			bit 5:	Lower.
//			bit 6:	Lower left.
//			bit 7:	Left.
//			bit 8:	Upper Left.

int State::check(int xPos, int yPos, DISKCOLORS diceColor)
{
	int ret = 0;

	if (initialized == false) return -1;

	if (xPos < 0 || xPos > 7 || yPos < 0 || yPos > 7) return -2;
	if (board[xPos * 8 + yPos] != DISKCOLORS::COLOR_NONE) return -3;
	if (checkOneDir(xPos, yPos, diceColor, 0, -1) > 0) ret = ret | 1;
	if (checkOneDir(xPos, yPos, diceColor, 1, -1) > 0) ret = ret | 2;
	if (checkOneDir(xPos, yPos, diceColor, 1, 0) > 0) ret = ret | 4;
	if (checkOneDir(xPos, yPos, diceColor, 1, 1) > 0) ret = ret | 8;
	if (checkOneDir(xPos, yPos, diceColor, 0, 1) > 0) ret = ret | 16;
	if (checkOneDir(xPos, yPos, diceColor, -1, 1) > 0) ret = ret | 32;
	if (checkOneDir(xPos, yPos, diceColor, -1, 0) > 0) ret = ret | 64;
	if (checkOneDir(xPos, yPos, diceColor, -1, -1) > 0) ret = ret | 128;

	return ret;
}

//
//	Function Name: checkOneDir
//	Summary: Return the number of disks will be turned in the specified direction if the specified disk put to the specified place.
//
//	In:
//		board:			The pointer to the board array.
//		xPos, yPos:		The position to put the disk.
//		color:			The disk color to put.
//		xStep, yStep:	The direction to be checked.
//
//	Return:	The number of disks to be turned.
//
int State::checkOneDir(int xPos, int yPos, DISKCOLORS diceColor, int xStep, int yStep)
{
	// Check parameters
	if (xPos < 0 || xPos > 7 || yPos < 0 || yPos > 7) return 0;
	if (diceColor != DISKCOLORS::COLOR_BLACK && diceColor != DISKCOLORS::COLOR_WHITE) return 0;
	if (xStep < -1 || xStep > 1 || yStep < -1 || yStep > 1) return 0;

	// Check if the position is empty or not
	if (board[xPos * 8 + yPos] != DISKCOLORS::COLOR_NONE) return 0;

	// Check if the next place is on the board or not
	if (xPos + xStep < 0 || xPos + xStep > 7 || yPos + yStep < 0 || yPos + yStep > 7) return 0;

	// Count the number of disks to be turned
	int x, y, NumTurned = 0;
	for (x = xPos + xStep, y = yPos + yStep; 0 <= x && x <= 7 && 0 <= y && y <= 7; x += xStep, y += yStep) {
		int i = x * 8 + y;
		if (board[i] == diceColor) return NumTurned;				// meeting player's disk
		else if (board[i] == DISKCOLORS::COLOR_NONE) return 0;		// reached to empty without meeting player's disk
		else if (board[i] == OPPONENT(diceColor)) NumTurned++;		// meeting opponent's disk
		else return 0;												// Illegal case
	}

	// In this case, reached to the edge without meeting player's disk
	return 0;
}

//
//	Function Name: turnDisk
//	Summary: Turn disks by putting a disk at the specified place of the specified color.
//
//	In:
//		board:			The pointer to the board array.
//		xPos, yPos:		The position to put the disk.
//		color:			The disk color to put.
//		flag:			The result of check() function.
//
//	Return:	The number of disks to be turned.
//
int State::turnDisk(int xPos, int yPos, DISKCOLORS diceColor, int flag)
{
	if (initialized == false) return -1;

	if (xPos < 0 || xPos > 7 || yPos < 0 || yPos > 7) return 0;
	if (board[xPos * 8 + yPos] != DISKCOLORS::COLOR_NONE) return 0;
	if (flag & 1) turnDiskOneDir(xPos, yPos, diceColor, 0, -1);
	if (flag & 2) turnDiskOneDir(xPos, yPos, diceColor, 1, -1);
	if (flag & 4) turnDiskOneDir(xPos, yPos, diceColor, 1, 0);
	if (flag & 8) turnDiskOneDir(xPos, yPos, diceColor, 1, 1);
	if (flag & 16) turnDiskOneDir(xPos, yPos, diceColor, 0, 1);
	if (flag & 32) turnDiskOneDir(xPos, yPos, diceColor, -1, 1);
	if (flag & 64) turnDiskOneDir(xPos, yPos, diceColor, -1, 0);
	if (flag & 128) turnDiskOneDir(xPos, yPos, diceColor, -1, -1);

	return 0;
}

//
//	Function Name: turnDiskOneDir
//	Summary: Turn disks by putting a disk at the specified place of the specified color in the specified direction.
//
//	In:
//		board:			The pointer to the board array.
//		xPos, yPos:		The position to put the disk.
//		color:			The disk color to put.
//		xStep, yStep:	The direction to be turned.
//
//	Return:	The number of disks to be turned.
//
int State::turnDiskOneDir(int xPos, int yPos, DISKCOLORS diceColor, int xStep, int yStep)
{
	// Place player's disk on the board
	board[xPos * 8 + yPos] = diceColor;

	// Turn disks
	int x, y;
	for (x = xPos + xStep, y = yPos + yStep; 0 <= x && x <= 7 && 0 <= y && y <= 7; x += xStep, y += yStep) {
		int i = x * 8 + y;
		if (board[i] == diceColor) break;
		board[i] = diceColor;
	}

	return 0;
}

//
//	Function Name: IsPlayerShouldPass
//	Summary: Check if the player should pass or not.
//	
//	In:
//		_turn:			The number of turn.
//
//	Return:
//		false:	The player should not pass.
//		true:	The player should pass.
//
int State::IsPlayerMustPass(bool *result)
{
	int x, y;

	if (initialized == false) return -1;

	// Check if current player can put a disk or not
	for (x = 0; x < 8; x++) {
		for (y = 0; y < 8; y++) {
			if (check(x, y, currentPlayer) > 0) {
				*result = false;
				return 0;
			}
		}
	}

	*result = true;
	return 0;
}

//
//	Function Name: IsNextPlayerMustPass
//	Summary: Check if the next player must pass or not.
//	
//	In:
//		No parameters.
//
//	Return:
//		true: Next player must pass.
//		false: Next player must not pass.
//
int State::IsNextPlayerMustPass(bool *result)
{
	int x, y;

	if (initialized == false) return -1;

	// Check if current player can put a disk or not
	for (x = 0; x < 8; x++) {
		for (y = 0; y < 8; y++) {
			if (check(x, y, opponent) > 0) {
				*result = false;
				return 0;
			}
		}
	}

	*result = true;
	return 0;
}

//
//	Function Name: IsGameOver
//	Summary: Check if the game is finished or not.
//	
//	In:
//		_turn:			The number of turn.
//
//	Return:
//		false:	The game is not finished.
//		true:	The game is finished.
//
int State::IsGameOver(bool *result)
{
	int ret;
	bool isPlayerMustPass;
	
	if (initialized == false) return -1;

	*result = false;

	ret = IsPlayerMustPass(&isPlayerMustPass);
	if (ret < 0) return -1;

	if (isPlayerMustPass == true) {
		ret = IsNextPlayerMustPass(&isPlayerMustPass);
		if (ret < 0) return -2;

		if (isPlayerMustPass == true) *result = true;
	}

	return 0;
}

int State::getGameResult(GAMERESULT *result)
{
	int diceNumCurrentPlayer = 0, diceNumOpponent = 0;

	if (initialized == false) return -1;

	for (int i = 0; i < 64; i++) {
		if (board[i] == currentPlayer) diceNumCurrentPlayer++;
		else if (board[i] == opponent) diceNumOpponent++;
	}

	if (diceNumCurrentPlayer > diceNumOpponent) *result = GAMERESULT_WIN;
	else if (diceNumCurrentPlayer < diceNumOpponent) *result = GAMERESULT_LOSE;
	else *result = GAMERESULT_EVEN;

	return 0;
}

int State::logout(Logging logging)
{
	return logoutBoard(logging, board);
}

//int State::logoutBoard(Logging* logging)
//{
//	int ret = 0;
//
//	for (size_t y = 0; y < 8; y++) {
//		for (size_t x = 0; x < 8; x++) {
//			size_t i = x * 8 + y;
//			logging->logprintf("%s", board[i] == DISKCOLORS::COLOR_BLACK ? "œ" : board[i] == DISKCOLORS::COLOR_WHITE ? "›" : "@");
//		}
//		logging->logprintf("\n");
//	}
//
//	return ret;
//}
//
//int logoutBoard(Logging logging, DISKCOLORS* _board)
//{
//	for (int y = 0; y < 8; y++) {
//		for (int x = 0; x < 8; x++) {
//			switch (_board[x * 8 + y]) {
//			case DISKCOLORS::COLOR_BLACK:
//				logging.logprintf(LOGLEVEL_TRACE, "œ");
//				break;
//			case DISKCOLORS::COLOR_WHITE:
//				logging.logprintf(LOGLEVEL_TRACE, "›");
//				break;
//			default:
//				logging.logprintf(LOGLEVEL_TRACE, "E");
//				break;
//			}
//		}
//		logging.logprintf(LOGLEVEL_TRACE, "\n");
//	}
//
//	return 0;
//}
