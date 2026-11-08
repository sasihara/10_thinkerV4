#pragma once
#include "State.hpp"
#include "Node.hpp"
//#include "history.hpp"

#ifndef PV_EVALUATE_COUNT
#define PV_EVALUATE_COUNT 2000
#endif

typedef double	Temperature;
typedef int		Action;

class Pv_mcts_action {
public:
	Pv_mcts_action(Model* _model, Temperature _temperature, int _numIterations = PV_EVALUATE_COUNT, bool _isBreadthFirst = false);
	int run(State *state, Action* action, GameId gameId);
	int pv_mcts_scores(State *state, std::vector<Score>* scores);
	int ranom_choice(State *state, std::vector<Score> scores, Action *action);

private:
	Model *model;
	Temperature temperature;
	int numIterations = PV_EVALUATE_COUNT;
	bool isBreadthFirst = false;
	//History history;

	int bolzman(std::vector<Score>* scores, Temperature temperature);
};

bool compare(Score a, Score b);
