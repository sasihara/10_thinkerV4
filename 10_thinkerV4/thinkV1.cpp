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
	int eval = INT_MIN, score;
	size_t pos;
	bool foundMove = false;

	// 合法手を得る
	uint64_t legal_moves = generate_legal_moves_sve2(bitBoard.my, bitBoard.opp);

	// すべての合法手について
	for(size_t i = 0 ; i < 60; i++) {
		size_t movePos = CheckPos[i];
		if(legal_moves & (1ULL << movePos)) {
			// *** Debug ***
			LOGOUT(LOGLEVEL_TRACE, "  [DEBUG] findBestPlaceForCurrentPlayer: Checking position %zu (x=%zu, y=%zu)", movePos, movePos % 8 + 1, movePos / 8 + 1);

			// ここでmovePos番目の位置に石を置いた場合の評価値を計算する
			uint64_t reverted = flip_discs_sve2(bitBoard.my, bitBoard.opp, movePos);

			uint64_t tmpMy = bitBoard.my | reverted | (1ULL << movePos);
			uint64_t tmpOpp = bitBoard.opp & (~reverted);

			// ** Debug ***
			LOGOUT(LOGLEVEL_TRACE, "  [DEBUG] findBestPlaceForCurrentPlayer: After placing disk at position %zu", movePos);
#ifdef _DEBUG
			logoutBitBoard(logging, tmpMy, tmpOpp, currentPlayer);
#endif

			// 評価値の計算はMaxLevelやMinLevelを使って行う
			score = MinLevel(lv - 1, false, eval, INT_MAX, tmpOpp, tmpMy);

			if (foundMove == false || eval < score) {
				pos = movePos;
				eval = score;
				foundMove = true;
			}
		}
	}

	// 合法手がない場合、適当な位置を返す
	if (foundMove == false) {
		return -11;
	}
	else{
		int res = (int)pos % 8 * 10 + (int)pos / 8;
		// printf("  [DEBUG] findBestPlaceForCurrentPlayer: x=%d, y=%d, res=%d\n", (int)pos % 8, (int)pos / 8, res);
		fflush(stdout);
		return res;
	}
}

int ThinkerV1::findBestPlaceForCurrentPlayerMP(int lv)
{
	int bestEval = INT_MIN;
	int bestPos = -1;
	bool foundMove = false;
	std::atomic<int> alphaAtomic{INT_MIN}; // 各スレッドで共有する暫定ベストスコア（枝刈り用）

	// 使用するスレッド数を設定
	if (this->numThreads > 0) {
		omp_set_num_threads(this->numThreads);
	}

	// 合法手を配列にリストアップ
	uint64_t legal_moves = generate_legal_moves_sve2(bitBoard.my, bitBoard.opp);
	int moveList[64];
	int numMoves = 0;
	for (int i = 0; i < 60; i++) {
		int movePos = CheckPos[i];
		if (legal_moves & (1ULL << movePos)) {
			moveList[numMoves++] = movePos;
		}
	}

	// 合法手がない場合（パス）
	if (numMoves == 0) {
		return -11;
	}

	// 合法手を各スレッドへ動的割り当てして並列探索
#pragma omp parallel for schedule(dynamic)
	for (int idx = 0; idx < numMoves; idx++) {
		int i = moveList[idx];

		// この時点での暫定最高スコアを取得（下限alphaとして枝刈りに利用）
		int currentAlpha = alphaAtomic.load(std::memory_order_relaxed);

		// 石を置いた場合の反転と次盤面の生成
		uint64_t reverted = flip_discs_sve2(bitBoard.my, bitBoard.opp, i);
		uint64_t tmpMy = bitBoard.my | reverted | (1ULL << i);
		uint64_t tmpOpp = bitBoard.opp & (~reverted);

		// 深さ lv - 1 で MinLevel を探索
		int score = MinLevel(lv - 1, false, currentAlpha, INT_MAX, tmpOpp, tmpMy);

		// ベストスコアと着手位置を安全に更新
#pragma omp critical(update_best_mp)
		{
			if (foundMove == false || score > bestEval) {
				bestEval = score;
				bestPos = i;
				foundMove = true;
				alphaAtomic.store(score, std::memory_order_relaxed);
			}
		}
	}

	// 最善手の座標変換とリターン
	int res = (int)bestPos % 8 * 10 + (int)bestPos / 8;
	// printf("  [DEBUG] findBestPlaceForCurrentPlayerMP: x=%d, y=%d, res=%d, score=%d\n", 
	    //    (int)bestPos % 8, (int)bestPos / 8, res, bestEval);
	fflush(stdout);
	return res;
}

