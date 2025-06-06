#include "CommandLineArgs.h"
#include "Run.h"
#include "Common.h"
#include <iostream>
#include <codecvt>
using namespace std;

void PopulateSessionOptions(LearningModelSessionOptions& sessionOptions)
{
    // Batch Size Override as 1
    try
    {
        sessionOptions.BatchSizeOverride(1);
    }
    catch (...)
    {
        std::cerr << "Batch size override couldn't be set." << std::endl;
        throw;
    }
}

int main(int argc, char *argv[])
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    vector<wstring> argsList;
    CommandLineArgs* commandLineArgs = NULL;
    for (int i = 1; i < argc; i++)
    {
        string str(argv[i]);
        argsList.push_back(converter.from_bytes(argv[i]));
    }
    try
    {
        commandLineArgs = new CommandLineArgs(argsList);
    }
    catch (hresult_error hr)
    {
        std::cout << "Creating CommandLineArgs Failed. Invalid Arguments." << std::endl;
        std::wcout << hr.message().c_str() << std::endl;
        return hr.code();
    }
    auto profiler = std::make_unique<Profiler<WINML_MODEL_TEST_PERF>>();
    vector<LearningModelDeviceWithMetadata> deviceList;
    try
    {
        PopulateLearningModelDeviceList(*commandLineArgs, deviceList);
    }
    catch (const hresult_error& error)
    {
        std::wcerr << error.message().c_str() << std::endl;
        return error.code();
    }
    LearningModelSessionOptions sessionOptions;
    PopulateSessionOptions(sessionOptions);
    int returnCode = run(*commandLineArgs, *profiler, deviceList, sessionOptions);
    delete commandLineArgs;
    return returnCode;
}
