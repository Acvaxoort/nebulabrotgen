#include <thread>
#include <array>
#include <iostream>
#include <chrono>
#include <cmath>
#include <fstream>
#include <complex>
#include "libnebulabrotgen.h"

const double xmid = 0;
const double ymid = 0;
const double size = 8;
const size_t width = 1920;
const size_t height = 1080;
const size_t iterations = 1000000;
const double random_radius = 4;
const double norm_limit = 256;

inline double limit(double value) {
  return std::min(1.0, std::max(0.0, value));
}

inline double mapv(double value, double in_diff, double out_diff) {
  return value * out_diff / in_diff;
}

typedef std::complex<double> complex;

void func(std::complex<double>& z, std::complex<double> c) {
  z = z * c + c;
}

uint32_t img_func(double* values) {
  uint8_t result[4];
  result[0] = (uint8_t) (255.0 * limit(values[3] * 0.375 + sqrt(values[4] * 0.375) + sqrt(values[5]) * 0.5 + sqrt(values[6]) * 0.675));
  result[1] = (uint8_t) (255.0 * limit(values[1] * 0.375 + sqrt(values[2]) * 0.375 + sqrt(values[3]) * 0.5 + sqrt(values[4]) * 0.375 + values[5] * 0.375));
  result[2] = (uint8_t) (255.0 * limit(sqrt(values[0]) * 0.625 + sqrt(values[1]) * 0.5 + sqrt(values[2]) * 0.375 + values[3] * 0.375));
  result[3] = 0xff;
  return * (uint32_t*) result;
}

uint32_t img_monochrome(double* values) {
  uint8_t result[4];
  double val = sqrt(values[0]);
  result[0] = (uint8_t) (255.0 * val);
  result[1] = (uint8_t) (255.0 * val);
  result[2] = (uint8_t) (255.0 * val);
  result[3] = 0xff;
  return * (uint32_t*) result;
}

/*
#include <dlfcn.h>

std::string function_code = R"(
void temp_func(double* buf_x, double* buf_y, double add_x, double add_y) {
  double x = *buf_x;
  double y = *buf_y;
  double x2 = x * x - y * y;
  double y2 = 2 * x * y;
  *buf_x = y2 / (x + 1) + add_x;
  *buf_y = x2 / (x + 1) + add_y;
}
)";

std::string function_code2 = R"(
#include <complex>

typedef std::complex<double> complex;
typedef float real;

extern "C"
void temp_func(std::complex<double>& x, std::complex<double> c) {
  x = x * x + c;
}
)";*/

//const std::string func_name = "z^2 + c at 0, f=4";

typedef std::array<std::array<uint32_t, width>, height> arrwh;

void func_whole(size_t num_pixels, uint32_t** pixels, uint32_t* channels_max, uint32_t* result) {
  for (size_t i = 0; i < num_pixels; ++i) {
    uint8_t* c = (uint8_t*) (result + i);
    c[0] = (uint8_t) (255.0 * pixels[0][i] / channels_max[0]);
    c[1] = (uint8_t) (255.0 * pixels[0][i] / channels_max[0]);
    c[2] = (uint8_t) (255.0 * pixels[0][i] / channels_max[0]);
    c[3] = 0xff;
  }
}

