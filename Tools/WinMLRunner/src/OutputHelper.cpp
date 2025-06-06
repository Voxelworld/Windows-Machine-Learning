#include "Common.h"
#include "CommandLineArgs.h"
#include <fstream>
#include <ctime>
#include <locale>
#include <utility>
#include <codecvt>
#include <iomanip>
#include <dxgi.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <filesystem>
#include <queue>
#include "TimerHelper.h"
#include "LearningModelDeviceHelper.h"
#include "OutputHelper.h"

#ifdef USE_WINML_NUGET
using namespace winrt::Microsoft::AI::MachineLearning;
#else
using namespace winrt::Windows::AI::MachineLearning;
#endif
using namespace winrt::Windows::Storage::Streams;
using namespace ::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace DirectX::PackedVector;

std::string OutputHelper::ToString(std::wstring s) 
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.to_bytes(s);
}

std::wstring OutputHelper::ToWString(std::string s) 
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.from_bytes(s);
}

// Returns a string like "[1, 10, 20, 30, 1]"
std::string OutputHelper::ToString(const std::vector<int64_t>& shape)
{
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < shape.size(); ++i)
    {
        ss << shape[i];
        if (i + 1 < shape.size())
            ss << ", ";
    }
    ss << "]";
    return ss.str();
}

void OutputHelper::PrintLoadingInfo(const std::wstring& modelPath) const
{
    wprintf(L"Loading model (path = %s)...\n", modelPath.c_str());
}

void OutputHelper::PrintBindingInfo(uint32_t iteration, DeviceType deviceType, InputBindingType inputBindingType,
                        InputDataType inputDataType, DeviceCreationLocation deviceCreationLocation,
                        const std::string& status) const
{
    printf("Binding (device = %s, iteration = %d, inputBinding = %s, inputDataType = %s, deviceCreationLocation = "
            "%s)...%s\n",
            TypeHelper::Stringify(deviceType).c_str(), iteration, TypeHelper::Stringify(inputBindingType).c_str(),
            TypeHelper::Stringify(inputDataType).c_str(), TypeHelper::Stringify(deviceCreationLocation).c_str(),
            status.c_str());
}

void OutputHelper::PrintEvaluatingInfo(uint32_t iteration, DeviceType deviceType, InputBindingType inputBindingType,
                                       InputDataType inputDataType, DeviceCreationLocation deviceCreationLocation,
                                       const std::string& status) const
{
    printf("Evaluating (device = %s, iteration = %d, inputBinding = %s, inputDataType = %s, deviceCreationLocation "
            "= %s)...%s\n",
            TypeHelper::Stringify(deviceType).c_str(), iteration, TypeHelper::Stringify(inputBindingType).c_str(),
            TypeHelper::Stringify(inputDataType).c_str(), TypeHelper::Stringify(deviceCreationLocation).c_str(),
            status.c_str());
}

void OutputHelper::PrintModelInfo(const std::wstring& modelPath, const LearningModel& model) const
{
    std::cout << "=================================================================" << std::endl;
    std::wcout << "Name: " << model.Name().c_str() << std::endl;
    std::wcout << "Author: " << model.Author().c_str() << std::endl;
    std::wcout << "Version: " << model.Version() << std::endl;
    std::wcout << "Domain: " << model.Domain().c_str() << std::endl;
    std::wcout << "Description: " << model.Description().c_str() << std::endl;
    std::wcout << "Path: " << modelPath << std::endl;
    std::cout << "Support FP16: " << std::boolalpha << doesModelContainFP16(model) << std::endl;

    std::cout << std::endl;
    // print out information about input of model
    std::cout << "Input Feature Info:" << std::endl;
    for (auto&& inputFeature : model.InputFeatures())
    {
        PrintFeatureDescriptorInfo(inputFeature);
    }
    // print out information about output of model
    std::cout << "Output Feature Info:" << std::endl;
    for (auto&& outputFeature : model.OutputFeatures())
    {
        PrintFeatureDescriptorInfo(outputFeature);
    }
    std::cout << "=================================================================" << std::endl;
    std::cout << std::endl;
}

void OutputHelper::PrintFeatureDescriptorInfo(const ILearningModelFeatureDescriptor& descriptor) const
{
    // IMPORTANT: This learningModelFeatureKind array needs to match the "enum class
    // LearningModelFeatureKind" idl in Windows.AI.MachineLearning.0.h
    const std::string learningModelFeatureKind[] = {
        "Tensor",
        "Sequence",
        "Map",
        "Image",
    };
    std::wstring name(descriptor.Name());
    std::wcout << "Name: " << name << std::endl;
    std::wcout << "Feature Kind: " << FeatureDescriptorToString(descriptor) << std::endl;
    std::cout << std::endl;
}

void OutputHelper::PrintHardwareInfo() const
{
    std::cout << "WinML Runner" << std::endl;
    std::cout << "Printing available GPUs with DXGI.." << std::endl;
    com_ptr<IDXGIFactory6> factory;
    CreateDXGIFactory1(__uuidof(IDXGIFactory6), factory.put_void());
    std::vector<com_ptr<IDXGIAdapter1>> validAdapters;
    for (UINT i = 0;; ++i)
    {
        com_ptr<IDXGIAdapter1> spAdapter;
        if (factory->EnumAdapters1(i, spAdapter.put()) != S_OK)
        {
            break;
        }
        DXGI_ADAPTER_DESC1 pDesc;
        spAdapter->GetDesc1(&pDesc);

        // is a software adapter
        if (pDesc.Flags == DXGI_ADAPTER_FLAG_SOFTWARE || (pDesc.VendorId == 0x1414 && pDesc.DeviceId == 0x8c))
        {
            continue;
        }
        // valid GPU adapter
        else
        {
            printf("Index: %d, Description: %ls\n", static_cast<int>(validAdapters.size()), pDesc.Description);
            validAdapters.push_back(spAdapter);
        }
    }
    std::cout << std::endl;
}

void OutputHelper::PopulateLearningModelDevice(LearningModelDeviceWithMetadata& device)
{
    device.DeviceName = L"";
    device.DedicatedMemory = 0;

    if (device.DeviceType == DeviceType::CPU)
    {
        int CPUInfo[4] = { -1 };
        char CPUBrandString[0x40];
        __cpuid(CPUInfo, 0x80000000);
        unsigned int nExIds = CPUInfo[0];
        memset(CPUBrandString, 0, sizeof(CPUBrandString));

        // Get the information associated with each extended ID.
        for (unsigned int i = 0x80000000; i <= nExIds; ++i)
        {
            __cpuid(CPUInfo, i);
            // Interpret CPU brand string.
            if (i == 0x80000002)
                memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
            else if (i == 0x80000003)
                memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
            else if (i == 0x80000004)
                memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
        }
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        device.DeviceName = converter.from_bytes(CPUBrandString);

        MEMORYSTATUSEX statex;
        statex.dwLength = sizeof(statex);
        GlobalMemoryStatusEx(&statex);
        device.DedicatedMemory = (size_t)statex.ullTotalPhys;
    }
    else
    {
        IDirect3DDevice d3dDevice = device.LearningModelDevice.Direct3D11Device();
        com_ptr<IDirect3DDxgiInterfaceAccess> dxgi;
        dxgi = d3dDevice.try_as<IDirect3DDxgiInterfaceAccess>();
        if (dxgi)
        {
            com_ptr<IDXGIDevice> dxgiDevice;
            dxgi->GetInterface(__uuidof(IDXGIDevice), dxgiDevice.put_void());
            com_ptr<IDXGIAdapter> adapter;
            dxgiDevice->GetAdapter(adapter.put());
            DXGI_ADAPTER_DESC description;
            if (SUCCEEDED(adapter->GetDesc(&description)))
            {
                device.DeviceName = description.Description;
                device.DedicatedMemory = description.DedicatedVideoMemory;
            }
        }
        else
        {
            std::cerr << "Failed to retrieve Learning Model Device Information" << std::endl;
        }
    }
}

