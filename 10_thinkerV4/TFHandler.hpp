#pragma once
#include "externalThinkerMessages.hpp"
#include "State.hpp"
//#include "tensorflow\c\c_api.h"

#define DN_OUTPUT_SIZE 65

typedef struct _Model {
	TF_Graph* Graph;
	TF_Status* Status;
	TF_SessionOptions* SessionOpts;
	TF_Buffer* RunOpts;
	TF_Session* Session;
} Model;

int load_model(Model* model);
int free_model(Model* model);
int predict(Model *model, State _state, float* _policies, float* _value);
void NoOpDeallocator(void* data, size_t a, void* b);
