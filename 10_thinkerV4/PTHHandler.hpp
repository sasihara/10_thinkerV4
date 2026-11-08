#pragma once
#include <torch/script.h>
#include <torch/torch.h>
#include <memory>
#include "externalThinkerMessages.hpp"
#include "State.hpp"

#define DN_OUTPUT_SIZE 65

enum class RunningMode {
	RUNNINGMODE_AUTO = 0,
	RUNNINGMODE_CPU,
	RUNNINGMODE_GPU
};

typedef struct _Model {
	std::shared_ptr<torch::jit::script::Module> module;
	torch::Device device = torch::kCPU; // ここに保存しておく
} Model;

int load_model(Model* model, RunningMode _runningMode, int gpuid = -1);
int free_model(Model* model);
int predict(Model* model, const State& _state, float* _policies, float* _value);