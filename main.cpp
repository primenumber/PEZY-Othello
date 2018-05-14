#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <fstream>
#include <random>
#include <utility>
#include <omp.h>
#include <PZSDKHelper.h>
#include <pzcl/pzcl_ocl_wrapper.h>
#include "types.hpp"
#include "solver.hpp"
#include "to_board.hpp"

// parameters
constexpr size_t lower_stack_size = 10;
constexpr size_t upper_stack_size = 5;

int popcnt(uint64_t x) {
  x = ((x & UINT64_C(0xAAAAAAAAAAAAAAAA)) >>  1) + (x & UINT64_C(0x5555555555555555));
  x = ((x & UINT64_C(0xCCCCCCCCCCCCCCCC)) >>  2) + (x & UINT64_C(0x3333333333333333));
  x = ((x & UINT64_C(0xF0F0F0F0F0F0F0F0)) >>  4) + (x & UINT64_C(0x0F0F0F0F0F0F0F0F));
  x = ((x & UINT64_C(0xFF00FF00FF00FF00)) >>  8) + (x & UINT64_C(0x00FF00FF00FF00FF));
  x = ((x & UINT64_C(0xFFFF0000FFFF0000)) >> 16) + (x & UINT64_C(0x0000FFFF0000FFFF));
  return ((x >> 32) + x) & UINT64_C(0x00000000FFFFFFFF);
}

int score(const Board& bd) {
  int me = popcnt(bd.me);
  int op = popcnt(bd.op);
  if (me == op) return 0;
  if (me > op) return 64 - 2*op;
  else return -64 + 2*me;
}

uint64_t empty(const Board& bd) {
  return ~(bd.me | bd.op);
}

uint64_t upper_bit(uint64_t x) {
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  return x & ~(x >> 1);
}

template <int simd_index>
uint64_t flip_impl(const Board& bd, int pos) {
  uint64_t OM = bd.op;
  if (simd_index) OM &= UINT64_C(0x7E7E7E7E7E7E7E7E);
  constexpr uint64_t mask1[] = {
    UINT64_C(0x0080808080808080),
    UINT64_C(0x7F00000000000000),
    UINT64_C(0x0102040810204000),
    UINT64_C(0x0040201008040201)
  };
  uint64_t mask = mask1[simd_index] >> (63 - pos);
  uint64_t outflank = upper_bit(~OM & mask) & bd.me;
  uint64_t flipped = (-outflank << 1) & mask;
  constexpr uint64_t mask2[] = {
    UINT64_C(0x0101010101010100),
    UINT64_C(0x00000000000000FE),
    UINT64_C(0x0002040810204080),
    UINT64_C(0x8040201008040200)
  };
  mask = mask2[simd_index] << pos;
  outflank = mask & ((OM | ~mask) + 1) & bd.me;
  flipped |= (outflank - (outflank != 0)) & mask;
  return flipped;
}

uint64_t flip(const Board& bd, int pos) {
  return flip_impl<0>(bd, pos) | flip_impl<1>(bd, pos) | flip_impl<2>(bd, pos) | flip_impl<3>(bd, pos);
}

Board move(const Board& bd, uint64_t flips, int pos) {
  return Board(bd.op ^ flips, (bd.me ^ flips) | (UINT64_C(1) << pos));
}

Board unmove(const Board& bd, uint64_t flips, int pos) {
  return Board(bd.op ^ flips ^ (UINT64_C(1) << pos), bd.me ^ flips);
}

Board move_pass(const Board& bd) {
  return Board(bd.op, bd.me);
}

Board unmove_pass(const Board &bd) {
  return Board(bd.op, bd.me);
}

struct Search {
  Board bd;
  signed char alpha_beta(signed char alpha, signed char beta, bool passed_prev = false) {
    signed char result = -64;
    bool pass = true;
    for (uint64_t bits = empty(bd); bits; bits &= bits-1) {
      uint64_t flips, pos;
      {
        uint64_t bit = bits & -bits;
        pos = popcnt(bit - 1);
        flips = flip(bd, pos);
      }
      if (flips) {
        pass = false;
        bd = move(bd, flips, pos);
        result = std::max(result, (signed char)-alpha_beta(-beta, -alpha));
        bd = unmove(bd, flips, pos);
        if (result >= beta) return result;
        alpha = std::max(alpha, result);
      }
    }
    if (pass) {
      if (passed_prev) {
        return score(bd);
      } else {
        bd = move_pass(bd);
        result = -alpha_beta(-beta, -alpha, true);
        bd = unmove_pass(bd);
      }
    }
    return result;
  }
};

