#include "nvml_util.hpp"
#include <nvml.h>
#include <cstring>

bool nvml_init() { return nvmlInit() == NVML_SUCCESS; }
void nvml_shutdown() { (void)nvmlShutdown(); }

bool nvml_device_count(int& count) {
    unsigned int c = 0;
    if (nvmlDeviceGetCount_v2(&c) != NVML_SUCCESS) return false;
    count = static_cast<int>(c);
    return true;
}

bool nvml_sample_device(int index, NvmlDeviceInfo& info) {
    nvmlDevice_t dev;
    if (nvmlDeviceGetHandleByIndex_v2(static_cast<unsigned int>(index), &dev) != NVML_SUCCESS)
        return false;

    nvmlMemory_t mem;
    if (nvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS) {
        info.memory_total_bytes = mem.total;
        info.memory_free_bytes = mem.free;
        info.memory_supported = true;
    }

    nvmlUtilization_t util;
    if (nvmlDeviceGetUtilizationRates(dev, &util) == NVML_SUCCESS) {
        info.compute_util_percent = util.gpu;
        info.mem_util_percent = util.memory;
        info.supported = true;
    }

    unsigned int temp = 0;
    if (nvmlDeviceGetTemperature(dev, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS)
        info.temperature = static_cast<int>(temp);

    unsigned int power = 0;
    if (nvmlDeviceGetPowerUsage(dev, &power) == NVML_SUCCESS)
        info.power_mW = power;

    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    if (nvmlDeviceGetName(dev, name, NVML_DEVICE_NAME_BUFFER_SIZE) == NVML_SUCCESS)
        std::strncpy(info.name, name, sizeof(info.name) - 1);
    return true;
}
