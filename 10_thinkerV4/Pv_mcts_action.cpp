#include <float.h>
#include <algorithm>
#include "common.h"
#include "Pv_mcts.action.hpp"
//#include "TFHandler.hpp"
#include "PTHHandler.hpp"
#include "Node.hpp"
#include "logging.h"
#include "history.hpp"

// Global
extern Logging logging;
extern History history;

Pv_mcts_action::Pv_mcts_action(Model* _model, Temperature _temperature, int _numIterations, bool _isBreadthFirst)
{
	model = _model;
	temperature = _temperature;
	numIterations = _numIterations;
	isBreadthFirst = _isBreadthFirst;
}

int Pv_mcts_action::run(State* state, Action* action, GameId gameId)
{
	int ret;
	std::vector<Score> scores;

	LOGOUT(LOGLEVEL_TRACE, "========== Pv_mcts_action::run()開�?. ==========");

	// 盤面のログ出�?
	state->logout(logging);

	// pv_mcts_scores()
	ret = pv_mcts_scores(state, &scores);

	// 戻り値のチェ�?ク
	if (ret < 0) {
		return -1;
	}

	// 学習データの保�?
	ret = history.add(gameId, state->currentPlayer, state->board, scores);
	if (ret < 0) {
		return -2;
	}

	// np.random.choice
	if (scores.size() > 0) {
		// 合法手が存在する場�?
		ret = ranom_choice(state, scores, action);

		// 戻り値のチェ�?ク
		if (ret < 0) {
			return -3;
		}
	}
	else {
		// 合法手が存在しなかった�?��?
		// パスを示すx=8, y=0をセ�?�?
		*action = 80;
	}

	// ログ出�?
	logging.logout("現在の盤面は以下�?�通りで�?.");
	state->logout(logging);

	LOGOUT(LOGLEVEL_TRACE, "========== Pv_mcts_action::run()終�?. ==========");
	// printf("  [DEBUG] Pv_mcts_action::run selected action = %d\n", (int)*action);

	// リターン
	return 0;
}

int Pv_mcts_action::pv_mcts_scores(State* state, std::vector<Score>* scores)
{
	int ret;

	LOGOUT(LOGLEVEL_TRACE, "pv_mcts_scores()開�?.");

	Node* root_node;
	
	try {
		root_node = new Node(model, state, 0.0, isBreadthFirst);
	}
	catch (...) {
		return -1;
	}

	try {
		for (int i = 0; i < numIterations; i++) {
			LOGOUT(LOGLEVEL_TRACE, "===== �?%d回目の評価を開�?.�?=====", i + 1);

			float value;
			ret = root_node->evaluate(&value);
			if (ret < 0) throw -2;
		}

		//scores = nodes_to_scores(root_node.child_nodes)
		LOGOUT(LOGLEVEL_TRACE, "�?合法手の回数のリスト化を開�?.");
		ret = root_node->nodes_to_scores(scores);
		if (ret < 0) throw -3;
	}
	catch (int ret) {
		delete root_node;
		return ret;
	}

	// root_nodeと子ノード�?�メモリ解放
	delete root_node;

	// 合法手有無のチェ�?ク
	// 合法手が無�?場合�?�scoresリスト�?�サイズは0とな�?
	if (scores->size() > 0) {
		//	if temperature == 0:
		LOGOUT(LOGLEVEL_TRACE, "温度を�?慮しつつ�?合法手の選択確�?を設定。温度=%.1f.", temperature);
		if (temperature <= DBL_EPSILON) {
			LOGOUT(LOGLEVEL_TRACE, "温度=0なので、最大スコアの合法手の選択確�?=1としま�?.", temperature);

			// nが最大となるノードを見つける
			int max_n = INT_MIN;
			bool isFound = false;
			size_t max_i = 0;

			//action = np.argmax(scores)
			//	scores = np.zeros(len(scores))
			//	scores[action] = 1
			for (size_t i = 0; i < scores->size(); i++) {
				// こ�?�時点でとりあえずすべての要�?のprobabilityに0をセ�?トしておく
				scores->at(i).probability = 0.0;

				if (scores->at(i).n >= max_n || isFound == false) {
					max_i = i;
					max_n = scores->at(i).n;
					isFound = true;
				}
			}

			// 最大となったノード�?�probability�?1にセ�?�?
			if (isFound == true) {
				scores->at(max_i).probability = 1.0;
			}
			else {
				// こ�?�条件が満たされることは無�?が念のため
				return -4;
			}
		}
		else {
			LOGOUT(LOGLEVEL_TRACE, "温度�?0なので、�?�ル�?マン�?�?に従って�?合法手の選択確�?をセ�?トしま�?.", temperature);

			//scores = boltzman(scores, temperature)
			ret = bolzman(scores, temperature);
			if (ret < 0) {
				return -5;
			}
		}
	}

	LOGOUT(LOGLEVEL_TRACE, "pv_mcts_scores()終�?.");

	//	return scores
	return 0;
}

