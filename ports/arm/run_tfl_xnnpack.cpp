/**
 * Copyright (C) 2023 EEMBC
 * Copyright (C) 2024 Arm Limited
 *
 * All EEMBC Benchmark Software are products of EEMBC and are provided under the
 * terms of the EEMBC Benchmark License Agreements. The EEMBC Benchmark Software
 * are proprietary intellectual properties of EEMBC and its Members and is
 * protected under all applicable laws, including all applicable copyright laws.
 *
 */

#define restrict __restrict__

extern "C"
{
#include "ee_audiomark.h"
#include "ee_api.h"
#include "ee_mfcc_f32.h"
#include "ee_nn.h"
}

#include <iostream>
#include <cstdint>
#include <vector>
#if defined(AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS)
#include <time.h>
#endif

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
//#include <tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h>

#include <filesystem>

// #include <random>

#define NN_NUM_OUTPUT_BYTES (OUT_DIM)

typedef int8_t input_tensor_t[MFCC_FIFO_BYTES];
typedef int8_t output_tensor_t[NN_NUM_OUTPUT_BYTES];

static uint64_t last_invoke_cycles;

static inline uint64_t
read_aarch64_counter()
{
#if defined(AUDIOMARK_ARM_PROFILE_COUNTER_LINUX_NS)
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
#elif defined(__aarch64__)
    uint64_t value;

    __asm__ volatile("isb\n"
                     "mrs %0, cntvct_el0\n"
                     "isb"
                     : "=r"(value)
                     :
                     : "memory");
    return value;
#else
    return 0;
#endif
}

class TFLiteModel
{
public:
    TFLiteModel()
        : interpreter(nullptr)
    {
    }

    int Init(const char *model_path)
    {

        std::string model_path_str = model_path;

        // Check if the model file exists in the specified path
        if (!std::filesystem::exists(model_path_str))
        {
            // If not, check in the current directory
            std::string current_dir_model_path = "./ds_cnn_s_quantized.tflite";
            if (std::filesystem::exists(current_dir_model_path))
            {
                model_path_str = current_dir_model_path;
            }
            else
            {
                std::cerr << "Failed to find model in both specified and "
                             "current directories."
                          << std::endl;
                return EE_STATUS_ERROR;
            }
        }

        // Load the model
        model = tflite::FlatBufferModel::BuildFromFile(model_path_str.c_str());
        if (!model)
        {
            std::cerr << "Failed to load model." << std::endl;
            return EE_STATUS_ERROR;
        }

        // Build the interpreter
        tflite::ops::builtin::BuiltinOpResolver resolver;
        tflite::InterpreterBuilder(*model, resolver)(&interpreter);
        if (!interpreter)
        {
            std::cerr << "Failed to create interpreter." << std::endl;
            return EE_STATUS_ERROR;
        }
#if 0
        TfLiteXNNPackDelegateWeightsCache *weights_cache
            = TfLiteXNNPackDelegateWeightsCacheCreate();

        // Create and add the XNNPACK delegate
        TfLiteXNNPackDelegateOptions xnnpack_options
            = TfLiteXNNPackDelegateOptionsDefault();

        xnnpack_options.flags = 0;
        printf("xnnpack_options %x\n",xnnpack_options.flags);
        //xnnpack_options.flags |= TFLITE_XNNPACK_DELEGATE_FLAG_QS8
        //                         | TFLITE_XNNPACK_DELEGATE_FLAG_QU8;

        xnnpack_options.weights_cache = weights_cache;

        auto *xnnpack_delegate = TfLiteXNNPackDelegateCreate(&xnnpack_options);
        if (interpreter->ModifyGraphWithDelegate(xnnpack_delegate) != kTfLiteOk)
        {
            std::cerr << "Failed to apply XNNPACK delegate." << std::endl;
            return EE_STATUS_ERROR;
        }

        TfLiteXNNPackDelegateWeightsCacheFinalizeHard(weights_cache);
#endif
        // Allocate tensor buffers
        if (interpreter->AllocateTensors() != kTfLiteOk)
        {
            std::cerr << "Failed to allocate tensors." << std::endl;
            return EE_STATUS_ERROR;
        }


        return EE_STATUS_OK;
    }

    int Classify(const int8_t *in_data, int8_t *out_data)
    {
        uint64_t invoke_begin;
        uint64_t invoke_end;

        last_invoke_cycles = 0;

        if (!interpreter)
        {
            std::cerr << "Interpreter is not initialized." << std::endl;
            return EE_STATUS_ERROR;
        }

        // Fill input tensor with provided data
        int           input_index  = interpreter->inputs()[0];
        TfLiteTensor *input_tensor = interpreter->tensor(input_index);
        if (!input_tensor)
        {
            std::cerr << "Failed to get input tensor." << std::endl;
            return EE_STATUS_ERROR;
        }

        // Copy input data to the input tensor
        memcpy(input_tensor->data.int8, in_data, input_tensor->bytes);

        // Run inference
        invoke_begin = read_aarch64_counter();
        if (interpreter->Invoke() != kTfLiteOk)
        {
            std::cerr << "Failed to invoke interpreter." << std::endl;
            return EE_STATUS_ERROR;
        }
        invoke_end = read_aarch64_counter();
        last_invoke_cycles = invoke_end - invoke_begin;

        // Get output tensor
        int           output_index  = interpreter->outputs()[0];
        TfLiteTensor *output_tensor = interpreter->tensor(output_index);
        if (!output_tensor)
        {
            std::cerr << "Failed to get output tensor." << std::endl;
            return EE_STATUS_ERROR;
        }

        // Copy output data from the output tensor
        memcpy(out_data, output_tensor->data.int8, output_tensor->bytes);

        return EE_STATUS_OK;
    }

private:
    std::unique_ptr<tflite::FlatBufferModel> model;
    std::unique_ptr<tflite::Interpreter>     interpreter;
};

// Global instance of the TFLiteModel class
TFLiteModel tflite_model;

extern "C"
{

    int tflite_nn_init(void)
    {
        return tflite_model.Init("../ports/arm/ds_cnn_s_quantized.tflite");
    }

    int classify_on_tflite(const int8_t *in_data, int8_t *out_data)
    {
        return tflite_model.Classify(in_data, out_data);
    }

    void tflite_reset_last_invoke_cycles(void)
    {
        last_invoke_cycles = 0;
    }

    uint64_t tflite_last_invoke_cycles(void)
    {
        return last_invoke_cycles;
    }

}