void OutputHelper::PrintLearningModelDevice(const LearningModelDeviceWithMetadata& device)
{
    float memoryInGB = float(device.DedicatedMemory) / (1024 * 1024 * 1024);
    if (device.DeviceType == DeviceType::CPU)
    {
        std::wcout << "\nCreated LearningModelDevice with CPU: " << device.DeviceName 
                   << " (" << std::fixed << std::setprecision(2) << memoryInGB << " GB RAM)"
                   << std::endl;
    }
    else
    {
        std::wcout << L"\nCreated LearningModelDevice with GPU: " << device.DeviceName 
                   << " (" << std::fixed << std::setprecision(2) << memoryInGB << " GB VRAM)"
                   << std::endl;
    }
}

void OutputHelper::PrintResults(const Profiler<WINML_MODEL_TEST_PERF>& profiler, uint32_t numIterations, DeviceType deviceType,
                    InputBindingType inputBindingType, InputDataType inputDataType,
                    DeviceCreationLocation deviceCreationLocation, bool isPerformanceConsoleOutputVerbose) const
{
    double loadTime = profiler[LOAD_MODEL].GetAverage(CounterType::TIMER);
    double createSessionTime = profiler[CREATE_SESSION].GetAverage(CounterType::TIMER);

    double averageBindTime = profiler[BIND_VALUE].GetAverage(CounterType::TIMER);
    double stdevBindTime = profiler[BIND_VALUE].GetStdev(CounterType::TIMER);
    double minBindTime = profiler[BIND_VALUE].GetMin(CounterType::TIMER);
    double maxBindTime = profiler[BIND_VALUE].GetMax(CounterType::TIMER);
    double firstBindTime = profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::TIMER);

    double averageEvalTime = profiler[EVAL_MODEL].GetAverage(CounterType::TIMER);
    double stdevEvalTime = profiler[EVAL_MODEL].GetStdev(CounterType::TIMER);
    double minEvalTime = profiler[EVAL_MODEL].GetMin(CounterType::TIMER);
    double maxEvalTime = profiler[EVAL_MODEL].GetMax(CounterType::TIMER);
    double firstEvalTime = profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::TIMER);

    double firstLoadWorkingSetMemoryUsage = profiler[LOAD_MODEL].GetAverage(CounterType::WORKING_SET_USAGE);
    double firstLoadSharedMemoryUsage = profiler[LOAD_MODEL].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double firstLoadDedicatedMemoryUsage = profiler[LOAD_MODEL].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double firstLoadPeakWorkingSetUsage = profiler[LOAD_MODEL].GetAverage(CounterType::PEAK_WORKING_SET_USAGE);

    double firstSessionCreationWorkingSetMemoryUsage =
        profiler[CREATE_SESSION].GetAverage(CounterType::WORKING_SET_USAGE);
    double firstSessionCreationSharedMemoryUsage =
        profiler[CREATE_SESSION].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double firstSessionCreationDedicatedMemoryUsage =
        profiler[CREATE_SESSION].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double firstSessionPeakWorkingSetUsage =
        profiler[CREATE_SESSION].GetAverage(CounterType::PEAK_WORKING_SET_USAGE);

    double averageBindMemoryUsage = profiler[BIND_VALUE].GetAverage(CounterType::WORKING_SET_USAGE);
    double stdevBindMemoryUsage = profiler[BIND_VALUE].GetStdev(CounterType::WORKING_SET_USAGE);
    double minBindMemoryUsage = profiler[BIND_VALUE].GetMin(CounterType::WORKING_SET_USAGE);
    double maxBindMemoryUsage = profiler[BIND_VALUE].GetMax(CounterType::WORKING_SET_USAGE);
    double firstBindMemoryUsage = profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::WORKING_SET_USAGE);
    double firstBindPeakMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::PEAK_WORKING_SET_USAGE);

    double averageEvalMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::WORKING_SET_USAGE);
    double stdevEvalMemoryUsage = profiler[EVAL_MODEL].GetStdev(CounterType::WORKING_SET_USAGE);
    double minEvalMemoryUsage = profiler[EVAL_MODEL].GetMin(CounterType::WORKING_SET_USAGE);
    double maxEvalMemoryUsage = profiler[EVAL_MODEL].GetMax(CounterType::WORKING_SET_USAGE);
    double firstEvalMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::WORKING_SET_USAGE);
    double firstEvalPeakMemoryUsage =
        profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::PEAK_WORKING_SET_USAGE);

    double averageBindDedicatedMemoryUsage = profiler[BIND_VALUE].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double stdevBindDedicatedMemoryUsage = profiler[BIND_VALUE].GetStdev(CounterType::GPU_DEDICATED_MEM_USAGE);
    double minBindDedicatedMemoryUsage = profiler[BIND_VALUE].GetMin(CounterType::GPU_DEDICATED_MEM_USAGE);
    double maxBindDedicatedMemoryUsage = profiler[BIND_VALUE].GetMax(CounterType::GPU_DEDICATED_MEM_USAGE);
    double firstBindDedicatedMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);

    double averageEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double stdevEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetStdev(CounterType::GPU_DEDICATED_MEM_USAGE);
    double minEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetMin(CounterType::GPU_DEDICATED_MEM_USAGE);
    double maxEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetMax(CounterType::GPU_DEDICATED_MEM_USAGE);
    double firstEvalDedicatedMemoryUsage =
        profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);

    double averageBindSharedMemoryUsage = profiler[BIND_VALUE].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double stdevBindSharedMemoryUsage = profiler[BIND_VALUE].GetStdev(CounterType::GPU_SHARED_MEM_USAGE);
    double minBindSharedMemoryUsage = profiler[BIND_VALUE].GetMin(CounterType::GPU_SHARED_MEM_USAGE);
    double maxBindSharedMemoryUsage = profiler[BIND_VALUE].GetMax(CounterType::GPU_SHARED_MEM_USAGE);
    double firstBindSharedMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);

    double averageEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double stdevEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetStdev(CounterType::GPU_SHARED_MEM_USAGE);
    double minEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetMin(CounterType::GPU_SHARED_MEM_USAGE);
    double maxEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetMax(CounterType::GPU_SHARED_MEM_USAGE);
    double firstEvalSharedMemoryUsage =
        profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);

    double firstIterationWorkingSetMemoryUsage =
        profiler[LOAD_MODEL].GetAverage(CounterType::WORKING_SET_USAGE) +
        profiler[CREATE_SESSION].GetAverage(CounterType::WORKING_SET_USAGE) +
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::WORKING_SET_USAGE) +
        profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::WORKING_SET_USAGE);

    double firstIterationSharedMemoryUsage =
        profiler[LOAD_MODEL].GetAverage(CounterType::GPU_SHARED_MEM_USAGE) +
        profiler[CREATE_SESSION].GetAverage(CounterType::GPU_SHARED_MEM_USAGE) +
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::GPU_SHARED_MEM_USAGE) +
        profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);

    double firstIterationDedicatedMemoryUsage =
        profiler[LOAD_MODEL].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE) +
        profiler[CREATE_SESSION].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE) +
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE) +
        profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);

    double firstIterationPeakWorkingSet = firstLoadPeakWorkingSetUsage + firstSessionPeakWorkingSetUsage +
                                            firstBindPeakMemoryUsage + firstEvalPeakMemoryUsage;

    printf("\nResults (device = %s, numIterations = %d, inputBinding = %s, inputDataType = %s, "
            "deviceCreationLocation = %s):\n",
            TypeHelper::Stringify(deviceType).c_str(), numIterations,
            TypeHelper::Stringify(inputBindingType).c_str(), TypeHelper::Stringify(inputDataType).c_str(),
            TypeHelper::Stringify(deviceCreationLocation).c_str());

    std::cout << "\nFirst Iteration Performance (load, bind, session creation, and evaluate): " << std::endl;
    std::cout << "  Load: " << loadTime << " ms" << std::endl;
    std::cout << "  Bind: " << firstBindTime << " ms" << std::endl;
    std::cout << "  Session Creation: " << createSessionTime << " ms" << std::endl;
    std::cout << "  Evaluate: " << firstEvalTime << " ms" << std::endl;

    if (isPerformanceConsoleOutputVerbose)
    {
        std::cout << "\n  Working Set Memory usage (load): " << firstLoadWorkingSetMemoryUsage << " MB"
                    << std::endl;
        std::cout << "  Working Set Memory usage (session creation): " << firstSessionCreationWorkingSetMemoryUsage
                    << " MB" << std::endl;
        std::cout << "  Working Set Memory usage (bind): " << firstBindMemoryUsage << " MB" << std::endl;
    }
    else
    {
        std::cout << std::endl;
    }
    std::cout << "  Working Set Memory usage (evaluate): " << firstEvalMemoryUsage << " MB" << std::endl;
    std::cout << "  Working Set Memory usage (load, bind, session creation, and evaluate): "
                << firstIterationWorkingSetMemoryUsage << " MB" << std::endl;

    if (isPerformanceConsoleOutputVerbose)
    {
        std::cout << std::endl;
        std::cout << "  Peak Working Set Memory Difference (from start to load): " << firstLoadPeakWorkingSetUsage
                    << " MB" << std::endl;
        std::cout << "  Peak Working Set Memory Difference (from model load to session creation): "
                    << firstSessionPeakWorkingSetUsage << " MB" << std::endl;
        std::cout << "  Peak Working Set Memory Difference (from session to bind): " << firstBindPeakMemoryUsage
                    << " MB" << std::endl;
        std::cout << "  Peak Working Set Memory Difference (from bind to evaluate): " << firstEvalPeakMemoryUsage
                    << " MB" << std::endl;
    }

    std::cout << "  Peak Working Set Memory Difference (load, bind, session creation, and evaluate): "
                << firstIterationPeakWorkingSet << " MB" << std::endl;

    if (isPerformanceConsoleOutputVerbose)
    {
        std::cout << "\n  Dedicated Memory usage (load): " << firstLoadDedicatedMemoryUsage << " MB" << std::endl;
        std::cout << "  Dedicated Memory usage (session creation): " << firstSessionCreationDedicatedMemoryUsage
                    << " MB" << std::endl;
        std::cout << "  Dedicated Memory usage (bind): " << firstBindDedicatedMemoryUsage << " MB" << std::endl;
    }
    else
    {
        std::cout << std::endl;
    }
    std::cout << "  Dedicated Memory usage (evaluate): " << firstEvalDedicatedMemoryUsage << " MB" << std::endl;
    std::cout << "  Dedicated Memory usage (load, bind, session creation, and evaluate): "
                << firstIterationDedicatedMemoryUsage << " MB" << std::endl;

    if (isPerformanceConsoleOutputVerbose)
    {
        std::cout << "\n  Shared Memory usage (load): " << firstLoadSharedMemoryUsage << " MB" << std::endl;
        std::cout << "  Shared Memory usage (session creation): " << firstSessionCreationSharedMemoryUsage << " MB"
                    << std::endl;
        std::cout << "  Shared Memory usage (bind): " << firstBindSharedMemoryUsage << " MB" << std::endl;
    }
    else
    {
        std::cout << std::endl;
    }
    std::cout << "  Shared Memory usage (evaluate): " << firstEvalSharedMemoryUsage << " MB" << std::endl;
    std::cout << "  Shared Memory usage (load, bind, session creation, and evaluate): "
                << firstIterationSharedMemoryUsage << " MB" << std::endl;

    if (numIterations > 1)
    {
        printf("\nAverage Performance excluding first iteration. Iterations %d to %d. (Iterations greater than 1 "
                "only bind and evaluate)\n",
                2, numIterations);
        std::cout << "  Average Bind: " << averageBindTime << " ms" << std::endl;
        if (isPerformanceConsoleOutputVerbose)
        {
            std::cout << "  Minimum Bind: " << minBindTime << " ms" << std::endl;
            std::cout << "  Maximum Bind: " << maxBindTime << " ms" << std::endl;
            std::cout << "  Standard Deviation Bind: " << stdevBindTime << " ms" << std::endl;
        }
        std::cout << "  Average Evaluate: " << averageEvalTime << " ms" << std::endl;
        if (isPerformanceConsoleOutputVerbose)
        {
            std::cout << "  Minimum Evaluate: " << minEvalTime << " ms" << std::endl;
            std::cout << "  Maximum Evaluate: " << maxEvalTime << " ms" << std::endl;
            std::cout << "  Standard Deviation Evaluate: " << stdevEvalTime << " ms" << std::endl;
        }

        std::cout << "\n  Average Working Set Memory usage (bind): " << averageBindMemoryUsage << " MB"
                    << std::endl;
        if (isPerformanceConsoleOutputVerbose)
        {
            std::cout << "  Min Working Set Memory usage (bind): " << minBindMemoryUsage << " MB" << std::endl;
            std::cout << "  Max Working Set Memory usage (bind): " << maxBindMemoryUsage << " MB" << std::endl;
            std::cout << "  Standard Deviation Working Set Memory usage (bind): " << stdevBindMemoryUsage << " MB"
                        << std::endl;
        }
        std::cout << "  Average Working Set Memory usage (evaluate): " << averageEvalMemoryUsage << " MB"
                    << std::endl;
        if (isPerformanceConsoleOutputVerbose)
        {
            std::cout << "  Min Working Set Memory usage (evaluate): " << minEvalMemoryUsage << " MB" << std::endl;
            std::cout << "  Max Working Set Memory usage (evaluate): " << maxEvalMemoryUsage << " MB" << std::endl;
            std::cout << "  Standard Deviation Working Set Memory usage (evaluate): " << stdevEvalMemoryUsage
                        << " MB" << std::endl;
        }

        std::cout << "\n  Average Dedicated Memory usage (bind): " << averageBindDedicatedMemoryUsage << " MB"
                    << std::endl;
        if (isPerformanceConsoleOutputVerbose)
        {
            std::cout << "  Min Dedicated Memory usage (bind): " << minBindDedicatedMemoryUsage << " MB"
                        << std::endl;
            std::cout << "  Max Dedicated Memory usage (bind): " << maxBindDedicatedMemoryUsage << " MB"
                        << std::endl;
            std::cout << "  Standard Deviation Working Set Memory usage (evaluate): "
                        << stdevBindDedicatedMemoryUsage << " MB" << std::endl;
        }
        std::cout << "  Average Dedicated Memory usage (evaluate): " << averageEvalDedicatedMemoryUsage << " MB"
                    << std::endl;
        if (isPerformanceConsoleOutputVerbose)
        {
            std::cout << "  Min Dedicated Memory usage (evaluate): " << minEvalDedicatedMemoryUsage << " MB"
                        << std::endl;
            std::cout << "  Max Dedicated Memory usage (evaluate): " << maxEvalDedicatedMemoryUsage << " MB"
                        << std::endl;
            std::cout << "  Standard Deviation Dedicated Memory usage (evaluate): " << stdevEvalDedicatedMemoryUsage
                        << " MB" << std::endl;
        }

        std::cout << "\n  Average Shared Memory usage (bind): " << averageBindSharedMemoryUsage << " MB"
                    << std::endl;
        if (isPerformanceConsoleOutputVerbose)
        {
            std::cout << "  Min Shared Memory usage (bind): " << minBindSharedMemoryUsage << " MB" << std::endl;
            std::cout << "  Max Shared Memory usage (bind): " << maxBindSharedMemoryUsage << " MB" << std::endl;
            std::cout << "  Standard Deviation Shared Memory usage (bind): " << stdevBindSharedMemoryUsage << " MB"
                        << std::endl;
        }
        std::cout << "  Average Shared Memory usage (evaluate): " << averageEvalSharedMemoryUsage << " MB"
                    << std::endl;
        if (isPerformanceConsoleOutputVerbose)
        {
            std::cout << "  Min Shared Memory usage (evaluate): " << minEvalSharedMemoryUsage << " MB" << std::endl;
            std::cout << "  Max Shared Memory usage (evaluate): " << maxEvalSharedMemoryUsage << " MB" << std::endl;
            std::cout << "  Standard Deviation Shared Memory usage (evaluate): " << stdevEvalSharedMemoryUsage
                        << " MB" << std::endl;
        }
    }
    std::cout << std::endl << std::endl << std::endl;
}