int Pv_mcts_action::bolzman(std::vector<Score>* scores, Temperature temperature)
{
	LOGOUT(LOGLEVEL_TRACE, "bolzman()開�?. temperature=%.6f", temperature);

	double sum = 0.0;

	LOGOUT(LOGLEVEL_TRACE, "bolzman()開�?.");
	LOGOUT(LOGLEVEL_TRACE, "----- 回数→�?�ル�?マン�?�?値変換 -----");
	//xs = [x * *(1 / temperature) for x in xs]
	for (size_t i = 0; i < scores->size(); i++) {
		scores->at(i).probability = pow((double)scores->at(i).n, 1.0 / temperature);
		LOGOUT(LOGLEVEL_INFO, "%d: (x =%d, y = %d) %.6f => %.6f", i + 1, scores->at(i).x, scores->at(i).y, (double)scores->at(i).n, scores->at(i).probability);

		sum += scores->at(i).probability;
	}

	//return[x / sum(xs) for x in xs]
	LOGOUT(LOGLEVEL_TRACE, "----- ボル�?マン�?�?値を合�?1の値に正規化 -----");
	for (size_t i = 0; i < scores->size(); i++) {
		scores->at(i).probability = scores->at(i).probability / sum;
		LOGOUT(LOGLEVEL_TRACE, "%d: (x =%d, y = %d) => %.6f", i + 1, scores->at(i).x, scores->at(i).y, scores->at(i).probability);
	}

	LOGOUT(LOGLEVEL_TRACE, "bolzman()終�?.");
	return 0;
}

//
//	関数�?: ranom_choice
//	概�?: �?定したスコアに比例した確�?�?�?に従って次の一手を選択す�?
//	
//	入�?:
//		state	現在のス�?ー�?(盤面・次の石の色)
//		scores	選択する確�?�?�?
//
//	出�?:
//		action	選択した次の�?
//
//	実�?方�?:
//		�?合法手ごとに乱数を生成し、その値にスコア値(=確�?�?�?)を乗じる。その乗じた結果の中で最大の合法手を選択する�?
// 
int Pv_mcts_action::ranom_choice(State *state, std::vector<Score> scores, Action* action)
{
	LOGOUT(LOGLEVEL_TRACE, "ranom_choice()開�?.");

	// scoreの�?容をコピ�?�する
	std::vector<Score> scoresSorted = scores;
	
	// scoresのprobabilityの大きい�?にソートす�?
	// 別にソートしなくてもprobability値の確�?でそ�?�手が選択されるから、この処�?は�?須でな�?ことに注�?
	std::sort(scoresSorted.begin(), scoresSorted.end(), compare);
	
	// 0?�?1までの乱数を生成す�?
	double randomValue = (double)rand() / (double)RAND_MAX;

	LOGOUT(LOGLEVEL_TRACE, "乱数値 = %.6f", randomValue);

	// 確�?�?�?の値を�?��?�から�?算し乱数値を�?えた手前の値を選択す�?
	double sum = 0.0;
	size_t x = 0, y = 0;

	for (size_t i = 0; i < scoresSorted.size(); i++) {
		sum += scoresSorted[i].probability;

		// 合計値が乱数値以上となった�?�合�?�そ�?�値を選�?
		// 確�?�?�?の合計値は�?ず最終的に乱数値の最大値の1.0となる�?�で、�?ず以下�?�if�?が実行される
		if (sum >= randomValue - DBL_EPSILON) {
			x = scoresSorted[i].x;
			y = scoresSorted[i].y;

			LOGOUT(LOGLEVEL_TRACE, "%d番目の手を選択�?", i + 1);

			*action = (int)x * 10 + (int)y;

			LOGOUT(LOGLEVEL_TRACE, "ranom_choice()終�?. 打ち手�?�x = %d, y = %d で�?.", x, y);
			return 0;
		}
	}

	// 確�?�?�?の合計値�?1.0未満で本来はあり得な�?。この場合�?�エラーとして返す�?
	LOGOUT(LOGLEVEL_TRACE, "[ERROR] 入力された確�?�?�?の積�??値�?1より小さ�?です。積�??値 = %f�?", sum);
	LOGOUT(LOGLEVEL_TRACE, "ranom_choice()終�?.");
	return -1;
}

bool compare(Score a, Score b)
{
	return(a.probability > b.probability);
}