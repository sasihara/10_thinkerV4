// Othello Thinker Version 3.00
// Copyright (C) 1994  T.Sashihara
// This program goes with Othello for Windows Ver 3.00

#include <omp.h>
#include <stdio.h>
#include <memory.h>
#include <limits.h>
#include <arm_sve.h>
#include <arm_neon.h>
#include "externalThinkerMessages.hpp"
#include "thinkV1.hpp"
#include <atomic>

extern Logging logging;

namespace {
	constexpr int SCORE_INF = 1000;
	constexpr size_t TT_SIZE_BITS = 20; // 2^20 = 1,048,576 entries (~24MB)
	constexpr size_t TT_SIZE = 1ULL << TT_SIZE_BITS;
	constexpr size_t TT_MASK = TT_SIZE - 1;
	constexpr int TT_MIN_EMPTIES = 7;
	constexpr int FF_MIN_EMPTIES = 9;

	struct TTEntry {
		uint64_t my = 0ULL;
		uint64_t opp = 0ULL;
		int8_t lower = -64;
		int8_t upper = 64;
		uint8_t bestMove = 64;
		std::atomic_flag lock = ATOMIC_FLAG_INIT;
	};

	TTEntry g_tt[TT_SIZE];

	inline size_t compute_tt_index(uint64_t my, uint64_t opp) {
		uint64_t h = (my * 0x9E3779B97F4A7C15ULL) ^ (opp * 0xBF58476D1CE4E5B9ULL);
		h ^= (h >> 30);
		h *= 0x94D049BB133111EBULL;
		h ^= (h >> 31);
		return (size_t)(h & TT_MASK);
	}
}