int main() {
  /*
  auto time0 = std::chrono::high_resolution_clock::now();

  std::ofstream ofs;
  ofs.open("temp_func.c", std::ios::trunc);
  if (!ofs.is_open()) {
    logMessage("Failed to save temporary function file");
    exit(EXIT_FAILURE);
  }
  ofs << function_code2;
  if (!ofs.good()) {
    logMessage("Error while writing temporary function file");
    exit(EXIT_FAILURE);
  }
  ofs.close();

  int compilation_error = system("g++ -shared -Ofast -march=native -o temp_func.so temp_func.c");
  if (compilation_error) {
    logMessage("Error while compiling temporary function file");
    exit(EXIT_FAILURE);
  }

  void* lib = dlopen("./temp_func.so", RTLD_NOW);
  if (!lib) {
    logMessage("Error while loading temporary function library");
    logMessage(dlerror());
    exit(EXIT_FAILURE);
  }

  InnerFunc temp_func = (InnerFunc) dlsym(lib, "temp_func");
  if (!temp_func) {
    logMessage("Error while loading temporary function symbol");
    exit(EXIT_FAILURE);
  }

  std::ofstream ofs2;
  ofs.open("temp_func.c", std::ios::trunc);
  if (!ofs.is_open()) {
    logMessage("Failed to save temporary function file");
    exit(EXIT_FAILURE);
  }
  ofs << function_code2;
  if (!ofs.good()) {
    logMessage("Error while writing temporary function file");
    exit(EXIT_FAILURE);
  }
  ofs.close();

  int compilation_error2 = system("g++ -shared -Ofast -march=native -o temp_func.so temp_func.c");
  if (compilation_error2) {
    logMessage("Error while compiling temporary function file");
    exit(EXIT_FAILURE);
  }

  void* lib2 = dlopen("./temp_func.so", RTLD_NOW);
  if (!lib2) {
    logMessage("Error while loading temporary function library");
    logMessage(dlerror());
    exit(EXIT_FAILURE);
  }

  InnerFunc temp_func2 = (InnerFunc) dlsym(lib, "temp_func");
  if (!temp_func2) {
    logMessage("Error while loading temporary function symbol");
    exit(EXIT_FAILURE);
  }

  double time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - time0).count();
  logMessage("Loaded function in " + std::to_string(time) + "seconds");
*/

  size_t threads = std::thread::hardware_concurrency();

  NebulabrotRenderingManager manager(xmid, ymid, size, random_radius, norm_limit, width, height, threads);
  manager.add("i1", NebulabrotIterationData(32, iterations, InnerFunctionData(func, 1)));
  manager.add("i2", NebulabrotIterationData(45, iterations, InnerFunctionData(func, 1)));
  manager.add("i3", NebulabrotIterationData(64, iterations, InnerFunctionData(func, 1)));
  manager.add("i4", NebulabrotIterationData(91, iterations, InnerFunctionData(func, 1)));
  manager.add("i5", NebulabrotIterationData(128, iterations, InnerFunctionData(func, 1)));
  manager.add("i6", NebulabrotIterationData(181, iterations, InnerFunctionData(func, 1)));
  manager.add("i7", NebulabrotIterationData(256, iterations, InnerFunctionData(func, 1)));
  auto collection = manager.execute();

  //NebulabrotChannelCollection collection_raw(width, height);
  //collection_raw.loadFile("raw");
  //collection.merge(collection_raw);
  //collection.saveFile("raw");

  ImageRenderingManager img_manager(12);
  img_manager.add("iall", ImageOutputData(ImageFunctionData(img_func, {"i1", "i2", "i3", "i4", "i5", "i6", "i7"}, {}), &collection));
  img_manager.add("i1", ImageOutputData(ImageFunctionData(img_monochrome, {"i1"}, {}), &collection));
  img_manager.add("i2", ImageOutputData(ImageFunctionData(img_monochrome, {"i2"}, {}), &collection));
  img_manager.add("i3", ImageOutputData(ImageFunctionData(img_monochrome, {"i3"}, {}), &collection));
  img_manager.add("i4", ImageOutputData(ImageFunctionData(img_monochrome, {"i4"}, {}), &collection));
  img_manager.add("i5", ImageOutputData(ImageFunctionData(img_monochrome, {"i5"}, {}), &collection));
  img_manager.add("i6", ImageOutputData(ImageFunctionData(img_monochrome, {"i6"}, {}), &collection));
  img_manager.add("i7", ImageOutputData(ImageFunctionData(img_monochrome, {"i7"}, {}), &collection));


  img_manager.execute();

  return 0;
}