//
//	Function Name: MaxLevel
//	Summary: Get the score for the current board for the current player.
//	
//	In:
//		lv:	Depth to search.
//		f:	
//		alpha: 
//		beta:
//		my:
//		opp:
//
//	Return:
//
//int ThinkerV1::MaxLevel(int lv, bool f, int alpha, int beta, uint64_t my, uint64_t opp)
//{
//	if ((my | opp) == ~0ULL) return evcal(my, opp);
//	if (lv == 0) return evcal(my, opp);		// 最深に達した場合は評価値を返す
//
//	bool pf = false;
//	int score;
//	int maxScore = -INT_MAX;
//
//	// 合法手を得る
//	uint64_t legal_moves = generate_legal_moves_sve2(my, opp);
//
//	// すべての合法手について
//	for(size_t i = 0 ; i < 60; i++) {
//		size_t movePos = CheckPos[i];
//		if(legal_moves & (1ULL << movePos)) {
//			pf = true;
//
//			// ここでmovePos番目の位置に石を置いた場合の評価値を計算する
//			uint64_t reverted = flip_discs_sve2(my, opp, movePos);
//
//			uint64_t tmpMy = my | reverted | (1ULL << movePos);
//			uint64_t tmpOpp = opp & (~reverted);
//
//			// 評価値の計算はMaxLevelやMinLevelを使って行う
//			score = MinLevel(lv - 1, false, alpha, beta, tmpOpp, tmpMy);
//
//			if (score > maxScore) {
//				maxScore = score;
//			}
//
//			if (alpha < score) {
//				alpha = score;
//			}
//
//			if (alpha >= beta) return alpha;
//		}
//	}
//
//	if (pf != false) return maxScore;				// 自身に合法手があった場合
//	if (f == true) return evcal(my, opp);			// 自身･相手ともに合法手がない場合は評価値を返す
//	return MinLevel(lv, true, alpha, beta, opp, my);		// 自身に合法手が無い場合は相手の番に移行する
//}

int ThinkerV1::MaxLevel(int lv, bool f, int alpha, int beta, uint64_t my, uint64_t opp)
{
	if ((my | opp) == ~0ULL) return evcal(my, opp);
	if (lv == 0) return evcal(my, opp);		// 最深に達した場合は評価値を返す

	// 合法手を得る
	uint64_t legal_moves = generate_legal_moves_sve2(my, opp);

	// 合法手がない場合（60回ループせず即座に判定）
	if (legal_moves == 0) {
		if (f == true) return evcal(my, opp);			// 自身･相手ともに合法手がない場合は評価値を返す
		return MinLevel(lv, true, alpha, beta, opp, my);	// 自身に合法手が無い場合は相手の番に移行する
	}

	int score;
	int maxScore = -INT_MAX;
	uint64_t remaining = legal_moves;

	// 優先度マスク順に、立っているビット（合法手）のみを走査
	for (int g = 0; g < 5; g++) {
		uint64_t moves = remaining & MOVE_PRIORITY_MASKS[g];
		while (moves) {
			int movePos = __builtin_ctzll(moves);	// 最下位の1ビットの位置を取得
			moves &= moves - 1;			// 最下位の1ビットをクリア

			// ここでmovePos番目の位置に石を置いた場合の評価値を計算する
			uint64_t reverted = flip_discs_sve2(my, opp, movePos);

			uint64_t tmpMy = my | reverted | (1ULL << movePos);
			uint64_t tmpOpp = opp & (~reverted);

			// 評価値の計算はMaxLevelやMinLevelを使って行う
			score = MinLevel(lv - 1, false, alpha, beta, tmpOpp, tmpMy);

			if (score > maxScore) {
				maxScore = score;
			}

			if (alpha < score) {
				alpha = score;
			}

			if (alpha >= beta) return alpha;
		}
		remaining &= ~MOVE_PRIORITY_MASKS[g];
		if (remaining == 0) break;
	}

	return maxScore;
}