//
//	Function Name: SetParams
//	Summary: Set parameters for thinker.
//	
//	In:
//		_turn:	The turn value.
//		_board:	The board data.
//
//	Return:
//		0: Succeed
//
int ThinkerV1::SetParams(int _turn, DISKCOLORS _board[64], THINKARC _thinkArc, int _numThreads, int _depth)
{
	// Player detection
	currentPlayer = CURRENTPLAYER(_turn);
	opponent = OPPONENT(currentPlayer);

	memcpy(board, _board, sizeof(board));

	// DISKCOLOR board[64]をuint64_tのビットボードに変換して格納する
	bitBoard.my = 0;
	bitBoard.opp = 0;
	
	for (size_t i = 0; i < 64; i++) {
		size_t k = (i % 8) * 8 + (i / 8);

		if (_board[i] == currentPlayer) {
			bitBoard.my |= (1ULL << k);
		}
		else if (_board[i] == opponent) {
			bitBoard.opp |= (1ULL << k);
		}
	}

	// ★デバッグ
	LOGOUT(LOGLEVEL_TRACE, "*** Result of logoutBoard ***");
	logoutBoard(logging, board);
	LOGOUT(LOGLEVEL_TRACE, "*** Result of logoutBitBoard ***");
	logoutBitBoard(logging, bitBoard.my, bitBoard.opp, currentPlayer);

	turn = _turn;
	thinkArc = _thinkArc;
	numThreads = _numThreads;
	depth = _depth;
	return 0;
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
int ThinkerV1::think()
{
	int depthToApply;
	int ret;
	int numSpaceLeft;

	LOGOUT(LOGLEVEL_TRACE, "★==================== think() start. ====================★");

#ifdef _DEBUG
	logging.logprintf("*** 現在の盤面 ***\n");
	logging.logprintf("プレイヤーの石の色: %s\n", currentPlayer == DISKCOLORS::COLOR_BLACK ? "●" : currentPlayer == DISKCOLORS::COLOR_WHITE ? "○" : "");
	ret = logoutBoard(logging, board);
#endif

	// Decide thinking mode.
	// numSpaceLeft = CountDisk(DISKCOLORS::COLOR_NONE, board);
	// numSpaceLeft = popcount_neon(~(bitBoard.my | bitBoard.opp));
	// LOGOUT(LOGLEVEL_TRACE, "残り石数 = %d.", numSpaceLeft);

	// if (numSpaceLeft <= depth * 2) {
	// 	depthToApply = INT_MAX;
	// 	thinkerState = GAMESTATE::GAMESTATE_END;
	// 	LOGOUT(LOGLEVEL_TRACE, "完全読みモードへ移ります.");
	// }
	// else if (numSpaceLeft <= NUM_FOR_GAMESTATE_MIDFIELD) {
	// 	depthToApply = depth;
	// 	thinkerState = GAMESTATE::GAMESTATE_MIDFIELD;
	// 	LOGOUT(LOGLEVEL_TRACE, "中盤モードで進めます.");
	// }
	// else {
	// 	depthToApply = depth;
	// 	thinkerState = GAMESTATE::GAMESTATE_EARLY_STAGE;
	// 	LOGOUT(LOGLEVEL_TRACE, "序盤モードで進めます.");
	// }

	depthToApply = INT_MAX;
	thinkerState = GAMESTATE::GAMESTATE_END;
	LOGOUT(LOGLEVEL_TRACE, "完全読みモードで進めます.");

	// Start to think.
	if (thinkArc == THINKARC::THINKARC_MINMAX_MP) {
		printf("Thinking(Min-Max MP, %dthreads)...\n", numThreads);
		ret = findBestPlaceForCurrentPlayerMP(depthToApply);
	}
	else {
		printf("Thinking(Min-Max)...\n");
		ret = findBestPlaceForCurrentPlayer(depthToApply);
	}

	LOGOUT(LOGLEVEL_TRACE, "★==================== think() end. ====================★");
	return ret;
}

//
//	Function Name: CountDisk
//	Summary: Count the number of dice of the specified color on the specified board.
//	
//	In:
//		_color: The disk color to count.
//		_board: The board to count.
//
//	Return:
//		The number of dice.
//
int ThinkerV1::CountDisk(DISKCOLORS color, DISKCOLORS _board[64])
{
	int c = 0, i;

	for (i = 0; i < 64; i++) {
		if (_board[i] == color) c++;
	}
	return c;
}

//
//	Function Name: findBestPlaceForCurrentPlayer
//	Summary: Find the best place for the current player in the specified depth.
//	
//	In:
//		lv:	Search depth.
//
//	Return:
//		The best place to put current player's disk. If the place is (x, y), the return value is x * 10 + y.
//
int ThinkerV1::findBestPlaceForCurrentPlayer(int lv)
{
	(void)lv;
	uint64_t legal_moves = generate_legal_moves_sve2(bitBoard.my, bitBoard.opp);
	if (legal_moves == 0ULL) {
		return -11;
	}

	int empties = popcount_neon(~(bitBoard.my | bitBoard.opp));

	struct RootMove {
		int pos;
		int orderScore;
		uint64_t tmpMy;
		uint64_t tmpOpp;
	};
	RootMove moves[64];
	int numMoves = 0;

	uint64_t rem = legal_moves;
	while (rem) {
		int movePos = __builtin_ctzll(rem);
		rem &= rem - 1;

		uint64_t rev = flip_discs_sve2(bitBoard.my, bitBoard.opp, movePos);
		uint64_t tmpMy = bitBoard.my | rev | (1ULL << movePos);
		uint64_t tmpOpp = bitBoard.opp & (~rev);

		int oppMob = popcount_neon(generate_legal_moves_sve2(tmpOpp, tmpMy));
		uint64_t bit = 1ULL << movePos;
		int priorityBias = 0;
		if (bit & MOVE_PRIORITY_MASKS[0]) priorityBias = -16;
		else if (bit & MOVE_PRIORITY_MASKS[4]) priorityBias = 8;
		else if (bit & MOVE_PRIORITY_MASKS[3]) priorityBias = 4;
		int orderScore = (oppMob << 2) + priorityBias;

		int ins = numMoves++;
		while (ins > 0 && moves[ins - 1].orderScore > orderScore) {
			moves[ins] = moves[ins - 1];
			ins--;
		}
		moves[ins] = { movePos, orderScore, tmpMy, tmpOpp };
	}

	if (numMoves == 1) {
		int res = moves[0].pos % 8 * 10 + moves[0].pos / 8;
		fflush(stdout);
		return res;
	}

	int bestEval = -SCORE_INF;
	int bestPos = moves[0].pos;

	for (int i = 0; i < numMoves; i++) {
		int score;
		if (i == 0) {
			score = -NegaAlpha(empties - 1, false, -SCORE_INF, -bestEval, moves[i].tmpOpp, moves[i].tmpMy);
		}
		else {
			score = -NegaAlpha(empties - 1, false, -bestEval - 1, -bestEval, moves[i].tmpOpp, moves[i].tmpMy);
			if (score > bestEval) {
				score = -NegaAlpha(empties - 1, false, -SCORE_INF, -bestEval, moves[i].tmpOpp, moves[i].tmpMy);
			}
		}

		if (score > bestEval) {
			bestEval = score;
			bestPos = moves[i].pos;
		}
	}

	int res = bestPos % 8 * 10 + bestPos / 8;
	fflush(stdout);
	return res;
}

int ThinkerV1::findBestPlaceForCurrentPlayerMP(int lv)
{
	(void)lv;
	// 使用するスレッド数を設定
	if (this->numThreads > 0) {
		omp_set_num_threads(this->numThreads);
	}

	uint64_t legal_moves = generate_legal_moves_sve2(bitBoard.my, bitBoard.opp);
	if (legal_moves == 0ULL) {
		return -11;
	}

	int empties = popcount_neon(~(bitBoard.my | bitBoard.opp));

	// 置換表にルートの最善手が記録されていれば取得
	int ttRootBest = -1;
	{
		size_t ttIdx = compute_tt_index(bitBoard.my, bitBoard.opp);
		TTEntry& entry = g_tt[ttIdx];
		if (!entry.lock.test_and_set(std::memory_order_acquire)) {
			if (entry.my == bitBoard.my && entry.opp == bitBoard.opp && entry.bestMove < 64) {
				ttRootBest = entry.bestMove;
			}
			entry.lock.clear(std::memory_order_release);
		}
	}

	// ルート合法手を Fastest-First（相手の合法手数が最小になる順）でソート
	struct RootMove {
		int pos;
		int orderScore;
		uint64_t tmpMy;
		uint64_t tmpOpp;
	};
	RootMove moves[64];
	int numMoves = 0;

	uint64_t rem = legal_moves;
	while (rem) {
		int movePos = __builtin_ctzll(rem);
		rem &= rem - 1;

		uint64_t rev = flip_discs_sve2(bitBoard.my, bitBoard.opp, movePos);
		uint64_t tmpMy = bitBoard.my | rev | (1ULL << movePos);
		uint64_t tmpOpp = bitBoard.opp & (~rev);

		int orderScore;
		if (movePos == ttRootBest) {
			orderScore = -1000;
		}
		else {
			int oppMob = popcount_neon(generate_legal_moves_sve2(tmpOpp, tmpMy));
			uint64_t bit = 1ULL << movePos;
			int priorityBias = 0;
			if (bit & MOVE_PRIORITY_MASKS[0]) priorityBias = -16;
			else if (bit & MOVE_PRIORITY_MASKS[4]) priorityBias = 8;
			else if (bit & MOVE_PRIORITY_MASKS[3]) priorityBias = 4;
			orderScore = (oppMob << 2) + priorityBias;
		}

		int ins = numMoves++;
		while (ins > 0 && moves[ins - 1].orderScore > orderScore) {
			moves[ins] = moves[ins - 1];
			ins--;
		}
		moves[ins] = { movePos, orderScore, tmpMy, tmpOpp };
	}

	// 合法手が1手のみの場合は探索不要で即リターン
	if (numMoves == 1) {
		int res = moves[0].pos % 8 * 10 + moves[0].pos / 8;
		fflush(stdout);
		return res;
	}

	// 【YBWC: 初手先行探索】まず最有力な1手目（idx = 0）を内部並列探索（NegaAlphaParallelNode）で高速に探索して高い下限値 alpha を確定させる
	int bestEval = -NegaAlphaParallelNode(empties - 1, false, -SCORE_INF, SCORE_INF, moves[0].tmpOpp, moves[0].tmpMy);
	int bestPos = moves[0].pos;
	int bestIdx = 0;
	std::atomic<int> alphaAtomic{ bestEval };

	// 2手目以降（idx = 1 .. numMoves - 1）を確定した alpha と PVS を用いて並列探索
#pragma omp parallel for schedule(dynamic, 1)
	for (int idx = 1; idx < numMoves; idx++) {
		int currentAlpha = alphaAtomic.load(std::memory_order_relaxed);

		// まず幅1の Null Window [-currentAlpha - 1, -currentAlpha] で高速に判定
		int score = -NegaAlpha(empties - 1, false, -currentAlpha - 1, -currentAlpha, moves[idx].tmpOpp, moves[idx].tmpMy);

		// 暫定ベスト（currentAlpha）を上回った場合、最新の alpha を用いてフルウィンドウで正確なスコアを再探索
		// ※Null Window探索の戻り値 score は下限値（currentAlpha + 1 等）に過ぎないため、
		//   他スレッドが latestAlpha を引き上げていた場合でも必ず再探索を行う必要がある
		if (score > currentAlpha) {
			int latestAlpha = alphaAtomic.load(std::memory_order_relaxed);
			score = -NegaAlpha(empties - 1, false, -SCORE_INF, -latestAlpha, moves[idx].tmpOpp, moves[idx].tmpMy);
		}

		// ベストスコアと着手位置を安全に更新
#pragma omp critical(update_best_mp)
		{
			if (score > bestEval || (score == bestEval && idx < bestIdx)) {
				if (score > bestEval) {
					alphaAtomic.store(score, std::memory_order_relaxed);
				}
				bestEval = score;
				bestPos = moves[idx].pos;
				bestIdx = idx;
			}
		}
	}

	// 最善手の座標変換とリターン
	int res = bestPos % 8 * 10 + bestPos / 8;
	fflush(stdout);
	return res;
}

int ThinkerV1::NegaAlpha(int empties, bool passed, int alpha, int beta, uint64_t my, uint64_t opp)
{
	if (empties <= 0) return evcal(my, opp);

	// 残り1マス専用処理（Last-1 Flip Count）
	if (empties == 1) {
		int last_pos = __builtin_ctzll(~(my | opp));
		int my_discs = popcount_neon(my);
		uint64_t rev = flip_discs_sve2(my, opp, last_pos);
		if (rev != 0ULL) {
			// 1. 自分が最後の1マスに置ける場合（計64石）
			return 2 * (my_discs + popcount_neon(rev) + 1) - 64;
		}
		uint64_t rev_opp = flip_discs_sve2(opp, my, last_pos);
		if (rev_opp != 0ULL) {
			// 2. 自分がパスで、相手が最後の1マスに置ける場合（計64石）
			return 2 * (my_discs - popcount_neon(rev_opp)) - 64;
		}
		// 3. 双方とも置けない場合（計63石以下）
		int opp_discs = popcount_neon(opp);
		return my_discs - opp_discs;
	}

	int origAlpha = alpha;
	int ttBestMove = -1;
	size_t ttIdx = 0;

	// 置換表（Transposition Table）の参照
	if (empties >= TT_MIN_EMPTIES) {
		ttIdx = compute_tt_index(my, opp);
		TTEntry& entry = g_tt[ttIdx];
		if (!entry.lock.test_and_set(std::memory_order_acquire)) {
			if (entry.my == my && entry.opp == opp) {
				int l = entry.lower;
				int u = entry.upper;
				int bm = entry.bestMove;
				entry.lock.clear(std::memory_order_release);

				if (l >= beta) return l;
				if (u <= alpha) return u;
				if (l == u) return l;
				if (l > alpha) alpha = l;
				if (u < beta) beta = u;
				if (alpha >= beta) return l;
				if (bm < 64) ttBestMove = bm;
			}
			else {
				entry.lock.clear(std::memory_order_release);
			}
		}
	}

	// 合法手を得る
	uint64_t legal_moves = generate_legal_moves_sve2(my, opp);

	// 合法手がない場合（パスまたは終局）
	if (legal_moves == 0ULL) {
		if (passed) return evcal(my, opp);
		return -NegaAlpha(empties, true, -beta, -alpha, opp, my);
	}

	int maxScore = -SCORE_INF;
	int bestMove = -1;

	if (empties >= FF_MIN_EMPTIES) {
		// Fastest-First（着手後の相手の合法手数が少ない順）＋ TT最善手優先
		struct MoveNode {
			int pos;
			int orderScore;
			uint64_t tmpMy;
			uint64_t tmpOpp;
		};
		MoveNode cands[32];
		int numCands = 0;

		uint64_t rem = legal_moves;
		while (rem) {
			int movePos = __builtin_ctzll(rem);
			rem &= rem - 1;

			uint64_t rev = flip_discs_sve2(my, opp, movePos);
			uint64_t tmpMy = my | rev | (1ULL << movePos);
			uint64_t tmpOpp = opp & (~rev);

			int orderScore;
			if (movePos == ttBestMove) {
				orderScore = -1000;
			}
			else {
				int oppMob = popcount_neon(generate_legal_moves_sve2(tmpOpp, tmpMy));
				uint64_t bit = 1ULL << movePos;
				int priorityBias = 0;
				if (bit & MOVE_PRIORITY_MASKS[0]) priorityBias = -16;
				else if (bit & MOVE_PRIORITY_MASKS[4]) priorityBias = 8;
				else if (bit & MOVE_PRIORITY_MASKS[3]) priorityBias = 4;
				orderScore = (oppMob << 2) + priorityBias;
			}

			int ins = numCands++;
			while (ins > 0 && cands[ins - 1].orderScore > orderScore) {
				cands[ins] = cands[ins - 1];
				ins--;
			}
			cands[ins] = { movePos, orderScore, tmpMy, tmpOpp };
		}

		for (int i = 0; i < numCands; i++) {
			int score;
			if (i == 0) {
				score = -NegaAlpha(empties - 1, false, -beta, -alpha, cands[i].tmpOpp, cands[i].tmpMy);
			}
			else {
				// PVS (Null Window Search)
				score = -NegaAlpha(empties - 1, false, -alpha - 1, -alpha, cands[i].tmpOpp, cands[i].tmpMy);
				if (score > alpha && score < beta) {
					score = -NegaAlpha(empties - 1, false, -beta, -alpha, cands[i].tmpOpp, cands[i].tmpMy);
				}
			}

			if (score > maxScore) {
				maxScore = score;
				bestMove = cands[i].pos;
				if (score > alpha) {
					alpha = score;
					if (alpha >= beta) break;
				}
			}
		}
	}
	else {
		// empties < FF_MIN_EMPTIES: 固定優先度マスク順（ソートコスト省略）＋ PVS
		uint64_t remaining = legal_moves;
		bool isFirst = true;

		if (ttBestMove >= 0 && (remaining & (1ULL << ttBestMove))) {
			remaining &= ~(1ULL << ttBestMove);
			uint64_t rev = flip_discs_sve2(my, opp, ttBestMove);
			uint64_t tmpMy = my | rev | (1ULL << ttBestMove);
			uint64_t tmpOpp = opp & (~rev);

			int score = -NegaAlpha(empties - 1, false, -beta, -alpha, tmpOpp, tmpMy);
			isFirst = false;
			maxScore = score;
			bestMove = ttBestMove;
			if (score > alpha) {
				alpha = score;
			}
		}

		if (alpha < beta) {
			for (int g = 0; g < 5 && remaining != 0ULL; g++) {
				uint64_t moves = remaining & MOVE_PRIORITY_MASKS[g];
				remaining &= ~MOVE_PRIORITY_MASKS[g];
				while (moves) {
					int movePos = __builtin_ctzll(moves);
					moves &= moves - 1;

					uint64_t rev = flip_discs_sve2(my, opp, movePos);
					uint64_t tmpMy = my | rev | (1ULL << movePos);
					uint64_t tmpOpp = opp & (~rev);

					int score;
					if (isFirst) {
						score = -NegaAlpha(empties - 1, false, -beta, -alpha, tmpOpp, tmpMy);
						isFirst = false;
					}
					else {
						score = -NegaAlpha(empties - 1, false, -alpha - 1, -alpha, tmpOpp, tmpMy);
						if (score > alpha && score < beta) {
							score = -NegaAlpha(empties - 1, false, -beta, -alpha, tmpOpp, tmpMy);
						}
					}

					if (score > maxScore) {
						maxScore = score;
						bestMove = movePos;
						if (score > alpha) {
							alpha = score;
							if (alpha >= beta) goto search_done;
						}
					}
				}
			}
		}
	}
search_done:

	// 置換表（Transposition Table）への保存
	if (empties >= TT_MIN_EMPTIES) {
		TTEntry& entry = g_tt[ttIdx];
		if (!entry.lock.test_and_set(std::memory_order_acquire)) {
			if (entry.my != my || entry.opp != opp) {
				entry.my = my;
				entry.opp = opp;
				entry.lower = -64;
				entry.upper = 64;
				entry.bestMove = 64;
			}
			if (maxScore <= origAlpha) {
				if (maxScore < entry.upper) entry.upper = (int8_t)maxScore;
			}
			else if (maxScore >= beta) {
				if (maxScore > entry.lower) entry.lower = (int8_t)maxScore;
				if (bestMove >= 0) entry.bestMove = (uint8_t)bestMove;
			}
			else {
				entry.lower = (int8_t)maxScore;
				entry.upper = (int8_t)maxScore;
				if (bestMove >= 0) entry.bestMove = (uint8_t)bestMove;
			}
			entry.lock.clear(std::memory_order_release);
		}
	}

	return maxScore;
}

int ThinkerV1::NegaAlphaParallelNode(int empties, bool passed, int alpha, int beta, uint64_t my, uint64_t opp)
{
	// 空きマスが少ない場合は通常のシリアルNegaAlphaで十分高速
	if (empties < 12) {
		return NegaAlpha(empties, passed, alpha, beta, my, opp);
	}

	int origAlpha = alpha;
	int ttBestMove = -1;
	size_t ttIdx = compute_tt_index(my, opp);
	{
		TTEntry& entry = g_tt[ttIdx];
		if (!entry.lock.test_and_set(std::memory_order_acquire)) {
			if (entry.my == my && entry.opp == opp) {
				int l = entry.lower;
				int u = entry.upper;
				int bm = entry.bestMove;
				entry.lock.clear(std::memory_order_release);

				if (l >= beta) return l;
				if (u <= alpha) return u;
				if (l == u) return l;
				if (l > alpha) alpha = l;
				if (u < beta) beta = u;
				if (alpha >= beta) return l;
				if (bm < 64) ttBestMove = bm;
			}
			else {
				entry.lock.clear(std::memory_order_release);
			}
		}
	}

	uint64_t legal_moves = generate_legal_moves_sve2(my, opp);
	if (legal_moves == 0ULL) {
		if (passed) return evcal(my, opp);
		return -NegaAlphaParallelNode(empties, true, -beta, -alpha, opp, my);
	}

	struct MoveNode {
		int pos;
		int orderScore;
		uint64_t tmpMy;
		uint64_t tmpOpp;
	};
	MoveNode cands[32];
	int numCands = 0;

	uint64_t rem = legal_moves;
	while (rem) {
		int movePos = __builtin_ctzll(rem);
		rem &= rem - 1;

		uint64_t rev = flip_discs_sve2(my, opp, movePos);
		uint64_t tmpMy = my | rev | (1ULL << movePos);
		uint64_t tmpOpp = opp & (~rev);

		int orderScore;
		if (movePos == ttBestMove) {
			orderScore = -1000;
		}
		else {
			int oppMob = popcount_neon(generate_legal_moves_sve2(tmpOpp, tmpMy));
			uint64_t bit = 1ULL << movePos;
			int priorityBias = 0;
			if (bit & MOVE_PRIORITY_MASKS[0]) priorityBias = -16;
			else if (bit & MOVE_PRIORITY_MASKS[4]) priorityBias = 8;
			else if (bit & MOVE_PRIORITY_MASKS[3]) priorityBias = 4;
			orderScore = (oppMob << 2) + priorityBias;
		}

		int ins = numCands++;
		while (ins > 0 && cands[ins - 1].orderScore > orderScore) {
			cands[ins] = cands[ins - 1];
			ins--;
		}
		cands[ins] = { movePos, orderScore, tmpMy, tmpOpp };
	}

	// 1手目を探索（まだ空きマスが多ければさらに1段下も並列分割して常に全スレッドを稼働させる）
	int firstScore;
	if (empties >= 16) {
		firstScore = -NegaAlphaParallelNode(empties - 1, false, -beta, -alpha, cands[0].tmpOpp, cands[0].tmpMy);
	}
	else {
		firstScore = -NegaAlpha(empties - 1, false, -beta, -alpha, cands[0].tmpOpp, cands[0].tmpMy);
	}

	int maxScore = firstScore;
	int bestMove = cands[0].pos;
	if (firstScore > alpha) {
		alpha = firstScore;
	}

	if (alpha < beta && numCands > 1) {
		std::atomic<int> sharedAlpha{ alpha };
		std::atomic<bool> cutoffFlag{ false };

#pragma omp parallel for schedule(dynamic, 1)
		for (int i = 1; i < numCands; i++) {
			if (cutoffFlag.load(std::memory_order_relaxed)) continue;

			int curA = sharedAlpha.load(std::memory_order_relaxed);
			if (curA >= beta) {
				cutoffFlag.store(true, std::memory_order_relaxed);
				continue;
			}

			int score = -NegaAlpha(empties - 1, false, -curA - 1, -curA, cands[i].tmpOpp, cands[i].tmpMy);
			if (score > curA && score < beta && !cutoffFlag.load(std::memory_order_relaxed)) {
				int latestA = sharedAlpha.load(std::memory_order_relaxed);
				if (latestA < beta) {
					score = -NegaAlpha(empties - 1, false, -beta, -latestA, cands[i].tmpOpp, cands[i].tmpMy);
				}
			}

#pragma omp critical(update_parallel_node)
			{
				if (score > maxScore) {
					maxScore = score;
					bestMove = cands[i].pos;
				}
				if (score > sharedAlpha.load(std::memory_order_relaxed)) {
					sharedAlpha.store(score, std::memory_order_relaxed);
					if (score >= beta) {
						cutoffFlag.store(true, std::memory_order_relaxed);
					}
				}
			}
		}
	}

	// 置換表への保存
	{
		TTEntry& entry = g_tt[ttIdx];
		if (!entry.lock.test_and_set(std::memory_order_acquire)) {
			if (entry.my != my || entry.opp != opp) {
				entry.my = my;
				entry.opp = opp;
				entry.lower = -64;
				entry.upper = 64;
				entry.bestMove = 64;
			}
			if (maxScore <= origAlpha) {
				if (maxScore < entry.upper) entry.upper = (int8_t)maxScore;
			}
			else if (maxScore >= beta) {
				if (maxScore > entry.lower) entry.lower = (int8_t)maxScore;
				if (bestMove >= 0) entry.bestMove = (uint8_t)bestMove;
			}
			else {
				entry.lower = (int8_t)maxScore;
				entry.upper = (int8_t)maxScore;
				if (bestMove >= 0) entry.bestMove = (uint8_t)bestMove;
			}
			entry.lock.clear(std::memory_order_release);
		}
	}

	return maxScore;
}

int ThinkerV1::MaxLevel(int lv, bool f, int alpha, int beta, uint64_t my, uint64_t opp)
{
	(void)lv;
	int empties = popcount_neon(~(my | opp));
	return NegaAlpha(empties, f, alpha, beta, my, opp);
}

int ThinkerV1::MinLevel(int lv, bool f, int alpha, int beta, uint64_t my, uint64_t opp)
{
	(void)lv;
	int empties = popcount_neon(~(my | opp));
	return -NegaAlpha(empties, f, -beta, -alpha, my, opp);
}

//
//	Function Name: evcal
//	Summary: Evaluate the board for the current player.
//	
//	In:
//		my:   Current player's bitboard.
//		opp:  Opponent's bitboard.
//
//	Return:
//		Score.
//
int ThinkerV1::evcal(uint64_t my, uint64_t opp)
{
	return popcount_neon(my) - popcount_neon(opp);
}

/*
 * generate_legal_moves_sve2(my, opp)
 *   概要 : SVE2命令で合法手を高速生成する。
 *          同一シフト方向の2方向を64bit×2レーンで並列処理。
 *          左シフト4方向・右シフト4方向を各2ペアで処理し、
 *          svorv_u64で1命令スカラーリダクション。
 *          Post-Masking設計: shift後にEDGE_MASKで偽ビットを除去。
 *   my   : uint64_t — 自石ビットボード(bit0=a1)
 *   opp  : uint64_t — 相手石ビットボード
 *   戻値 : uint64_t — 合法手ビットマスク(1=石を置ける空きマス)
 */
__attribute__((target("+sve2")))
uint64_t ThinkerV1::generate_legal_moves_sve2(uint64_t my, uint64_t opp)
{
    uint64_t empty = ~(my | opp);

	// svwhilelt_b64(0,2): 2レーン固定の述語
    // svptrue_b64は将来の広幅SVE実装でバッファオーバーリードの恐れがあるため使用不可
    svbool_t pg2 = svwhilelt_b64(
        (uint64_t)0, (uint64_t)2);
 
    svuint64_t v_legal = svdup_u64(0);
 
    // ---- 左シフト方向(<<)の4方向を2ペアで処理 ----
    // ペア0: 右(<<1) と 下(<<8)
    // ペア1: 右下(<<9) と 左下(<<7)
    for (int pair = 0; pair < 4; pair += 2) {
        // レーン別のシフト量をsvld1_u64で配列からロード
        // (svdup_u64は全レーン同値なのでレーン別設定には使用不可)
        const uint64_t s[2] = {
            LSL_SHIFTS[pair],
            LSL_SHIFTS[pair+1] };
        svuint64_t v_shift = svld1_u64(pg2, s);
 
        // レーン別のEDGE_MASKをロード (Post-Masking用)
        const uint64_t m[2] = {
            EDGE_MASK[LSL_MASKS_IDX[pair]],
            EDGE_MASK[LSL_MASKS_IDX[pair+1]] };
        svuint64_t v_mask  = svld1_u64(pg2, m);
 
        svuint64_t v_my    = svdup_u64(my);
        svuint64_t v_opp   = svdup_u64(opp);
        svuint64_t v_empty = svdup_u64(empty);
 
        // Step 1: 全自石を各方向に左シフト後、
        //         AND(opp): 相手石との交差を判定
        //         AND(mask): シフト後の偽ビットを除去 (Post-Masking)
        svuint64_t cand = svand_x(pg2,
            svand_x(pg2,
                svlsl_x(pg2, v_my, v_shift),
                v_opp),
            v_mask);
 
        // Step 2: 相手石列を最大5回拡張
        //         shift後にmaskで毎回偽ビットを除去
        for (int rep = 0; rep < 5; rep++) {
            svuint64_t next = svand_x(pg2,
                svand_x(pg2,
                    svlsl_x(pg2, cand, v_shift),
                    v_opp),
                v_mask);
            cand = svorr_x(pg2, cand, next);
        }
 
        // Step 3: 相手石列の先の空きマスを検出 = 合法手
        svuint64_t hit = svand_x(pg2,
            svand_x(pg2,
                svlsl_x(pg2, cand, v_shift),
                v_empty),
            v_mask);
 
        v_legal = svorr_x(pg2, v_legal, hit);
    }
 
    // ---- 右シフト方向(>>)の4方向を2ペアで処理 ----
    // ペア0: 左(>>1) と 上(>>8)
    // ペア1: 左上(>>9) と 右上(>>7)
    for (int pair = 0; pair < 4; pair += 2) {
        const uint64_t s[2] = {
            LSR_SHIFTS[pair],
            LSR_SHIFTS[pair+1] };
        svuint64_t v_shift = svld1_u64(pg2, s);
 
        const uint64_t m[2] = {
            EDGE_MASK[LSR_MASKS_IDX[pair]],
            EDGE_MASK[LSR_MASKS_IDX[pair+1]] };
        svuint64_t v_mask  = svld1_u64(pg2, m);
 
        svuint64_t v_my    = svdup_u64(my);
        svuint64_t v_opp   = svdup_u64(opp);
        svuint64_t v_empty = svdup_u64(empty);
 
        // Step 1: 右シフト後にPost-Maskingで偽ビット除去
        svuint64_t cand = svand_x(pg2,
            svand_x(pg2,
                svlsr_x(pg2, v_my, v_shift),
                v_opp),
            v_mask);
 
        // Step 2: 相手石列を最大5回拡張
        for (int rep = 0; rep < 5; rep++) {
            svuint64_t next = svand_x(pg2,
                svand_x(pg2,
                    svlsr_x(pg2, cand, v_shift),
                    v_opp),
                v_mask);
            cand = svorr_x(pg2, cand, next);
        }
 
        // Step 3: 空きマスを検出 = 合法手
        svuint64_t hit = svand_x(pg2,
            svand_x(pg2,
                svlsr_x(pg2, cand, v_shift),
                v_empty),
            v_mask);
 
        v_legal = svorr_x(pg2, v_legal, hit);
    }
 
    // 全8方向の結果をsvorv_u64で1命令スカラーリダクション
    return svorv_u64(pg2, v_legal);
}

/*
 * flip_discs_sve2(my, opp, pos)
 *   概要 : 着手位置posに石を置いたとき、挟まれて裏返る
 *          相手石のビットマスクをSVE2で高速に求める。
 *          8方向をLSL/LSRグループに分け、分岐なしで
 *          全方向を一括伝播する真のSVE2実装。
 *   my   : uint64_t — 自石ビットボード
 *   opp  : uint64_t — 相手石ビットボード
 *   pos  : int      — 着手マス番号 (0=a1, 63=h8)
 *   戻値 : uint64_t — 反転する相手石のビットマスク
 */
__attribute__((target("+sve2")))
uint64_t ThinkerV1::flip_discs_sve2(uint64_t my, uint64_t opp, int pos)
{
    uint64_t placed = 1ULL << pos;
    svbool_t pg2 = svwhilelt_b64((uint64_t)0, (uint64_t)2);
    svuint64_t v_flips = svdup_u64(0);
 
    // ---- 左シフト方向(<<) 4方向を2ペアで処理 ----
    for (int pair = 0; pair < 4; pair += 2) {
        const uint64_t s[2] = { LSL_SHIFTS[pair], LSL_SHIFTS[pair+1] };
        svuint64_t v_shift = svld1_u64(pg2, s);
 
        const uint64_t m[2] = { EDGE_MASK[LSL_MASKS_IDX[pair]], EDGE_MASK[LSL_MASKS_IDX[pair+1]] };
        svuint64_t v_mask = svld1_u64(pg2, m);
 
        svuint64_t v_opp    = svdup_u64(opp);
        svuint64_t v_my     = svdup_u64(my);
        svuint64_t v_cursor = svdup_u64(placed);
        svuint64_t v_accum  = svdup_u64(0);
 
        for (int step = 0; step < 7; step++) {
            // 1. カーソルを1マス移動
            v_cursor = svand_x(pg2, svlsl_x(pg2, v_cursor, v_shift), v_mask);

            // 2. 自石にヒットしたか判定 (ヒットした場合、それまでに蓄積した accum を採用)
            svbool_t pg_hit = svcmpne_n_u64(pg2, svand_x(pg2, v_cursor, v_my), 0);
            v_flips = svorr_x(pg2, v_flips, svand_z(pg_hit, v_accum, svdup_u64(0xFFFFFFFFFFFFFFFF)));

            // 3. 相手石のマスのみ残し、候補を蓄積
            v_cursor = svand_x(pg2, v_cursor, v_opp);
            v_accum  = svorr_x(pg2, v_accum, v_cursor);
        }
    }
 
    // ---- 右シフト方向(>>) 4方向を2ペアで処理 ----
    for (int pair = 0; pair < 4; pair += 2) {
        const uint64_t s[2] = { LSR_SHIFTS[pair], LSR_SHIFTS[pair+1] };
        svuint64_t v_shift = svld1_u64(pg2, s);
 
        const uint64_t m[2] = { EDGE_MASK[LSR_MASKS_IDX[pair]], EDGE_MASK[LSR_MASKS_IDX[pair+1]] };
        svuint64_t v_mask = svld1_u64(pg2, m);
 
        svuint64_t v_opp    = svdup_u64(opp);
        svuint64_t v_my     = svdup_u64(my);
        svuint64_t v_cursor = svdup_u64(placed);
        svuint64_t v_accum  = svdup_u64(0);
 
        for (int step = 0; step < 7; step++) {
            v_cursor = svand_x(pg2, svlsr_x(pg2, v_cursor, v_shift), v_mask);

            svbool_t pg_hit = svcmpne_n_u64(pg2, svand_x(pg2, v_cursor, v_my), 0);
            v_flips = svorr_x(pg2, v_flips, svand_z(pg_hit, v_accum, svdup_u64(0xFFFFFFFFFFFFFFFF)));

            v_cursor = svand_x(pg2, v_cursor, v_opp);
            v_accum  = svorr_x(pg2, v_accum, v_cursor);
        }
    }
 
    // 全8方向の反転マスクをスカラーに集約
    return svorv_u64(pg2, v_flips);
}
 
int logoutBoard(Logging logging, DISKCOLORS* _board)
{
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			switch (_board[x * 8 + y]) {
			case DISKCOLORS::COLOR_BLACK:
				logging.logprintf(LOGLEVEL_TRACE, "●");
				break;
			case DISKCOLORS::COLOR_WHITE:
				logging.logprintf(LOGLEVEL_TRACE, "○");
				break;
			default:
				logging.logprintf(LOGLEVEL_TRACE, "・");
				break;
			}
		}
		logging.logprintf(LOGLEVEL_TRACE, "\n");
	}

	return 0;
}

int logoutBitBoard(Logging logging, uint64_t my, uint64_t opp, DISKCOLORS currentPlayer)
{
	uint64_t black = (currentPlayer == DISKCOLORS::COLOR_BLACK) ? my : opp;
	uint64_t white = (currentPlayer == DISKCOLORS::COLOR_BLACK) ? opp : my;

	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			uint64_t mask = 1ULL << (y * 8 + x);

			if (black & mask) {
				logging.logprintf(LOGLEVEL_TRACE, "●");
			}
			else if (white & mask) {
				logging.logprintf(LOGLEVEL_TRACE, "○");
			}
			else {
				logging.logprintf(LOGLEVEL_TRACE, "・");
			}
		}
		logging.logprintf(LOGLEVEL_TRACE, "\n");
	}

	return 0;
}

