#ifndef LIBNEBULABROTGEN_H
#define LIBNEBULABROTGEN_H

#include "stb_image_write.h"
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <mutex>
#include <memory>
#include <complex>

void logMessage(const std::string& message);

class NebulabrotChannelBuffer {
public:
  NebulabrotChannelBuffer(size_t width, size_t height);
  NebulabrotChannelBuffer(const NebulabrotChannelBuffer& other);
  NebulabrotChannelBuffer& operator=(const NebulabrotChannelBuffer& other);

  void clear();
  uint32_t* getData();
  uint32_t getMaxValue() const;
  bool mergeWith(const NebulabrotChannelBuffer& other);
  bool toStream(std::ostream& os);
  bool fromStream(std::istream& is);
  void updateMaxValue();
  size_t completed_iterations;
private:
  std::vector<uint32_t> data;
  uint32_t max_value;
  std::unique_ptr<std::mutex> mergeMutex;
};

class NebulabrotChannelCollection {
public:
  NebulabrotChannelCollection(size_t width, size_t height);
  bool loadFile(const std::string& filename);
  bool saveFile(const std::string& filename);
  void merge(const NebulabrotChannelCollection& other);

  std::map<std::string, NebulabrotChannelBuffer> channels;

  inline size_t getWidth() const { return width; }
  inline size_t getHeight() const { return height; }
private:
  size_t width;
  size_t height;
};

//typedef void(*InnerFunc)(double*, double*, double, double);

typedef void(*InnerFunc)(std::complex<double>&, std::complex<double>);

struct InnerFunctionData {
  InnerFunctionData(InnerFunc ptr, double cost = 1.0);
  InnerFunc ptr;
  double cost;
};

struct NebulabrotIterationData {
  NebulabrotIterationData(size_t inner_iterations, size_t renderer_iterations, const InnerFunctionData& func);
  double getCost() const;
  size_t inner_iterations;
  size_t renderer_iterations;
  InnerFunctionData func;
};

struct NebulabrotRenderChannel {
  NebulabrotRenderChannel(const NebulabrotIterationData& data, const std::string& name);
  double cost;
  std::string name;
  NebulabrotIterationData data;
  NebulabrotChannelBuffer* buf;
  size_t unfinished_jobs;
  size_t threads_on_channel;
  std::vector<size_t> iteration_jobs;
  inline bool operator<(const NebulabrotRenderChannel& other) const;
};

struct IterJobData {
  IterJobData();
  NebulabrotIterationData iter_data;
  NebulabrotChannelBuffer* buf;
  size_t num_channel;
};

class NebulabrotRenderingManager {
public:
  NebulabrotRenderingManager(double xmid, double ymid, double factor,
                             double random_radius, double norm_limit,
                             size_t width, size_t height, size_t num_threads);
  bool add(const std::string& name, const NebulabrotIterationData& iteration_data);
  NebulabrotChannelCollection execute();

private:
  void threadFunction(size_t start_channel, size_t thread_num);
  IterJobData getAJob(size_t preferred_channel);
  void notifyJobCompletion(size_t channel_id);
  void leaveChannel(size_t previous_channel, size_t new_channel, size_t thread_num, const NebulabrotChannelBuffer& buf);

  std::vector<NebulabrotRenderChannel> channels;
  std::mutex execute_mutex;
  std::mutex job_getter_mutex;
  std::mutex notify_mutex;
  std::mutex leave_mutex;
  std::chrono::time_point<std::chrono::high_resolution_clock> render_start;
  int last_notification_elapsed;
  size_t jobs_total;
  size_t jobs_finished;
  double xmid;
  double ymid;
  double factor;
  double random_radius;
  double norm_limit;
  size_t width;
  size_t height;
  size_t num_threads;
};

class ImageColorBuffer {
public:
  ImageColorBuffer(size_t width, size_t height);
  ImageColorBuffer(ImageColorBuffer&& other) noexcept;
  ImageColorBuffer& operator=(ImageColorBuffer&& other) noexcept;
  ImageColorBuffer(const ImageColorBuffer& other) = delete;
  ImageColorBuffer& operator=(const ImageColorBuffer& other) = delete;
  ~ImageColorBuffer();
  inline uint32_t* getData() { return data; }
  bool saveFile(const std::string& filename);

private:
  size_t width;
  size_t height;
  uint32_t* data;
};

//return: RGBA value
//arg1: array of values corresponding to channels (val/max)
typedef uint32_t (*ImagePixelFunc)(double*);

//arg1: amount of pixels
//arg2: array of pointers to iterations results for each channel
//arg3: array of maximum values for each channel
//arg4: result pointer to RGBA
typedef void (*WholeImageFunc)(size_t, uint32_t**, uint32_t*, uint32_t*);

enum ImageMode {
  PIXEL_FUNC = 0, IMAGE_FUNC = 1
};

struct ImageFunctionData {
  union ImageFunc { ImagePixelFunc pixel; WholeImageFunc whole; };
  ImageFunctionData(ImagePixelFunc ptr, const std::vector<std::string>& channel_names, const std::vector<double>& desired_max, double cost = 1.0);
  ImageFunctionData(WholeImageFunc ptr, const std::vector<std::string>& channel_names, const std::vector<double>& desired_max, double cost = 1.0);
  ImageFunc ptr;
  ImageMode mode;
  std::vector<std::string> channel_names;
  std::vector<double> desired_max;
  double cost;
};

struct ImageOutputData {
  ImageOutputData(const ImageFunctionData&, NebulabrotChannelCollection* channels);
  double getCost() const;
  ImageFunctionData func;
  NebulabrotChannelCollection* channels;
};

struct ImageRenderChannel {
  ImageRenderChannel(const ImageOutputData& output_data, const std::string& filename);
  double cost;
  std::string filename;
  ImageOutputData output_data;
  ImageColorBuffer* buf;
  bool failed;
  size_t unfinished_jobs;
  size_t threads_on_channel;
  std::vector<std::pair<size_t, size_t>> render_jobs;
  inline bool operator<(const ImageRenderChannel& other) const;
};

struct ImageJobData {
  ImageJobData();
  ImageOutputData output_data;
  size_t start_index;
  size_t end_index;
  size_t num_image;
};

class ImageRenderingManager {
public:
  explicit ImageRenderingManager(size_t threads);
  bool add(const std::string& filename, const ImageOutputData& image_data);
  void execute();

private:
  void threadFunction(size_t start_image, size_t thread_num);
  ImageJobData getAJob(size_t preferred_image);
  void notifyJobCompletion(size_t image_id);
  void failImage(size_t image_id);
  void doJob(const ImageJobData& job, size_t image_num);

  std::vector<ImageRenderChannel> images;
  std::mutex execute_mutex;
  std::mutex job_getter_mutex;
  std::mutex notify_mutex;
  std::chrono::time_point<std::chrono::high_resolution_clock> render_start;
  int last_notification_elapsed;
  size_t jobs_total;
  size_t jobs_finished;
  size_t num_threads;
};

#endif
