/**
 * Copyright (C) 2023 EEMBC
 * Copyright (C) 2025 Arm Limited
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

#include <armnn/ArmNN.hpp>
#include <armnnTfLiteParser/ITfLiteParser.hpp>
#include <iostream>
#include <vector>
#include <random>

#define NN_NUM_OUTPUT_BYTES (OUT_DIM)

typedef int8_t input_tensor_t[MFCC_FIFO_BYTES];
typedef int8_t output_tensor_t[NN_NUM_OUTPUT_BYTES];

armnn::BindingPointInfo g_inputBindingInfo, g_outputBindingInfo;

armnn::IRuntime *runtime = nullptr;

armnn::NetworkId networkId;

extern "C"
{
    int armnn_init()
    {
        // Create a runtime object
        armnn::IRuntime::CreationOptions options;

        options.m_EnableGpuProfiling                 = false;
        options.m_ProfilingOptions.m_TimelineEnabled = false;
        options.m_ProfilingOptions.m_EnableProfiling = false;

        armnn::OptimizerOptionsOpaque optimizerOptions;
        optimizerOptions.SetReduceFp32ToFp16(false);
        armnn::BackendOptions cpuAcc(
            "CpuAcc",
            { { "FastMathEnabled", false },
              { "NumberOfThreads", 1 } });
        optimizerOptions.AddModelOption(cpuAcc);

        // armnn::IRuntime
        runtime = armnn::IRuntime::CreateRaw(options);

        // Create a TensorFlow Lite parser
        armnnTfLiteParser::ITfLiteParserPtr parser
            = armnnTfLiteParser::ITfLiteParser::Create();

        // Load the TensorFlow Lite model
        armnn::INetworkPtr network = parser->CreateNetworkFromBinaryFile(
            "../ports/arm/ds_cnn_s_quantized.tflite");

        std::vector<std::string> inputNames
            = parser->GetSubgraphInputTensorNames(0);
        g_inputBindingInfo
            = parser->GetNetworkInputBindingInfo(0, inputNames[0]);

        std::vector<std::string> outputNames
            = parser->GetSubgraphOutputTensorNames(0);
        g_outputBindingInfo
            = parser->GetNetworkOutputBindingInfo(0, outputNames[0]);

        // Optimize the network for CPU
        std::vector<armnn::BackendId> backends = { "CpuAcc", "CpuRef" };
        armnn::IOptimizedNetworkPtr   optimizedNet
            = armnn::Optimize(*network, backends, runtime->GetDeviceSpec());

        // Load the optimized network into the runtime
        runtime->LoadNetwork(networkId, std::move(optimizedNet));

        return EE_STATUS_OK;
    }

    #if defined(DEBUG) || defined(_DEBUG)
    void print_tensor(const int8_t* tensor, size_t size, const std::string& name) {
        std::cout << name << " = [";
        for (size_t i = 0; i < size; ++i) {
            std::cout << static_cast<int>(tensor[i]);
            if (i != size - 1) std::cout << ", ";
        }
        std::cout << "]\n";
    }
    #endif

    int classify_on_armnn(const input_tensor_t in_data,
                          output_tensor_t      out_data)
    {
        const armnn::TensorInfo &inputTensorInfo  = g_inputBindingInfo.second;
        const armnn::TensorInfo &outputTensorInfo = g_outputBindingInfo.second;


        // Create input tensor
        armnn::InputTensors inputTensors { { g_inputBindingInfo.first,
                                             armnn::ConstTensor(inputTensorInfo,
                                                                in_data) } };

        // Prepare output tensor
        armnn::OutputTensors outputTensors { { g_outputBindingInfo.first,
                                               armnn::Tensor(outputTensorInfo,
                                                             out_data) } };

        // Run inference
        armnn::Status status
            = runtime->EnqueueWorkload(networkId, inputTensors, outputTensors);

        #if defined(DEBUG) || defined(_DEBUG)
        print_tensor(in_data, MFCC_FIFO_BYTES, "Input Tensor");
        print_tensor(out_data, NN_NUM_OUTPUT_BYTES, "Output Tensor");
        #endif

        if (status != armnn::Status::Success)
        {
            // Display output data
            std::cout << "ArmNN Error " << std::endl;
            return EE_STATUS_ERROR;
        }

        return EE_STATUS_OK;
    }
}