std::wstring OutputHelper::FeatureDescriptorToString(const ILearningModelFeatureDescriptor& descriptor)
{
    switch (descriptor.Kind())
    {
        case LearningModelFeatureKind::Tensor:
        {
            auto tensorDescriptor = descriptor.as<TensorFeatureDescriptor>();
            return TypeHelper::Stringify(tensorDescriptor.TensorKind());
        }
        case LearningModelFeatureKind::Image:
        {
            auto imageDescriptor = descriptor.as<ImageFeatureDescriptor>();
            std::wstring str = L"Image (Height: " + std::to_wstring(imageDescriptor.Height()) + L", Width:  " +
                                std::to_wstring(imageDescriptor.Width()) + L")";
            return str;
        }
        case LearningModelFeatureKind::Map:
        {
            auto mapDescriptor = descriptor.as<MapFeatureDescriptor>();
            std::wstring str = L"Map<" + TypeHelper::Stringify(mapDescriptor.KeyKind()) + L",";
            str += FeatureDescriptorToString(mapDescriptor.ValueDescriptor());
            str += L">";
            return str;
        }
        case LearningModelFeatureKind::Sequence:
        {
            auto sequenceDescriptor = descriptor.as<SequenceFeatureDescriptor>();
            std::wstring str = L"List<" + FeatureDescriptorToString(sequenceDescriptor.ElementDescriptor()) + L">";
            return str;
        }
        default:
            return (L"Invalid feature %s.", descriptor.Name().c_str());
    }
}