constexpr int MAX_BIN_SIZE = 1000000;

const char *getErrorString(cl_int error)
{
  switch(error){
    case 0: return "CL_SUCCESS";
    case -1: return "CL_DEVICE_NOT_FOUND";
    case -2: return "CL_DEVICE_NOT_AVAILABLE";
    case -3: return "CL_COMPILER_NOT_AVAILABLE";
    case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5: return "CL_OUT_OF_RESOURCES";
    case -6: return "CL_OUT_OF_HOST_MEMORY";
    case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8: return "CL_MEM_COPY_OVERLAP";
    case -9: return "CL_IMAGE_FORMAT_MISMATCH";
    case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11: return "CL_BUILD_PROGRAM_FAILURE";
    case -12: return "CL_MAP_FAILURE";
    case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15: return "CL_COMPILE_PROGRAM_FAILURE";
    case -16: return "CL_LINKER_NOT_AVAILABLE";
    case -17: return "CL_LINK_PROGRAM_FAILURE";
    case -18: return "CL_DEVICE_PARTITION_FAILED";
    case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
    case -30: return "CL_INVALID_VALUE";
    case -31: return "CL_INVALID_DEVICE_TYPE";
    case -32: return "CL_INVALID_PLATFORM";
    case -33: return "CL_INVALID_DEVICE";
    case -34: return "CL_INVALID_CONTEXT";
    case -35: return "CL_INVALID_QUEUE_PROPERTIES";
    case -36: return "CL_INVALID_COMMAND_QUEUE";
    case -37: return "CL_INVALID_HOST_PTR";
    case -38: return "CL_INVALID_MEM_OBJECT";
    case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40: return "CL_INVALID_IMAGE_SIZE";
    case -41: return "CL_INVALID_SAMPLER";
    case -42: return "CL_INVALID_BINARY";
    case -43: return "CL_INVALID_BUILD_OPTIONS";
    case -44: return "CL_INVALID_PROGRAM";
    case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46: return "CL_INVALID_KERNEL_NAME";
    case -47: return "CL_INVALID_KERNEL_DEFINITION";
    case -48: return "CL_INVALID_KERNEL";
    case -49: return "CL_INVALID_ARG_INDEX";
    case -50: return "CL_INVALID_ARG_VALUE";
    case -51: return "CL_INVALID_ARG_SIZE";
    case -52: return "CL_INVALID_KERNEL_ARGS";
    case -53: return "CL_INVALID_WORK_DIMENSION";
    case -54: return "CL_INVALID_WORK_GROUP_SIZE";
    case -55: return "CL_INVALID_WORK_ITEM_SIZE";
    case -56: return "CL_INVALID_GLOBAL_OFFSET";
    case -57: return "CL_INVALID_EVENT_WAIT_LIST";
    case -58: return "CL_INVALID_EVENT";
    case -59: return "CL_INVALID_OPERATION";
    case -60: return "CL_INVALID_GL_OBJECT";
    case -61: return "CL_INVALID_BUFFER_SIZE";
    case -62: return "CL_INVALID_MIP_LEVEL";
    case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64: return "CL_INVALID_PROPERTY";
    case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66: return "CL_INVALID_COMPILER_OPTIONS";
    case -67: return "CL_INVALID_LINKER_OPTIONS";
    case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

    case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
    case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
    default: return "Unknown OpenCL error";
  }
}

using pfnPezyExtSetPerThreadStackSize = CL_API_ENTRY cl_int (CL_API_CALL *) (pzcl_kernel kernel, size_t size);

std::string to_string(const Board& bd) {
  std::string res;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      int index = i*8+j;
      if ((bd.me >> index) & 1) {
        res += 'x';
      } else if ((bd.op >> index) & 1) {
        res += 'o';
      } else {
        res += ' ';
      }
    }
    res += '\n';
  }
  return res;
}

