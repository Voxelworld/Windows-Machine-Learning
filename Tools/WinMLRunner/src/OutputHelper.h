#if defined(_AMD64_)
// PIX markers only work on amd64
#include <DXProgrammableCapture.h>
#endif
#include "TimerHelper.h"
#include "LearningModelDeviceHelper.h"
// Stores performance information and handles output to the command line and CSV files.
class OutputHelper
{
public:
    OutputHelper(int numIterations)
    {
        m_clockLoadTimes.resize(numIterations, NAN); //0.0);
        m_clockBindTimes.resize(numIterations, NAN); // 0.0);
        m_clockEvalTimes.resize(numIterations, NAN); // 0.0);
        m_CPUWorkingDiff.resize(numIterations, NAN); // 0.0);
        m_CPUWorkingStart.resize(numIterations, NAN); // 0.0);
        m_GPUSharedDiff.resize(numIterations, NAN);   // 0.0);
        m_GPUDedicatedDiff.resize(numIterations, NAN); // 0.0);
        m_GPUSharedStart.resize(numIterations, NAN);   // 0.0);
        m_outputResult.resize(numIterations, "");
        m_outputTensorHash.resize(numIterations, 0);
    }

    void PrintLoadingInfo(const std::wstring& modelPath) const;
    void PrintBindingInfo(uint32_t iteration, DeviceType deviceType, InputBindingType inputBindingType,
                          InputDataType inputDataType, DeviceCreationLocation deviceCreationLocation,
                          const std::string& status) const;
    void PrintEvaluatingInfo(uint32_t iteration, DeviceType deviceType, InputBindingType inputBindingType,
                             InputDataType inputDataType, DeviceCreationLocation deviceCreationLocation,
                             const std::string& status) const;
    void PrintModelInfo(const std::wstring& modelPath, const LearningModel& model) const;
    void PrintFeatureDescriptorInfo(const ILearningModelFeatureDescriptor& descriptor) const;
    void PrintHardwareInfo() const;
    void PrintResults(const Profiler<WINML_MODEL_TEST_PERF>& profiler, uint32_t numIterations, DeviceType deviceType,
                      InputBindingType inputBindingType, InputDataType inputDataType,
                      DeviceCreationLocation deviceCreationLocation, bool isPerformanceConsoleOutputVerbose) const;
    void SaveLoadTimes(Profiler<WINML_MODEL_TEST_PERF>& profiler, uint32_t iterNum);
    void SaveBindTimes(Profiler<WINML_MODEL_TEST_PERF>& profiler, uint32_t iterNum);
    void SaveEvalPerformance(Profiler<WINML_MODEL_TEST_PERF>& profiler, uint32_t iterNum);
    void SaveResult(uint32_t iterationNum, std::string result, size_t hashcode);
    void SetDefaultPerIterationFolder(const std::wstring& folderName);
    void SetDefaultCSVFileNamePerIteration();
    std::wstring GetDefaultCSVFileNamePerIteration() const;
    std::wstring GetCsvFileNamePerIterationResult() const;
    void SetDefaultCSVIterationResult(uint32_t iterationNum, const CommandLineArgs& args, const std::wstring& deviceName, const std::wstring& featureName);
    void SetCSVFileName(const std::wstring& fileName);
    void WritePerIterationPerformance(const CommandLineArgs& args, const std::wstring model,
                                      const std::wstring imagePath);
    void WritePerformanceDataToCSV(const Profiler<WINML_MODEL_TEST_PERF>& profiler, int numIterations,
                                   const std::wstring& modelPath, const std::string& deviceType, const std::string& inputBinding,
                                   const std::string& inputType, const std::string& deviceCreationLocation,
                                   const std::wstring& deviceName, size_t deviceMemory,
                                   const std::vector<std::pair<std::string, std::string>>& perfFileMetadata) const;
    static void PopulateLearningModelDevice(LearningModelDeviceWithMetadata& device);
    static void PrintLearningModelDevice(const LearningModelDeviceWithMetadata& device);
    static std::wstring FeatureDescriptorToString(const ILearningModelFeatureDescriptor& descriptor);
    static bool doesDescriptorContainFP16(const ILearningModelFeatureDescriptor& descriptor);
    static bool doesModelContainFP16(const LearningModel& model);
    template <typename T>
    static void ProcessTensorResult(const CommandLineArgs& args, const void* buffer, const uint32_t uCapacity,
                                    std::vector<std::pair<float, int>>& maxValues, std::ofstream& fout, unsigned int k,
                                    bool saveFullTensor);
    static std::string ToString(std::wstring s);
    static std::wstring ToWString(std::string s);
    static std::string ToString(const std::vector<int64_t>& shape);
    // PIX markers only work on amd64
#if defined(_AMD64_)
    com_ptr<IDXGraphicsAnalysis>& GetGraphicsAnalysis() { return m_graphicsAnalysis; }
#endif
private:
    std::vector<double> m_clockLoadTimes;
    std::vector<double> m_clockBindTimes;
    std::vector<double> m_clockEvalTimes;
    std::wstring m_csvFileName;
    std::wstring m_csvFileNamePerIterationSummary;
    std::wstring m_csvFileNamePerIterationResult;
    std::wstring m_folderNamePerIteration;
    std::wstring m_fileNameResultDevice;

    bool m_silent = false;

    std::vector<double> m_EvalTime;
    std::vector<double> m_CPUWorkingDiff;
    std::vector<double> m_CPUWorkingStart;
    std::vector<double> m_GPUSharedDiff;
    std::vector<double> m_GPUSharedStart;
    std::vector<double> m_GPUDedicatedDiff;
    std::vector<std::string> m_outputResult;
    std::vector<size_t> m_outputTensorHash;

#if defined(_AMD64_)
    // PIX markers only work on amd64
    com_ptr<IDXGraphicsAnalysis> m_graphicsAnalysis = nullptr;
#endif
};