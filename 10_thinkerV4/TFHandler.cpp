#include <stdio.h>
#include <cmath>
#include "tensorflow\c\c_api.h"
#include "externalThinkerMessages.hpp"
#include "TFHandler.hpp"
#include "thinkV1.hpp"

extern Logging logging;

int load_model(Model *model)
{
	// Load Othello Deep Model
	model->Graph = TF_NewGraph();
	model->Status = TF_NewStatus();

	model->SessionOpts = TF_NewSessionOptions();
	model->RunOpts = NULL;

	// Load Othello Deep Model
	const char* saved_model_dir = "04_OthelloDeepModel/best";
	const char* tags = "serve";
	int ntags = 1;

	model->Session = TF_LoadSessionFromSavedModel(model->SessionOpts, model->RunOpts, saved_model_dir, &tags, ntags, model->Graph, NULL, model->Status);

	if (TF_GetCode(model->Status) != TF_OK)
	{
		printf("\n[ERROR] While loading TensorFlow saved model, got: %s\n", TF_Message(model->Status));
		return -1;
	}
	else
	{
#ifdef _DEBUG
		printf("\n[info] While loading TensorFlow saved model, got: %s\n", TF_Message(model->Status));
#endif
	}

	return 0;
}

int free_model(Model *model)
{
	TF_DeleteGraph(model->Graph);
	TF_DeleteSession(model->Session, model->Status);
	TF_DeleteSessionOptions(model->SessionOpts);
	TF_DeleteStatus(model->Status);

	return 0;
}

// ---------------------------------------------------------------------------
//	predict
//		This function is correspond to the predict function in tensorflow
// 
// ---------------------------------------------------------------------------
int predict(Model *model, State _state, float* _policies, float* _value)
{
	int ret = 0;

	LOGOUT(LOGLEVEL_TRACE, "predict() start.");
	logging.logprintf("プレイヤーの石の色: %s\n", _state.currentPlayer == DISKCOLORS::COLOR_BLACK ? "●" : _state.currentPlayer == DISKCOLORS::COLOR_WHITE ? "○" : "");
	logging.logprintf("対戦相手の石の色: %s\n", _state.opponent == DISKCOLORS::COLOR_BLACK ? "●" : _state.opponent == DISKCOLORS::COLOR_WHITE ? "○" : "");
	ret = logoutBoard(logging, _state.board);

	// Define a input for the othello inference model
	int NumInputs = 1;
	TF_Output* Input = (TF_Output*)malloc(sizeof(TF_Output) * NumInputs);
	if (Input == NULL) return -2;

	TF_Output t0 = { TF_GraphOperationByName(model->Graph, "serving_default_input_1"), 0 };

	if (t0.oper == NULL) {
		printf("ERROR: Failed TF_GraphOperationByName serving_default_input_1\n");
	}
	else {
//#ifdef _DEBUG
//		printf("TF_GraphOperationByName serving_default_input_1 is OK\n");
//#endif
	}

	Input[0] = t0;

	// Define outputs for the othello inference model
	int NumOutputs = 2;
	TF_Output* Output = (TF_Output*)malloc(sizeof(TF_Output) * NumOutputs);
	if (Output == NULL) return -3;

	TF_Output t1 = { TF_GraphOperationByName(model->Graph, "StatefulPartitionedCall"), 0 };

	if (t1.oper == NULL)
		printf("ERROR: Failed TF_GraphOperationByName StatefulPartitionedCall\n");
	else {
//#ifdef _DEBUG
//		printf("TF_GraphOperationByName StatefulPartitionedCall is OK\n");
//#endif
	}

	TF_Output t2 = { TF_GraphOperationByName(model->Graph, "StatefulPartitionedCall"), 1 };

	if (t2.oper == NULL)
		printf("ERROR: Failed TF_GraphOperationByName StatefulPartitionedCall\n");
	else {
//#ifdef _DEBUG
//		printf("TF_GraphOperationByName StatefulPartitionedCall is OK\n");
//#endif
	}

	Output[0] = t1;
	Output[1] = t2;

	// Set input data
	TF_Tensor** InputValues = (TF_Tensor**)malloc(sizeof(TF_Tensor*) * NumInputs);
	if (InputValues == NULL) return -4;

	TF_Tensor** OutputValues = (TF_Tensor**)malloc(sizeof(TF_Tensor*) * NumOutputs);
	if (OutputValues == NULL) return -5;

	int in1_ndims = 4;
	int64_t in1_dims[] = { 1, 8, 8, 2 };
	float_t in1_data[128];
	int in1_ndata = sizeof(in1_data);

	// Set boad information to in1_data
	for (size_t i = 0; i < 128; i++) {
		if (i % 2 == 0) {
			// Set current player's dice information
			if (_state.board[i / 2] == _state.currentPlayer) in1_data[i] = 1.0;
			else in1_data[i] = 0.0;
		}
		else {
			// Set opponent's dice information
			if (_state.board[i / 2] == _state.opponent) in1_data[i] = 1.0;
			else in1_data[i] = 0.0;
		}

		if(i % 2 == 0) logging.logprintf("[%.0f,", in1_data[i]);
		else logging.logprintf("%.0f],", in1_data[i]);
		if (i % 16 == 15) logging.logprintf("\n");
	}
	logging.logprintf("\n");

	TF_Tensor* tensor0 = TF_NewTensor(TF_FLOAT, in1_dims, in1_ndims, in1_data, in1_ndata, &NoOpDeallocator, NULL);

	// InputValues[0] = tensor0;
	if (tensor0 == NULL) {
		printf("[ERROR] Failed to create tensor for input.\n");
	}
	else {
		InputValues[0] = tensor0;
	}

	// Inference
	TF_SessionRun(model->Session, NULL, Input, InputValues, NumInputs, Output, OutputValues, NumOutputs, NULL, 0, NULL, model->Status);

	if (TF_GetCode(model->Status) == TF_OK)
	{
//#ifdef _DEBUG
//		printf("Session is OK\n");
//#endif
	}
	else
	{
		printf("%s", TF_Message(model->Status));
	}

	// Get outputs
	float* result1 = (float*)TF_TensorData(OutputValues[0]);
	float* result2 = (float*)TF_TensorData(OutputValues[1]);

	// Get policies from result1
	for (int i = 0; i < DN_OUTPUT_SIZE; i++) {
		_policies[i] = result1[i];
	}

	*_value = result2[0];

	// free memory
	TF_DeleteTensor(tensor0);
	free(Input);
	free(InputValues);

	TF_DeleteTensor(OutputValues[0]);
	TF_DeleteTensor(OutputValues[1]);
	free(Output);
	free(OutputValues);

	logging.logprintf("value: %f\n", *_value);
	LOGOUT(LOGLEVEL_TRACE, "predict() finished.");
	return ret;
}

void NoOpDeallocator(void* data, size_t a, void* b)
{
//#ifdef _DEBUG
//	printf("Deallocator called with data %p\n", data);
//#endif
}

