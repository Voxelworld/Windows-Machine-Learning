#pragma once
#include "CommandLineArgs.h"

struct LearningModelDeviceWithMetadata
{
    LearningModelDevice LearningModelDevice;
    DeviceType DeviceType;
    DeviceCreationLocation DeviceCreationLocation;
    std::wstring DeviceName;
    size_t DedicatedMemory;
};

void PopulateLearningModelDeviceList(CommandLineArgs& args, std::vector<LearningModelDeviceWithMetadata>& deviceList);