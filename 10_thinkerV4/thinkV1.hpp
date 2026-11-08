#pragma once
#include "othello.hpp"
#include "logging.h"
#include "think.hpp"

// Bit for disk character
#define DISKCHARFLAG_EXISTENCE	0x01		// Indicates disk exsits or not
#define DISKCHARFLAG_CHANGABLE	0x02		// Indicates disk can be changed or not

// Parameters for thinker
#define SEARCH_DEPTH_MP				7
#define NUM_FOR_GAMESTATE_END_MP	17
#define SEARCH_DEPTH				6
#define NUM_FOR_GAMESTATE_END		16
//#define SEARCH_DEPTH				7
//#define NUM_FOR_GAMESTATE_END		14
#define NUM_FOR_GAMESTATE_MIDFIELD	48

#define	FIXED_DISK_WEIGHT			32

enum class GAMESTATE {
	GAMESTATE_EARLY_STAGE = 0,
	GAMESTATE_MIDFIELD,
	GAMESTATE_END
};

class ThinkerV1 {
public:
	int SetParams(int turn, DISKCOLORS board[64], THINKARC _thinkArc, int _numThreads, int _depth);
	int think();

private:
	DISKCOLORS board[64];
	struct BitBoard{
		uint64_t my;
		uint64_t opp;
	} bitBoard;
	int turn;
	GAMESTATE thinkerState = GAMESTATE::GAMESTATE_EARLY_STAGE;
	DISKCOLORS currentPlayer, opponent;
	THINKARC thinkArc;
	int numThreads = DEFAULT_NUM_THREADS;
	int depth = DEFAULT_SEARCH_DEPTH;

	int CheckPos[60] = {
		 0, 56,  7, 63,
		26, 34, 43, 44, 29, 37, 19, 20,
		25, 33, 51, 52, 30, 38, 11, 12,
		18, 42, 45, 21,
		16, 40, 58, 61, 23, 47,  2,  5,
		17, 41, 50, 53, 22, 46, 10, 13,
		24, 32, 59, 60, 31, 39,  3,  4,
		 8, 48, 57, 62, 15, 55,  1,  6,
		 9, 49, 14, 54
	};

	// Alpha-Beta探索のMove Ordering用 優先度マスク（角 → 内側 → 辺・Bマス → C打ち → X打ち）
	static constexpr uint64_t MOVE_PRIORITY_MASKS[5] = {
		0x8100000000000081ULL, // [0] 角 (0, 7, 56, 63)
		0x00003C24243C0000ULL, // [1] 中割り・ボックス (18..21, 26, 29, 34, 37, 42..45)
		0x3C3CC3C3C3C33C3CULL, // [2] 辺・A/Bマス
		0x4281000000008142ULL, // [3] C打ち (角の両隣: 1, 6, 8, 15, 48, 55, 57, 62)
		0x0042000000004200ULL  // [4] X打ち (角の斜め隣: 9, 14, 49, 54)
	};

	int weight[2][8][8] = {
		// Weight for Early stage (1 - 16)
		{
			 32,-24, 16,  2,  2, 16,-24, 32,
			-24,-24,-12, -4, -4,-12,-24,-24,
			 16,-12,  4,  1,  1,  4,-12, 16,
			  2, -4,  1,  1,  1,  1, -4,  2,
			  2, -4,  1,  1,  1,  1, -4,  2,
			 16,-12,  4,  1,  1,  4,-12, 16,
			-24,-24,-12, -4, -4,-12,-24,-24,
			 32,-24, 16,  2,  2, 16,-24, 32
		},
		// Weight for Mid stage (17 - 36)
		{
			 32,-24, 16,  2,  2, 16,-24, 32,
			-24,-24,-12, -4, -4,-12,-24,-24,
			 16,-12,  4,  1,  1,  4,-12, 16,
			  2, -4,  1,  1,  1,  1, -4,  2,
			  2, -4,  1,  1,  1,  1, -4,  2,
			 16,-12,  4,  1,  1,  4,-12, 16,
			-24,-24,-12, -4, -4,-12,-24,-24,
			 32,-24, 16,  2,  2, 16,-24, 32
		}
	};