bool OutputHelper::doesDescriptorContainFP16(const ILearningModelFeatureDescriptor& descriptor)
{
    switch (descriptor.Kind())
    {
        case LearningModelFeatureKind::Tensor:
        {
            return descriptor.as<TensorFeatureDescriptor>().TensorKind() == TensorKind::Float16;
        }
        break;
        case LearningModelFeatureKind::Map:
        {
            auto mapDescriptor = descriptor.as<MapFeatureDescriptor>();
            if (mapDescriptor.KeyKind() == TensorKind::Float16)
            {
                return true;
            }
            return doesDescriptorContainFP16(mapDescriptor.ValueDescriptor());
        }
        break;
        case LearningModelFeatureKind::Sequence:
        {
            return doesDescriptorContainFP16(descriptor.as<SequenceFeatureDescriptor>().ElementDescriptor());
        }
        break;
        default:
        {
            return false;
        }
    }
}

bool OutputHelper::doesModelContainFP16(const LearningModel& model)
{
    for (auto&& inputFeature : model.InputFeatures())
    {
        if (doesDescriptorContainFP16(inputFeature))
        {
            return true;
        }
    }
    return false;
}

void OutputHelper::SaveLoadTimes(Profiler<WINML_MODEL_TEST_PERF>& profiler, uint32_t iterNum)
{
    m_clockLoadTimes[iterNum] = profiler[LOAD_MODEL].GetClockTime();
}

void OutputHelper::SaveBindTimes(Profiler<WINML_MODEL_TEST_PERF>& profiler, uint32_t iterNum)
{
    m_clockBindTimes[iterNum] =
        (iterNum == 0) ? profiler[BIND_VALUE_FIRST_RUN].GetClockTime() : profiler[BIND_VALUE].GetClockTime();
}

void OutputHelper::SaveEvalPerformance(Profiler<WINML_MODEL_TEST_PERF>& profiler, uint32_t iterNum)
{
    enum WINML_MODEL_TEST_PERF eval = (iterNum == 0) ? EVAL_MODEL_FIRST_RUN : EVAL_MODEL;
    m_clockEvalTimes[iterNum] = profiler[eval].GetClockTime();
    m_CPUWorkingDiff[iterNum] = profiler[eval].GetCpuWorkingDiff();
    m_CPUWorkingStart[iterNum] = profiler[eval].GetCpuWorkingStart();
    m_GPUSharedDiff[iterNum] = profiler[eval].GetGpuSharedDiff();
    m_GPUSharedStart[iterNum] = profiler[eval].GetGpuSharedStart();
    m_GPUDedicatedDiff[iterNum] = profiler[eval].GetGpuDedicatedDiff();
}

