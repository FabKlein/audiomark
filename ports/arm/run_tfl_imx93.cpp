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

extern    "C"
{
#include "ee_audiomark.h"
#include "ee_api.h"
#include "ee_mfcc_f32.h"
#include "ee_nn.h"
}

#include <iostream>
#include <vector>

#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include "tensorflow/lite/delegates/external/external_delegate.h"

#include <filesystem>

#define TFLITE_FILE           "ds_cnn_s_quantized_vela_imx93.tflite"
#define TFLITE_U65_DELEGATE  "./libethosu_delegate_imx93.so"

#define NN_NUM_OUTPUT_BYTES (OUT_DIM)

typedef int8_t input_tensor_t[MFCC_FIFO_BYTES];
typedef int8_t output_tensor_t[NN_NUM_OUTPUT_BYTES];

class     TFLiteModel
{
 public:
    TFLiteModel():interpreter(nullptr)
    {
    }

    int       Init(const char *model_path)
    {

        std::string model_path_str = model_path;

        // Check if the model file exists in the specified path
        if (!std::filesystem::exists(model_path_str))
        {
            // If not, check in the current directory
            std::string current_dir_model_path = "./" TFLITE_FILE;
            if (std::filesystem::exists(current_dir_model_path))
            {
                model_path_str = current_dir_model_path;
            } else
            {
                std::cerr << "Failed to find model in both specified and " "current directories." << std::endl;
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
        tflite::InterpreterBuilder(*model, resolver) (&interpreter);
        if (!interpreter)
        {
            std::cerr << "Failed to create interpreter." << std::endl;
            return EE_STATUS_ERROR;
        }
        // Optional external delegate
        std::unique_ptr < TfLiteDelegate, void (*) (TfLiteDelegate *) >
            ext_delegate(nullptr, TfLiteExternalDelegateDelete);

        const char *delegate_path = TFLITE_U65_DELEGATE;

        if (delegate_path && delegate_path[0] != '\0')
        {
            TfLiteExternalDelegateOptions opts = TfLiteExternalDelegateOptionsDefault(delegate_path);

            ext_delegate.reset(TfLiteExternalDelegateCreate(&opts));
            if (!ext_delegate)
            {
                std::cerr << "failed to create external delegate from " << delegate_path << "\n";
            } else
            {
                if (interpreter->ModifyGraphWithDelegate(ext_delegate.get()) != kTfLiteOk)
                {
                    std::cerr << "failed to apply external delegate; continuing on CPU\n";
                    ext_delegate.reset(nullptr);
                } else
                {
                    std::cout << "applied external delegate: " << delegate_path << "\n";
                }
            }

        }
        // Allocate tensor buffers
        if (interpreter->AllocateTensors() != kTfLiteOk)
        {
            std::cerr << "Failed to allocate tensors." << std::endl;
            return EE_STATUS_ERROR;
        }

        return EE_STATUS_OK;
    }

    int       Classify(const int8_t * in_data, int8_t * out_data)
    {
        //printf("classify : \n");
        if (!interpreter)
        {
            std::cerr << "Interpreter is not initialized." << std::endl;
            return EE_STATUS_ERROR;
        }
        // Fill input tensor with provided data
        int       input_index = interpreter->inputs()[0];
        TfLiteTensor *input_tensor = interpreter->tensor(input_index);
        if (!input_tensor)
        {
            std::cerr << "Failed to get input tensor." << std::endl;
            return EE_STATUS_ERROR;
        }
        // Copy input data to the input tensor
        memcpy(input_tensor->data.int8, in_data, input_tensor->bytes);

        // Run inference
        if (interpreter->Invoke() != kTfLiteOk)
        {
            std::cerr << "Failed to invoke interpreter." << std::endl;
            return EE_STATUS_ERROR;
        }
        // Get output tensor
        int       output_index = interpreter->outputs()[0];
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
    std::unique_ptr < tflite::FlatBufferModel > model;
    std::unique_ptr < tflite::Interpreter > interpreter;
};

// Global instance of the TFLiteModel class
TFLiteModel tflite_model;

extern    "C"
{

    int       tflite_nn_init(void)
    {
        return tflite_model.Init("../ports/arm/" TFLITE_FILE);
    }

    int       classify_on_tflite(const int8_t * in_data, int8_t * out_data)
    {
        return tflite_model.Classify(in_data, out_data);
    }

}