//
//	Function Name: MinLevel
//	Summary: Get the score for the specified board for the the opponent of the current player.
//	
//	In:
//		lv: 
//		f: 
//		alpha: 
//		beta:
//		my:
//		opp:
//
//	Return:
//		Score.
//
//int ThinkerV1::MinLevel(int lv, bool f, int alpha, int beta, uint64_t my, uint64_t opp)
//{
//	if ((my | opp) == ~0ULL) return evcal(opp, my);
//	if (lv == 0) return evcal(opp, my);
//
//	bool pf = false;
//	int score;
//	int minScore = INT_MAX;
//
//	// 合法手を得る
//	uint64_t legal_moves = generate_legal_moves_sve2(my, opp);
//
//	// すべての合法手について
//	for(size_t i = 0 ; i < 60; i++) {
//		size_t movePos = CheckPos[i];
//		if(legal_moves & (1ULL << movePos)) {
//			pf = true;
//
//			// ここでmovePos番目の位置に石を置いた場合の評価値を計算する
//			uint64_t reverted = flip_discs_sve2(my, opp, movePos);
//
//			uint64_t tmpMy = my | reverted | (1ULL << movePos);
//			uint64_t tmpOpp = opp & (~reverted);
//
//			// 評価値の計算はMaxLevelやMinLevelを使って行う
//			score = MaxLevel(lv - 1, false, alpha, beta, tmpOpp, tmpMy);
//
//			if (score < minScore) {
//				minScore = score;
//			}
//
//			if (beta > score) {
//				beta = score;
//			}
//
//			if (beta <= alpha) return beta;
//		}
//	}
//
//	if (pf != false) return minScore;				// 自身に合法手があった場合
//	if (f == true) return evcal(opp, my);			// 自身･相手ともに合法手がない場合は評価値を返す
//	return MaxLevel(lv, true, alpha, beta, opp, my);		// 自身に合法手が無い場合は相手の番に移行する
//}

int ThinkerV1::MinLevel(int lv, bool f, int alpha, int beta, uint64_t my, uint64_t opp)
{
	if ((my | opp) == ~0ULL) return evcal(opp, my);
	if (lv == 0) return evcal(opp, my);

	// 合法手を得る
	uint64_t legal_moves = generate_legal_moves_sve2(my, opp);

	// 合法手がない場合（60回ループせず即座に判定）
	if (legal_moves == 0) {
		if (f == true) return evcal(opp, my);			// 自身･相手ともに合法手がない場合は評価値を返す
		return MaxLevel(lv, true, alpha, beta, opp, my);	// 自身に合法手が無い場合は相手の番に移行する
	}

	int score;
	int minScore = INT_MAX;
	uint64_t remaining = legal_moves;

	// 優先度マスク順に、立っているビット（合法手）のみを走査
	for (int g = 0; g < 5; g++) {
		uint64_t moves = remaining & MOVE_PRIORITY_MASKS[g];
		while (moves) {
			int movePos = __builtin_ctzll(moves);	// 最下位の1ビットの位置を取得
			moves &= moves - 1;			// 最下位の1ビットをクリア

			// ここでmovePos番目の位置に石を置いた場合の評価値を計算する
			uint64_t reverted = flip_discs_sve2(my, opp, movePos);

			uint64_t tmpMy = my | reverted | (1ULL << movePos);
			uint64_t tmpOpp = opp & (~reverted);

			// 評価値の計算はMaxLevelやMinLevelを使って行う
			score = MaxLevel(lv - 1, false, alpha, beta, tmpOpp, tmpMy);

			if (score < minScore) {
				minScore = score;
			}

			if (beta > score) {
				beta = score;
			}

			if (beta <= alpha) return beta;
		}
		remaining &= ~MOVE_PRIORITY_MASKS[g];
		if (remaining == 0) break;
	}

	return minScore;
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