void OutputHelper::SaveResult(uint32_t iterationNum, std::string result, size_t hashcode)
{
    m_outputResult[iterationNum] = result;
    m_outputTensorHash[iterationNum] = hashcode;
}

void OutputHelper::SetDefaultPerIterationFolder(const std::wstring& folderName)
{
    m_folderNamePerIteration = folderName;
    if (std::filesystem::create_directories(m_folderNamePerIteration.c_str()) != 0)
        std::wcout << L"Folder [" + m_folderNamePerIteration + L"] cannot be created";
}

void OutputHelper::SetDefaultCSVFileNamePerIteration()
{
    m_csvFileNamePerIterationSummary = m_folderNamePerIteration + L"\\Summary.csv";
}

std::wstring OutputHelper::GetDefaultCSVFileNamePerIteration() const { return m_csvFileNamePerIterationSummary; }

std::wstring OutputHelper::GetCsvFileNamePerIterationResult() const { return m_csvFileNamePerIterationResult; }

void OutputHelper::SetDefaultCSVIterationResult(uint32_t iterationNum, const CommandLineArgs& args,
                                                const std::wstring& deviceName, const std::wstring& featureName)
{
    m_fileNameResultDevice = m_folderNamePerIteration + L"\\" + featureName + deviceName + L"Iteration";
    m_csvFileNamePerIterationResult = m_fileNameResultDevice + std::to_wstring(iterationNum + 1) + L".csv";
}

void OutputHelper::SetCSVFileName(const std::wstring& fileName) { m_csvFileName = fileName; }

void OutputHelper::WritePerIterationPerformance(const CommandLineArgs& args, const std::wstring model,
                                    const std::wstring imagePath)
{
    if (m_csvFileNamePerIterationSummary.length() > 0)
    {
        bool bNewFile = false;
        std::ifstream fin;
        fin.open(m_csvFileNamePerIterationSummary);
        std::filebuf* outbuf = fin.rdbuf();
        if (EOF == outbuf->sbumpc())
        {
            bNewFile = true;
        }
        fin.close();

        std::ofstream fout;
        fout.open(m_csvFileNamePerIterationSummary, std::ios_base::app);

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::string modelName = converter.to_bytes(model);
        std::string fileNameResultDevice = converter.to_bytes(m_fileNameResultDevice);
        std::string inputName = args.IsCSVInput() ? converter.to_bytes(args.CsvPath())
                                 : args.IsJsonInput()   ? converter.to_bytes(args.JsonPath())
                                   : args.IsImageInput() ? converter.to_bytes(imagePath)
                                     : "";

        // capture tensor properties even if tensor is not saved
        if (bNewFile)
        {
            if (args.IsPerIterationCapture())
            {
                fout << "Model Name"
                        << ","
                        << "Input Name"
                        << ","
                        << "Iterations"
                        << ","
                        << "Iteration Number "
                        << ","
                        << "CPU Working Set Diff (MB)"
                        << ","
                        << "CPU Working Set Start (MB)"
                        << ","
                        << "GPU Shared Memory Diff (MB)"
                        << ","
                        << "GPU Shared Memory Start (MB)"
                        << ","
                        << "GPU Dedicated Memory Diff (MB)"
                        << ","
                        << "Load (ms)"
                        << ","
                        << "Bind (ms)"
                        << ","
                        << "Evaluate (ms)"
                        << ",";
                
                fout << "Result"
                        << ","
                        << "OutputTensorHash"
                        << ","
                        << "FileName";
            }
            else
            {
                fout << "Iteration Number"
                        << ","
                        << "Result"
                        << ","
                        << "OutputTensorHash"
                        << ","
                        << "FileName";
            }
            fout << std::endl;
        }

        if (args.IsPerIterationCapture())
        {
            for (uint32_t i = 0; i < args.NumIterations(); i++)
            {
                fout << modelName << "," << inputName << "," << args.NumIterations() << "," << i + 1 << ","
                     << m_CPUWorkingDiff[i] << "," << m_CPUWorkingStart[i] << "," << m_GPUSharedDiff[i] << ","
                     << m_GPUSharedStart[i] << "," << m_GPUDedicatedDiff[i] << "," << m_clockLoadTimes[i] << ","
                     << m_clockBindTimes[i] << "," << m_clockEvalTimes[i] << ",";
                fout << m_outputResult[i] << "," << m_outputTensorHash[i] << ","
                     << fileNameResultDevice + std::to_string(i + 1) + (args.IsCSVInput() ? ".csv" : ".json") << std::endl;
            }
        }
        else
        {
            for (uint32_t i = 0; i < args.NumIterations(); i++)
            {
                fout << i + 1 << "," 
                     << m_outputResult[i] << "," << m_outputTensorHash[i] << ","
                     << fileNameResultDevice + std::to_string(i + 1) + (args.IsCSVInput() ? ".csv" : ".json") << std::endl;
            }
        }
        fout.close();
    }
}

template <typename T>
void OutputHelper::ProcessTensorResult(const CommandLineArgs& args, const void* buffer, const uint32_t uCapacity,
                                       std::vector<std::pair<float, int>>& maxValues, std::ofstream& fout,
                                       unsigned int k, bool saveFullTensor)
{
    // Create a priority queue of size k that pops the lowest value first
    // We will remove lowest values as we iterate over all the values
    auto cmp = [](std::pair<float, int> x, std::pair<float, int> y) { return x.first > y.first; };
    std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, decltype(cmp)> topKvalues(cmp);

    T* tensor = (T*)buffer;
    int size = uCapacity / sizeof(T);
    for (int i = 0; i < size; i++)
    {
        float val = 0;
        if (!std::is_same<T, HALF>::value)
        {
            val = *(tensor + i);
        }
        else
        {
            val = XMConvertHalfToFloat(static_cast<HALF>(*(tensor + i)));
        }
        if (args.IsSaveTensor() && (saveFullTensor || i < 10))
        {
            fout << i << "," << val << std::endl;
        }

        if (topKvalues.size() < k)
        {
            topKvalues.push({ val, i });
        }
        else if (k > 0)
        {
            auto maxValue = topKvalues.top().first;
            if (maxValue < val)
            {
                topKvalues.pop();
                topKvalues.push({ val, i });
            }
        }
    }
    while (!topKvalues.empty())
    {
        auto pair = topKvalues.top();
        maxValues.push_back(pair);
        topKvalues.pop();
    }
    // Put vector in order of highest value to lowest
    std::reverse(maxValues.begin(), maxValues.end());
}
template void OutputHelper::ProcessTensorResult<float>(const CommandLineArgs& args, const void* buffer, const uint32_t uCapacity,
                                                       std::vector<std::pair<float, int>>& maxValues, std::ofstream& fout,
                                                       unsigned int k, bool saveFullTensor);
