# Utilization Observatory : optional CUDA/NVML hardware-proof backend.
# Isolated: built only when UO_ENABLE_CUDA=ON (default OFF). The core never needs CUDA.
set(CUDA_TOOLKIT_ROOT "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.9")
set(CMAKE_CUDA_COMPILER "${CUDA_TOOLKIT_ROOT}/bin/nvcc.exe" CACHE FILEPATH "" FORCE)
enable_language(CUDA)
set(CMAKE_CUDA_ARCHITECTURES "120" CACHE STRING "" FORCE)

add_library(uo_cuda_proof_kernels STATIC cuda_proof.cu)
target_include_directories(uo_cuda_proof_kernels PRIVATE "${CUDA_TOOLKIT_ROOT}/include")
target_link_libraries(uo_cuda_proof_kernels PRIVATE "${CUDA_TOOLKIT_ROOT}/lib/x64/cudart.lib")

add_library(uo_cuda_nvml STATIC nvml_util.cpp)
target_include_directories(uo_cuda_nvml PRIVATE "${CUDA_TOOLKIT_ROOT}/include" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(uo_cuda_nvml PRIVATE "${CUDA_TOOLKIT_ROOT}/lib/x64/nvml.lib")

add_executable(uo_cuda_proof proof_main.cpp)
target_link_libraries(uo_cuda_proof PRIVATE uo_cuda_proof_kernels uo_cuda_nvml utilization_observatory "${CUDA_TOOLKIT_ROOT}/lib/x64/nvml.lib")
target_include_directories(uo_cuda_proof PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