	const uint64_t EDGE_MASK[8] = {
		0xFEFEFEFEFEFEFEFEULL,  // [0] 右(<<1) : h列除外(シフト後にh列が出力先となる)
		0x7F7F7F7F7F7F7F7FULL,  // [1] 左(>>1) : a列除外(シフト後にa列が出力先となる)
		0xFFFFFFFFFFFFFF00ULL,  // [2] 下(<<8) : 1行目除外
		0x00FFFFFFFFFFFFFFULL,  // [3] 上(>>8) : 8行目除外
		0xFEFEFEFEFEFEFE00ULL,  // [4] 右下(<<9): h列+1行目除外
		0x007F7F7F7F7F7F7FULL,  // [5] 左上(>>9): a列+8行目除外
		0x7F7F7F7F7F7F7F00ULL,  // [6] 左下(<<7): a列+1行目除外(<<7の出力先はa列側)
		0x00FEFEFEFEFEFEFEULL,  // [7] 右上(>>7): h列+8行目除外(>>7の出力先はh列側)
	};

	// 左シフト方向(svlsl, <<): 右(+1),下(+8),右下(+9),左下(+7) の4方向
	// Post-Masking: 各方向でシフト後の「はみ出し先」を除外するマスクを使用
	const uint64_t LSL_SHIFTS[4]    = { 1,  8,  9,  7 };
	const uint64_t LSL_MASKS_IDX[4] = { 0,  2,  4,  6 };
	//   右(<<1) → MASK[0](h列除外)  下(<<8) → MASK[2](1行目除外)
	//   右下(<<9)→ MASK[4]         左下(<<7)→ MASK[6](a→h偽ビット除外)
	
	// 右シフト方向(svlsr, >>): 左(-1),上(-8),左上(-9),右上(-7) の4方向
	const uint64_t LSR_SHIFTS[4]    = { 1,  8,  9,  7 };
	const uint64_t LSR_MASKS_IDX[4] = { 1,  3,  5,  7 };
	//   左(>>1) → MASK[1](a列除外)  上(>>8) → MASK[3](8行目除外)
	//   左上(>>9)→ MASK[5]         右上(>>7)→ MASK[7](h→a偽ビット除外)

	int CountDisk(DISKCOLORS color, DISKCOLORS _board[64]);
	int findBestPlaceForCurrentPlayer(int lv);
	int findBestPlaceForCurrentPlayerMP(int lv);
	int MaxLevel(int lv, bool f, int alpha, int beta, uint64_t my, uint64_t opp);
	int MinLevel(int lv, bool f, int alpha, int beta, uint64_t my, uint64_t opp);
	int evcal(uint64_t my, uint64_t opp);

	uint64_t generate_legal_moves_sve2(uint64_t my, uint64_t opp);
	uint64_t flip_discs_sve2(uint64_t my, uint64_t opp, int pos);

	/*
	* popcount_neon(bits)
	*   概要 : NEON CNT命令で64bitビットボードの
	*          1ビット数(立っているビットの個数)を高速カウントする
	*   bits : uint64_t — カウント対象のビットボード
	*   戻値 : int      — 1ビットの個数 (0〜64)
	*/
	inline int popcount_neon(uint64_t bits) {
		uint8x8_t v = vreinterpret_u8_u64(vcreate_u64(bits));
		/*
		* vcnt_u8(a) ← CNT命令
		*   a    : uint8x8_t — 8バイトの入力ベクトル
		*   戻値 : uint8x8_t — 各バイトのビット数を並列カウント
		*/
		uint8x8_t cnt = vcnt_u8(v);
		/*
		* vaddlv_u8(a) ← ADDLV命令
		*   a    : uint8x8_t — CNT結果ベクトル
		*   戻値 : uint16_t  — 全8バイトのビット数の総和
		*/
		return (int)vaddlv_u8(cnt);
	};
};

int logoutBoard(Logging logging, DISKCOLORS* _board);
int logoutBitBoard(Logging logging, uint64_t my, uint64_t opp, DISKCOLORS currentPlayer);