template void OutputHelper::ProcessTensorResult<HALF>(const CommandLineArgs& args, const void* buffer,
                                                      const uint32_t uCapacity,
                                                      std::vector<std::pair<float, int>>& maxValues, std::ofstream& fout, 
                                                      unsigned int k, bool saveFullTensor);

void OutputHelper::WritePerformanceDataToCSV(const Profiler<WINML_MODEL_TEST_PERF>& profiler, int numIterations,
                            const std::wstring& modelPath, const std::string& deviceType, const std::string& inputBinding,
                            const std::string& inputType, const std::string& deviceCreationLocation,
                            const std::wstring& deviceName, size_t deviceMemory,
                            const std::vector<std::pair<std::string, std::string>>& perfFileMetadata) const
{
    double averageLoadTime = profiler[LOAD_MODEL].GetAverage(CounterType::TIMER);
    double stdevLoadTime = profiler[LOAD_MODEL].GetStdev(CounterType::TIMER);
    double minLoadTime = profiler[LOAD_MODEL].GetMin(CounterType::TIMER);
    double maxLoadTime = profiler[LOAD_MODEL].GetMax(CounterType::TIMER);
    uint32_t numberLoadIterations = profiler[LOAD_MODEL].GetCount();

    double averageCreateSessionTime = profiler[CREATE_SESSION].GetAverage(CounterType::TIMER);
    double stdevCreateSessionTime = profiler[CREATE_SESSION].GetStdev(CounterType::TIMER);
    double minCreateSessionTime = profiler[CREATE_SESSION].GetMin(CounterType::TIMER);
    double maxCreateSessionTime = profiler[CREATE_SESSION].GetMax(CounterType::TIMER);
    uint32_t numberCreateSessionIterations = profiler[CREATE_SESSION].GetCount();

    double averageBindTime = profiler[BIND_VALUE].GetAverage(CounterType::TIMER);
    double stdevBindTime = profiler[BIND_VALUE].GetStdev(CounterType::TIMER);
    double minBindTime = profiler[BIND_VALUE].GetMin(CounterType::TIMER);
    double maxBindTime = profiler[BIND_VALUE].GetMax(CounterType::TIMER);

    double averageFirstBindTime = profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::TIMER);
    double stdevFirstBindTime = profiler[BIND_VALUE_FIRST_RUN].GetStdev(CounterType::TIMER);
    double minFirstBindTime = profiler[BIND_VALUE_FIRST_RUN].GetMin(CounterType::TIMER);
    double maxFirstBindTime = profiler[BIND_VALUE_FIRST_RUN].GetMax(CounterType::TIMER);

    double averageEvalTime = profiler[EVAL_MODEL].GetAverage(CounterType::TIMER);
    double stdevEvalTime = profiler[EVAL_MODEL].GetStdev(CounterType::TIMER);
    double minEvalTime = profiler[EVAL_MODEL].GetMin(CounterType::TIMER);
    double maxEvalTime = profiler[EVAL_MODEL].GetMax(CounterType::TIMER);

    double averageFirstEvalTime = profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::TIMER);
    double stdevFirstEvalTime = profiler[EVAL_MODEL_FIRST_RUN].GetStdev(CounterType::TIMER);
    double minFirstEvalTime = profiler[EVAL_MODEL_FIRST_RUN].GetMin(CounterType::TIMER);
    double maxFirstEvalTime = profiler[EVAL_MODEL_FIRST_RUN].GetMax(CounterType::TIMER);

    double averageLoadWorkingSetMemoryUsage = profiler[LOAD_MODEL].GetAverage(CounterType::WORKING_SET_USAGE);
    double stdevLoadWorkingSetMemoryUsage = profiler[LOAD_MODEL].GetStdev(CounterType::WORKING_SET_USAGE);
    double minLoadWorkingSetMemoryUsage = profiler[LOAD_MODEL].GetMin(CounterType::WORKING_SET_USAGE);
    double maxLoadWorkingSetMemoryUsage = profiler[LOAD_MODEL].GetMax(CounterType::WORKING_SET_USAGE);

    double averageCreateSessionWorkingSetMemoryUsage =
        profiler[CREATE_SESSION].GetAverage(CounterType::WORKING_SET_USAGE);
    double stdevCreateSessionWorkingSetMemoryUsage =
        profiler[CREATE_SESSION].GetStdev(CounterType::WORKING_SET_USAGE);
    double minCreateSessionWorkingSetMemoryUsage = profiler[CREATE_SESSION].GetMin(CounterType::WORKING_SET_USAGE);
    double maxCreateSessionWorkingSetMemoryUsage = profiler[CREATE_SESSION].GetMax(CounterType::WORKING_SET_USAGE);

    double averageBindWorkingSetMemoryUsage = profiler[BIND_VALUE].GetAverage(CounterType::WORKING_SET_USAGE);
    double stdevBindWorkingSetMemoryUsage = profiler[BIND_VALUE].GetStdev(CounterType::WORKING_SET_USAGE);
    double minBindWorkingSetMemoryUsage = profiler[BIND_VALUE].GetMin(CounterType::WORKING_SET_USAGE);
    double maxBindWorkingSetMemoryUsage = profiler[BIND_VALUE].GetMax(CounterType::WORKING_SET_USAGE);

    double averageFirstBindWorkingSetMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::WORKING_SET_USAGE);
    double stdevFirstBindWorkingSetMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetStdev(CounterType::WORKING_SET_USAGE);
    double minFirstBindWorkingSetMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetMin(CounterType::WORKING_SET_USAGE);
    double maxFirstBindWorkingSetMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetMax(CounterType::WORKING_SET_USAGE);

    double averageEvalWorkingSetMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::WORKING_SET_USAGE);
    double stdevEvalWorkingSetMemoryUsage = profiler[EVAL_MODEL].GetStdev(CounterType::WORKING_SET_USAGE);
    double minEvalWorkingSetMemoryUsage = profiler[EVAL_MODEL].GetMin(CounterType::WORKING_SET_USAGE);
    double maxEvalWorkingSetMemoryUsage = profiler[EVAL_MODEL].GetMax(CounterType::WORKING_SET_USAGE);

    double averageFirstEvalWorkingSetMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::WORKING_SET_USAGE);
    double stdevFirstEvalWorkingSetMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetStdev(CounterType::WORKING_SET_USAGE);
    double minFirstEvalWorkingSetMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetMin(CounterType::WORKING_SET_USAGE);
    double maxFirstEvalWorkingSetMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetMax(CounterType::WORKING_SET_USAGE);

    double averageLoadDedicatedMemoryUsage = profiler[LOAD_MODEL].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double stdevLoadDedicatedMemoryUsage = profiler[LOAD_MODEL].GetStdev(CounterType::GPU_DEDICATED_MEM_USAGE);
    double minLoadDedicatedMemoryUsage = profiler[LOAD_MODEL].GetMin(CounterType::GPU_DEDICATED_MEM_USAGE);
    double maxLoadDedicatedMemoryUsage = profiler[LOAD_MODEL].GetMax(CounterType::GPU_DEDICATED_MEM_USAGE);

    double averageCreateSessionDedicatedMemoryUsage =
        profiler[CREATE_SESSION].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double stdevCreateSessionDedicatedMemoryUsage =
        profiler[CREATE_SESSION].GetStdev(CounterType::GPU_DEDICATED_MEM_USAGE);
    double minCreateSessionDedicatedMemoryUsage =
        profiler[CREATE_SESSION].GetMin(CounterType::GPU_DEDICATED_MEM_USAGE);
    double maxCreateSessionDedicatedMemoryUsage =
        profiler[CREATE_SESSION].GetMax(CounterType::GPU_DEDICATED_MEM_USAGE);

    double averageFirstBindDedicatedMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double stdevFirstBindDedicatedMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetStdev(CounterType::GPU_DEDICATED_MEM_USAGE);
    double minFirstBindDedicatedMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetMin(CounterType::GPU_DEDICATED_MEM_USAGE);
    double maxFirstBindDedicatedMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetMax(CounterType::GPU_DEDICATED_MEM_USAGE);

    double averageBindDedicatedMemoryUsage = profiler[BIND_VALUE].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double stdevBindDedicatedMemoryUsage = profiler[BIND_VALUE].GetStdev(CounterType::GPU_DEDICATED_MEM_USAGE);
    double minBindDedicatedMemoryUsage = profiler[BIND_VALUE].GetMin(CounterType::GPU_DEDICATED_MEM_USAGE);
    double maxBindDedicatedMemoryUsage = profiler[BIND_VALUE].GetMax(CounterType::GPU_DEDICATED_MEM_USAGE);

    double averageFirstEvalDedicatedMemoryUsage =
        profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double stdevFirstEvalDedicatedMemoryUsage = 
        profiler[EVAL_MODEL_FIRST_RUN].GetStdev(CounterType::GPU_DEDICATED_MEM_USAGE);
    double minFirstEvalDedicatedMemoryUsage = 
        profiler[EVAL_MODEL_FIRST_RUN].GetMin(CounterType::GPU_DEDICATED_MEM_USAGE);
    double maxFirstEvalDedicatedMemoryUsage =
        profiler[EVAL_MODEL_FIRST_RUN].GetMax(CounterType::GPU_DEDICATED_MEM_USAGE);

    double averageEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::GPU_DEDICATED_MEM_USAGE);
    double stdevEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetStdev(CounterType::GPU_DEDICATED_MEM_USAGE);
    double minEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetMin(CounterType::GPU_DEDICATED_MEM_USAGE);
    double maxEvalDedicatedMemoryUsage = profiler[EVAL_MODEL].GetMax(CounterType::GPU_DEDICATED_MEM_USAGE);

    double averageLoadSharedMemoryUsage = profiler[LOAD_MODEL].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double stdevLoadSharedMemoryUsage = profiler[LOAD_MODEL].GetStdev(CounterType::GPU_SHARED_MEM_USAGE);
    double minLoadSharedMemoryUsage = profiler[LOAD_MODEL].GetMin(CounterType::GPU_SHARED_MEM_USAGE);
    double maxLoadSharedMemoryUsage = profiler[LOAD_MODEL].GetMax(CounterType::GPU_SHARED_MEM_USAGE);

    double averageCreateSessionSharedMemoryUsage =
        profiler[CREATE_SESSION].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double stdevCreateSessionSharedMemoryUsage =
        profiler[CREATE_SESSION].GetStdev(CounterType::GPU_SHARED_MEM_USAGE);
    double minCreateSessionSharedMemoryUsage = profiler[CREATE_SESSION].GetMin(CounterType::GPU_SHARED_MEM_USAGE);
    double maxCreateSessionSharedMemoryUsage = profiler[CREATE_SESSION].GetMax(CounterType::GPU_SHARED_MEM_USAGE);

    double averageFirstBindSharedMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double stdevFirstBindSharedMemoryUsage =
        profiler[BIND_VALUE_FIRST_RUN].GetStdev(CounterType::GPU_SHARED_MEM_USAGE);
    double minFirstBindSharedMemoryUsage = profiler[BIND_VALUE_FIRST_RUN].GetMin(CounterType::GPU_SHARED_MEM_USAGE);
    double maxFirstBindSharedMemoryUsage = profiler[BIND_VALUE_FIRST_RUN].GetMax(CounterType::GPU_SHARED_MEM_USAGE);

    double averageBindSharedMemoryUsage = profiler[BIND_VALUE].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double stdevBindSharedMemoryUsage = profiler[BIND_VALUE].GetStdev(CounterType::GPU_SHARED_MEM_USAGE);
    double minBindSharedMemoryUsage = profiler[BIND_VALUE].GetMin(CounterType::GPU_SHARED_MEM_USAGE);
    double maxBindSharedMemoryUsage = profiler[BIND_VALUE].GetMax(CounterType::GPU_SHARED_MEM_USAGE);

    double averageEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double stdevEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetStdev(CounterType::GPU_SHARED_MEM_USAGE);
    double minEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetMin(CounterType::GPU_SHARED_MEM_USAGE);
    double maxEvalSharedMemoryUsage = profiler[EVAL_MODEL].GetMax(CounterType::GPU_SHARED_MEM_USAGE);

    double averageFirstEvalSharedMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetAverage(CounterType::GPU_SHARED_MEM_USAGE);
    double stdevFirstEvalSharedMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetStdev(CounterType::GPU_SHARED_MEM_USAGE);
    double minFirstEvalSharedMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetMin(CounterType::GPU_SHARED_MEM_USAGE);
    double maxFirstEvalSharedMemoryUsage = profiler[EVAL_MODEL_FIRST_RUN].GetMax(CounterType::GPU_SHARED_MEM_USAGE);

    if (!m_csvFileName.empty())
    {
        // Check if header exists
        bool bNewFile = false;
        std::ifstream fin;
        fin.open(m_csvFileName);
        std::filebuf* outbuf = fin.rdbuf();
        if (EOF == outbuf->sbumpc())
        {
            bNewFile = true;
        }
        fin.close();

        std::ofstream fout;
        fout.open(m_csvFileName, std::ios_base::app);

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        if (bNewFile)
        {
            fout << "model name" << ","
                 << "device type" << ","
                 << "device name" << ","
                 << "device memory" << ","
                 << "input binding" << ","
                 << "input type" << ","
                 << "device creation location" << ","
                 << "iterations" << ","
                 << "load iterations" << ","
                 << "session creation iterations"
                 << ","
                 // load  (ms)
                 << "load first (ms)"
                 << ","
                 // create  (ms)
                 << "session creation first (ms)"
                 << ","
                 // [first]bind  (ms)
                 << "bind first (ms)" << ","
                 << "bind average (ms)" << ","
                 << "bind standard deviation (ms)" << ","
                 << "bind min (ms)" << ","
                 << "bind max (ms)"
                 << ","
                 // [first]evaluate (ms)
                 << "evaluate first (ms)" << ","
                 << "evaluate average (ms)" << ","
                 << "evaluate standard deviation (ms)" << ","
                 << "evaluate min (ms)" << ","
                 << "evaluate max (ms)"
                 << ","

                 // load memory (WorkingSetMemory)
                 << "load average working set memory (MB)"
                 << ","
                 // create memory (WorkingSetMemory)
                 << "session creation average working set memory (MB)"
                 << ","
                 // [first]bind  (WorkingSetMemory)
                 << "bind first working set memory (MB)" << ","
                 << "bind average working set memory (MB)" << ","
                 << "bind standard deviation working set memory (MB)" << ","
                 << "bind min working set memory (MB)" << ","
                 << "bind max working set memory (MB)"
                 << ","
                 // [first]eval (WorkingSetMemory)
                 << "evaluate first working set memory (MB)" << ","
                 << "evaluate average working set memory (MB)" << ","
                 << "evaluate standard deviation working set memory (MB)" << ","
                 << "evaluate min working set memory (MB)" << ","
                 << "evaluate max working set memory (MB)"
                 << ","

                 // load memory (VRAM)
                 << "load average dedicated memory (MB)"
                 << ","
                 // create memory (VRAM)
                 << "session creation average dedicated memory (MB)"
                 << ","
                 // [first]bind  (VRAM)
                 << "bind first dedicated memory (MB)" << ","
                 << "bind average dedicated memory (MB)" << ","
                 << "bind standard deviation dedicated memory (MB)" << ","
                 << "bind min dedicated memory (MB)" << ","
                 << "bind max dedicated memory (MB)"
                 << ","
                 // [first]eval (VRAM)
                 << "evaluate first dedicated memory (MB)" << ","
                 << "evaluate average dedicated memory (MB)" << ","
                 << "evaluate standard deviation dedicated memory (MB)" << ","
                 << "evaluate min dedicated memory (MB)" << ","
                 << "evaluate max dedicated memory (MB)"
                 << ","

                 // load memory (SharedMemory)
                 << "load average shared memory (MB)"
                 << ","
                 // create memory (SharedMemory)
                 << "session creation average shared memory (MB)"
                 << ","
                 // [first]bind  (SharedMemory)
                 << "bind first shared memory (MB)" << ","
                 << "bind average shared memory (MB)" << ","
                 << "bind standard deviation shared memory (MB)" << ","
                 << "bind min shared memory (MB)" << ","
                 << "bind max shared memory (MB)"
                 << ","
                 // [first]evaluate (SharedMemory)
                 << "evaluate first shared memory (MB)" << ","
                 << "evaluate average shared memory (MB)" << ","
                 << "evaluate standard deviation shared memory (MB)" << ","
                 << "evaluate min shared memory (MB)" << ","
                 << "evaluate max shared memory (MB)" << ","
                 << "iteration summary path";
            for (auto metaDataPair : perfFileMetadata)
            {
                fout << "," << metaDataPair.first;
            }
            fout << std::endl;
        }

        fout << converter.to_bytes(modelPath) << "," 
             << deviceType << ","
             << converter.to_bytes(deviceName) << ","
             << deviceMemory << ","
             << inputBinding << "," 
             << inputType << ","
             << deviceCreationLocation << "," 
             << numIterations << "," 
             << numberLoadIterations << ","
             << numberCreateSessionIterations << "," 
             // load (ms)
             << averageLoadTime << "," 
             // create (ms)
             << averageCreateSessionTime << "," 
             // [first]bind (ms)
             << averageFirstBindTime << ","
             << (numIterations <= 1 ? NAN : averageBindTime) << "," // std::numeric_limits<double>::quiet_NaN()
             << (numIterations <= 1 ? NAN : stdevBindTime) << ","
             << (numIterations <= 1 ? NAN : minBindTime) << "," 
             << (numIterations <= 1 ? NAN : maxBindTime) << ","
             // [first]evaluate (ms)
             << averageFirstEvalTime << "," 
             << (numIterations <= 1 ? NAN : averageEvalTime) << "," 
             << (numIterations <= 1 ? NAN : stdevEvalTime) << "," 
             << (numIterations <= 1 ? NAN : minEvalTime) << "," 
             << (numIterations <= 1 ? NAN : maxEvalTime)
             << ","

             // load memory (WorkingSetMemory)
             << averageLoadWorkingSetMemoryUsage << "," 
             // create memory (WorkingSetMemory)
             << averageCreateSessionWorkingSetMemoryUsage << ","
             // [first]bind  (WorkingSetMemory) 
             << averageFirstBindWorkingSetMemoryUsage << "," 
             << (numIterations <= 1 ? NAN : averageBindWorkingSetMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : stdevBindWorkingSetMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : minBindWorkingSetMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : maxBindWorkingSetMemoryUsage)
             << ","
             // [first]eval (WS)
             << averageFirstEvalWorkingSetMemoryUsage << "," 
             << (numIterations <= 1 ? NAN : averageEvalWorkingSetMemoryUsage) << "," 
             << (numIterations <= 1 ? NAN : stdevEvalWorkingSetMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : minEvalWorkingSetMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : maxEvalWorkingSetMemoryUsage)
             << ","

             // load memory (VRAM)
             << averageLoadDedicatedMemoryUsage << "," 
             // create memory (VRAM)
             << averageCreateSessionDedicatedMemoryUsage << "," 
             // [first]bind  (VRAM)
             << averageFirstBindDedicatedMemoryUsage << "," 
             << (numIterations <= 1 ? NAN : averageBindDedicatedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : stdevBindDedicatedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : minBindDedicatedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : maxBindDedicatedMemoryUsage)
             << ","
             // [first]evaluate (VRAM)
             << averageFirstEvalDedicatedMemoryUsage << "," 
             << (numIterations <= 1 ? NAN : averageEvalDedicatedMemoryUsage) << "," 
             << (numIterations <= 1 ? NAN : stdevEvalDedicatedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : minEvalDedicatedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : maxEvalDedicatedMemoryUsage)
             << ","

             // load memory (SharedMemory)
             << averageLoadSharedMemoryUsage << "," 
             // create memory (SharedMemory)
             << averageCreateSessionSharedMemoryUsage << ","
             // [first]bind  (SharedMemory)
             << averageFirstBindSharedMemoryUsage << ","
             << (numIterations <= 1 ? NAN : averageBindSharedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : stdevBindSharedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : minBindSharedMemoryUsage) << "," 
             << (numIterations <= 1 ? NAN : maxBindSharedMemoryUsage)
             << ","
             // [first]evaluate (SharedMemory)
             << averageFirstEvalSharedMemoryUsage << "," 
             << (numIterations <= 1 ? NAN : averageEvalSharedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : stdevEvalSharedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : minEvalSharedMemoryUsage) << ","
             << (numIterations <= 1 ? NAN : maxEvalSharedMemoryUsage) << ","
             << converter.to_bytes(m_csvFileNamePerIterationSummary);
        for (auto metaDataPair : perfFileMetadata)
        {
            fout << "," << metaDataPair.second;
        }
        fout << std::endl;
        fout.close();
    }
}
