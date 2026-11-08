#pragma once
#include <vector>
#include "State.hpp"
#include "PTHHandler.hpp"

#define	C_PUCT	1.0

class Node;

typedef	double	Temperature;

typedef struct _CHILD {
	int x, y;
	Node* node;
} Child;

typedef struct _SCORE {
	int x, y;
	int n;
	double probability;
} Score;

class Node {
public:
	Model *model;
	int n;

	Node(Model *_model, State *_state, double _p, bool _isBreadthFirst = false);
	~Node();
	int evaluate(float *result);
	int get_next_child_node(Child** next_child_node);
	int sum_child_nodes(int* result);
	int nodes_to_scores(std::vector<Score> *scores);
private:
	bool initialized = false;
	State state;
	double p = 0.0;
	double w = 0.0;
	std::vector<Child> child_node_list;
	bool isBreadthFirst = false;
};