int main(int argc, char **argv) {
  std::ifstream fin(argv[1]);
  std::size_t N;
  fin >> N;
  std::cerr << "N = " << N << std::endl;
  std::vector<AlphaBetaProblem> problems;
  std::vector<std::string> board_str_vec;
  for (std::size_t i = 0; i < N; ++i) {
    std::string b81;
    fin >> b81;
    const Board bd = toBoard(b81.c_str());
    problems.push_back((AlphaBetaProblem){bd.me, bd.op, -64, 64});
    board_str_vec.push_back(b81);
  }

  cl_platform_id platform_id = nullptr;
  cl_uint num_platforms = 0;
  cl_int result = 0;
  result = clGetPlatformIDs(1, &platform_id, &num_platforms);
  std::cerr << "Number of platforms: " << num_platforms << std::endl;

  cl_uint num_devices = 0;
  result = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 0, nullptr, &num_devices);
  std::cerr << "Number of devices: " << num_devices << std::endl;

  std::vector<cl_device_id> device_ids(num_devices);
  result = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, num_devices, device_ids.data(), nullptr);

  std::cerr << "load program" << std::endl;
  unsigned char *binary = (unsigned char *)malloc(MAX_BIN_SIZE * sizeof(char));
  FILE *fp = fopen("kernel.sc2/solver.pz", "rb");
  std::size_t size = fread(binary, sizeof(char), MAX_BIN_SIZE, fp);
  fclose(fp);

  std::vector<cl_context> contexts;
  std::vector<cl_command_queue> command_queues;
  std::vector<cl_program> programs;
  std::vector<cl_kernel> kernels;
  std::vector<cl_mem> mem_probs;
  std::vector<cl_mem> mem_results;
  std::vector<cl_mem> mem_ustacks;
  std::vector<cl_mem> mem_lstacks;
  std::vector<cl_mem> mem_numnodes;
  std::vector<cl_mem> mem_params;
  std::vector<cl_mem> mem_indices;

  const size_t chunk_size = (N + num_devices - 1) / num_devices;
  constexpr size_t global_work_size = 15872; // max size

  for (cl_uint i = 0; i < num_devices; ++i) {
    std::cerr << "Device " << i << std::endl;
    cl_device_id device_id = device_ids[i];

    cl_context context = clCreateContext(nullptr, 1, &device_id, nullptr, nullptr, &result);
    contexts.push_back(context);

    cl_command_queue command_queue = clCreateCommandQueue(context, device_id, 0, &result);
    command_queues.push_back(command_queue);

    std::cerr << "create program" << std::endl;
    cl_int binary_status = 0;
    cl_program program = clCreateProgramWithBinary(context, 1, &device_id, &size, (const unsigned char **)&binary, &binary_status, &result);
    programs.push_back(program);
  
    std::cerr << "create kernel" << std::endl;
    cl_kernel kernel = clCreateKernel(program, "Solve", &result);
    kernels.push_back(kernel);

    std::cerr << "create buffer" << std::endl;
    const size_t real_chunk_size = std::min(chunk_size, N - i * chunk_size);
    cl_mem memProb = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(AlphaBetaProblem)*real_chunk_size, nullptr, &result);
    cl_mem memRes = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int32_t)*real_chunk_size, nullptr, &result);
    cl_mem memUStack = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(UpperNode)*global_work_size*upper_stack_size, nullptr, &result);
    cl_mem memLStack = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(Node)*global_work_size*lower_stack_size, nullptr, &result);
    cl_mem memNodesTotal = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(uint64_t)*global_work_size, nullptr, &result);
    cl_mem memParams = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(Params), nullptr, &result);
    cl_mem memIndex = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(size_t), nullptr, &result);
    mem_probs.push_back(memProb);
    mem_results.push_back(memRes);
    mem_ustacks.push_back(memUStack);
    mem_lstacks.push_back(memLStack);
    mem_params.push_back(memParams);
    mem_numnodes.push_back(memNodesTotal);
    mem_indices.push_back(memIndex);

    clEnqueueWriteBuffer(command_queue, memProb, CL_TRUE, 0, sizeof(AlphaBetaProblem)*real_chunk_size, &problems[i * chunk_size], 0, nullptr, nullptr);

    Params params = {real_chunk_size, upper_stack_size, lower_stack_size};

    clEnqueueWriteBuffer(command_queue, memParams, CL_TRUE, 0, sizeof(Params), &params, 0, nullptr, nullptr);

    size_t zero = 0;

    clEnqueueWriteBuffer(command_queue, memIndex, CL_TRUE, 0, sizeof(size_t), &zero, 0, nullptr, nullptr);

    // pfnPezyExtSetPerThreadStackSize clExtSetPerThreadStackSize = (pfnPezyExtSetPerThreadStackSize)clGetExtensionFunctionAddress("pezy_set_per_thread_stack_size");
    // constexpr size_t per_thread_stack = 0x1000;
    // result = clExtSetPerThreadStackSize(kernel, per_thread_stack);
    // if (result != CL_SUCCESS) {
    //   std::cerr << getErrorString(result) << std::endl;
    // }
  }

  free(binary);

  auto start = std::chrono::system_clock::now();
  std::cerr << "start" << std::endl;
  for (cl_uint i = 0; i < num_devices; ++i) {
    const size_t real_chunk_size = std::min(chunk_size, N - i * chunk_size);

    clSetKernelArg(kernels[i], 0, sizeof(cl_mem), (void *)&mem_probs[i]);
    clSetKernelArg(kernels[i], 1, sizeof(cl_mem), (void *)&mem_results[i]);
    clSetKernelArg(kernels[i], 2, sizeof(cl_mem), (void *)&mem_ustacks[i]);
    clSetKernelArg(kernels[i], 3, sizeof(cl_mem), (void *)&mem_lstacks[i]);
    clSetKernelArg(kernels[i], 4, sizeof(cl_mem), (void *)&mem_params[i]);
    clSetKernelArg(kernels[i], 5, sizeof(cl_mem), (void *)&mem_numnodes[i]);
    clSetKernelArg(kernels[i], 6, sizeof(cl_mem), (void *)&mem_indices[i]);
  
    result = clEnqueueNDRangeKernel(command_queues[i], kernels[i], 1, nullptr, &global_work_size, nullptr, 0, nullptr, nullptr);
    if (result != CL_SUCCESS) {
      std::cerr << "kernel launch error: " << getErrorString(result) << std::endl;
    }
  }

  std::vector<int32_t> results(N);
  for (cl_uint i = 0; i < num_devices; ++i) {
    const size_t real_chunk_size = std::min(chunk_size, N - i * chunk_size);

    result = clEnqueueReadBuffer(command_queues[i], mem_results[i], CL_TRUE, 0, sizeof(int32_t)*real_chunk_size, &results[i * chunk_size], 0, nullptr, nullptr);
    if (result != CL_SUCCESS) {
      std::cerr << "read buffer error: " << getErrorString(result) << std::endl;
    }
  }
  auto end = std::chrono::system_clock::now();
  double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();
  std::cerr << "elapsed: " << elapsed << std::endl;

  std::ofstream ofs(argv[2]);
  ofs << N << '\n';
  uint64_t diff = 0;
  omp_lock_t write_lock;
  omp_init_lock(&write_lock);
#pragma omp parallel for
  for (std::size_t i = 0; i < N; ++i) {
    Search search = {Board(problems[i].me, problems[i].op)};
    int answer = search.alpha_beta(-64, 64);
    omp_set_lock(&write_lock);
    ofs << board_str_vec[i] << ' ' << results[i] << ' ' << answer << '\n';
    diff += abs(results[i] - answer);
    omp_unset_lock(&write_lock);
  }
  std::cerr << "diff: " << diff << std::endl;

  for (cl_uint i = 0; i < num_devices; ++i) {
    clReleaseKernel(kernels[i]);
    clReleaseProgram(programs[i]);
    clReleaseMemObject(mem_probs[i]);
    clReleaseMemObject(mem_results[i]);
    clReleaseMemObject(mem_ustacks[i]);
    clReleaseMemObject(mem_lstacks[i]);
    clReleaseMemObject(mem_params[i]);
    clReleaseMemObject(mem_numnodes[i]);
    clReleaseMemObject(mem_indices[i]);
    clReleaseCommandQueue(command_queues[i]);
    clReleaseContext(contexts[i]);
  }

  return 0;
}
