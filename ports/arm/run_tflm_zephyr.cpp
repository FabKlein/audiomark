/*
 * Copyright (C) 2026 Arm Limited
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <zephyr/cache.h>

#ifndef restrict
#define restrict __restrict__
#endif

#include "ee_audiomark.h"
#include "ee_api.h"
#include "ee_mfcc_f32.h"

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "BufAttributes.hpp"

#define NN_NUM_OUTPUT_BYTES OUT_DIM

typedef int8_t input_tensor_t[MFCC_FIFO_BYTES];
typedef int8_t output_tensor_t[NN_NUM_OUTPUT_BYTES];

extern "C" {
extern const uint8_t *GetModelPointer(void);
extern size_t GetModelLen(void);
}

namespace {

uint8_t tensor_arena[ACTIVATION_BUF_SZ] ACTIVATION_BUF_ATTRIBUTE;

const tflite::Model *model;
tflite::MicroInterpreter *interpreter;
TfLiteTensor *input_tensor;
TfLiteTensor *output_tensor;

alignas(tflite::MicroInterpreter) uint8_t interpreter_storage[sizeof(tflite::MicroInterpreter)];

} // namespace

extern "C" void ethosu_nn_init(void)
{
	model = tflite::GetModel(GetModelPointer());
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		printf("AudioMark model schema version unsupported: version=%lu, supported=%d\n",
		       static_cast<unsigned long>(model->version()), TFLITE_SCHEMA_VERSION);
		return;
	}

	static tflite::MicroMutableOpResolver<7> resolver;
	resolver.AddReshape();
	resolver.AddAveragePool2D();
	resolver.AddConv2D();
	resolver.AddDepthwiseConv2D();
	resolver.AddFullyConnected();
	resolver.AddSoftmax();
	resolver.AddEthosU();

	interpreter = new (interpreter_storage)
		tflite::MicroInterpreter(model, resolver, tensor_arena, sizeof(tensor_arena));

	if (interpreter->AllocateTensors() != kTfLiteOk) {
		printf("AudioMark TFLM AllocateTensors failed\n");
		interpreter = nullptr;
		return;
	}

	if (interpreter->inputs_size() != 1 || interpreter->outputs_size() != 1) {
		printf("AudioMark model expects 1 input/1 output, got %zu/%zu\n",
		       interpreter->inputs_size(), interpreter->outputs_size());
		interpreter = nullptr;
		return;
	}

	input_tensor = interpreter->input(0);
	output_tensor = interpreter->output(0);

	if (input_tensor->bytes != MFCC_FIFO_BYTES || output_tensor->bytes != NN_NUM_OUTPUT_BYTES) {
		printf("AudioMark tensor size mismatch: input=%zu/%zu output=%zu/%zu\n",
		       input_tensor->bytes, static_cast<size_t>(MFCC_FIFO_BYTES),
		       output_tensor->bytes, static_cast<size_t>(NN_NUM_OUTPUT_BYTES));
		interpreter = nullptr;
		return;
	}

	printf("AudioMark TFLM initialized. model=%zu bytes arena=%zu bytes\n",
	       GetModelLen(), sizeof(tensor_arena));
}

extern "C" int classify_on_ethosu(const input_tensor_t in_data, output_tensor_t out_data)
{
	if (interpreter == nullptr || input_tensor == nullptr || output_tensor == nullptr) {
		return EE_STATUS_ERROR;
	}

	std::memcpy(input_tensor->data.int8, in_data, MFCC_FIFO_BYTES);
	sys_cache_data_flush_range(input_tensor->data.data, input_tensor->bytes);

	//printf("Invoke Ethos start\n");	

	if (interpreter->Invoke() != kTfLiteOk) {
		printf("AudioMark TFLM Invoke failed\n");
		return EE_STATUS_ERROR;
	}
	//printf("Invoke Ethos done\n");	
	sys_cache_data_invd_range(output_tensor->data.data, output_tensor->bytes);
	std::memcpy(out_data, output_tensor->data.int8, NN_NUM_OUTPUT_BYTES);

	return EE_STATUS_OK;
}
