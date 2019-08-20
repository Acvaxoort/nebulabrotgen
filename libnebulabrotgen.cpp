#include "libnebulabrotgen.h"
#include "stdcomplexrenderer.hpp"

#include <algorithm>
#include <thread>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//#define RENDERING_DEBUG
//#define IMAGE_DEBUG

inline bool file_exists(const std::string& name) {
  if (FILE *file = fopen((name + ".png").c_str(), "r")) {
    fclose(file);
    return true;
  } else {
    return false;
  }
}

void logMessage(const std::string& message) {
  std::cout<<message + "\n";
}


NebulabrotChannelBuffer::NebulabrotChannelBuffer(size_t width, size_t height)
    : completed_iterations(0), data(width*height), max_value(0), mergeMutex(new std::mutex()) {}

NebulabrotChannelBuffer::NebulabrotChannelBuffer(const NebulabrotChannelBuffer& other) {
  completed_iterations = other.completed_iterations;
  data = other.data;
  max_value = other.max_value;
  mergeMutex = std::unique_ptr<std::mutex>(new std::mutex());
}

NebulabrotChannelBuffer& NebulabrotChannelBuffer::operator=(const NebulabrotChannelBuffer& other) {
  completed_iterations = other.completed_iterations;
  data = other.data;
  max_value = other.max_value;
  mergeMutex = std::unique_ptr<std::mutex>(new std::mutex());
  return *this;
}

void NebulabrotChannelBuffer::clear() {
  std::fill(data.begin(), data.end(), 0);
}

bool NebulabrotChannelBuffer::mergeWith(const NebulabrotChannelBuffer& other) {
  std::lock_guard<std::mutex> lock(*mergeMutex);
  size_t mem_size = data.size();
  if (mem_size != other.data.size()) {
    return false;
  }
  for (size_t i = 0; i < mem_size; ++i) {
    data[i] += other.data[i];
  }
  completed_iterations += other.completed_iterations;
  return true;
}

bool NebulabrotChannelBuffer::toStream(std::ostream& os) {
  os.write((char*) &completed_iterations, sizeof(size_t));
  os.write((char*) &max_value, sizeof(size_t));
  os.write((char*) data.data(), data.size() * sizeof(uint32_t));
  return os.good();
}

bool NebulabrotChannelBuffer::fromStream(std::istream& is) {
  is.read((char*) &completed_iterations, sizeof(size_t));
  is.read((char*) &max_value, sizeof(size_t));
  is.read((char*) data.data(), data.size() * sizeof(uint32_t));
  return is.good();
}

void NebulabrotChannelBuffer::updateMaxValue() {
#ifdef RENDERING_DEBUG
  auto time_begin = std::chrono::high_resolution_clock::now();
#endif
  max_value = 0;
  size_t mem_size = data.size();
  for (size_t i = 0; i < mem_size; ++i) {
    if (data[i] > max_value) {
      max_value = data[i];
    }
  }
#ifdef RENDERING_DEBUG
  double time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - time_begin).count();
  std::cout<<"Updated max value in " + std::to_string(time) + "s\n";
#endif
}

NebulabrotChannelCollection::NebulabrotChannelCollection(size_t width, size_t height)
    : width(width), height(height) {}

bool NebulabrotChannelCollection::loadFile(const std::string& filename) {
  auto fs = std::fstream(filename, std::ios::in | std::ios::binary);
  if (!fs.is_open()) {
    std::cout<<"Unable to open raw results file: "<<filename<<"\n";
    return false;
  }
  size_t read_width;
  size_t read_height;
  fs.read((char*) &read_width, sizeof(read_width));
  fs.read((char*) &read_height, sizeof(read_height));
  if (!fs.good()) {
    std::cout<<"Error while reading raw results file: "<<filename<<"\n";
    fs.close();
    return false;
  }
  if (width != read_width || height != read_height) {
    std::cout<<"Error while loading: "<<filename<<", resolution mismatch\n";
    fs.close();
  }
  std::string channels_info;
  while (true) {
    NebulabrotChannelBuffer buf(width, height);
    size_t read_name_length = 0;
    std::string name;

    fs.read((char*) &read_name_length, sizeof(read_name_length));
    if (read_name_length >= 1024) {
      std::cout<<"Warning: channel name is "<<read_name_length<<" bytes long\n";
    }
    name.resize(read_name_length);
    fs.read(&name[0], read_name_length);

    if (!fs.good()) {
      if (fs.eof()) {
        break;
      } else {
        std::cout<<"Error while loading: "<<filename<<"\n";
        fs.close();
        return false;
      }
    }
    if (!buf.fromStream(fs)) {
      if (fs.eof()) {
        std::cout<<"Error while loading "<<name<<" from "<<filename<<", EoF reached\n";
      } else {
        std::cout<<"Error while loading "<<name<<" from "<<filename<<"\n";
      }
    }
    if (!channels_info.empty()) {
      channels_info += ", ";
    }
    auto it = channels.find(name);
    if (it == channels.end()) {
      channels.emplace(name, std::move(buf));
      channels_info += name;
    } else {
      if (!it->second.mergeWith(buf)) {
        std::cout<<"Error while loading and merging "<<name<<" from "<<filename<<": this should never happen\n";
      }
      it->second.updateMaxValue();
      channels_info += name + "(merged)";
    }
  }
  fs.close();
  std::cout<<"Loaded raw results file: "<<filename<<", channels: "<<channels_info<<"\n";
  return true;
}

bool NebulabrotChannelCollection::saveFile(const std::string& filename) {
  auto fs = std::fstream(filename, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!fs.is_open()) {
    std::cout<<"Unable to create raw results file: "<<filename<<"\n";
    return false;
  }
  fs.write((char*) &width, sizeof(width));
  fs.write((char*) &height, sizeof(height));
  std::string channels_info;
  for (auto& p : channels) {
    size_t string_len = p.first.size();
    fs.write((char*) &string_len, sizeof(string_len));
    fs.write(&p.first[0], string_len);
    if (!p.second.toStream(fs)) {
      std::cout<<"Error while saving raw results file: "<<filename<<"\n";
      fs.close();
      return false;
    }
    if (!channels_info.empty()) {
      channels_info += ", ";
    }
    channels_info += p.first;
  }
  fs.close();
  std::cout<<"Saved raw results file: "<<filename<<", channels: "<<channels_info<<"\n";
  return true;
}

void NebulabrotChannelCollection::merge(const NebulabrotChannelCollection& other) {
  std::string channels_info;
  for (auto& p : other.channels) {
    if (!channels_info.empty()) {
      channels_info += ", ";
    }
    auto it = channels.find(p.first);
    if (it == channels.end()) {
      channels.emplace(p.first, p.second);
      channels_info += p.first;
    } else {
      it->second.mergeWith(p.second);
      it->second.updateMaxValue();
      channels_info += p.first + "(merged)";
    }
  }
  std::cout << "Merged channel collection: " + channels_info + "\n";
}

InnerFunctionData::InnerFunctionData(InnerFunc ptr, double cost)
    : ptr(ptr), cost(cost) {}

NebulabrotIterationData::NebulabrotIterationData(size_t inner_iterations,
                                                 size_t renderer_iterations, const InnerFunctionData& func)
    : inner_iterations(inner_iterations), renderer_iterations(renderer_iterations), func(func) {}

double NebulabrotIterationData::getCost() const {
  return func.cost * renderer_iterations * (inner_iterations + 128.0 * std::pow(2.0, inner_iterations / 1024.0));
}

NebulabrotRenderChannel::NebulabrotRenderChannel(const NebulabrotIterationData& data, const std::string& name)
    : name(name), data(data) {
  cost = data.getCost();
}

bool NebulabrotRenderChannel::operator<(const NebulabrotRenderChannel& other) const {
  return cost < other.cost;
}

NebulabrotRenderingManager::NebulabrotRenderingManager(double xmid, double ymid, double factor,
                                                       size_t width, size_t height, size_t num_threads)
    : xmid(xmid), ymid(ymid), factor(factor),
      width(width), height(height), num_threads(num_threads) {}

bool NebulabrotRenderingManager::add(const std::string& name, const NebulabrotIterationData& iteration_data) {
  auto insert_it = channels.begin();
  for (auto it = insert_it; it != channels.end(); ++it) {
    int comp_result = name.compare(it->name);
    if (comp_result == 0) {
      std::cout<<"Error while adding iteration channel to rendering manager: name conflict (" + name + ")\n";
      return false;
    } else if (comp_result < 0) {
      insert_it = it + 1;
    }
  }
  channels.emplace(insert_it, iteration_data, name);
  return true;
}

NebulabrotChannelCollection NebulabrotRenderingManager::execute() {
  std::lock_guard<std::mutex> lock(execute_mutex);
  NebulabrotChannelCollection result(width, height);
  if (channels.empty()) {
    return result;
  }
  render_start = std::chrono::high_resolution_clock::now();
  std::string starting_message = "Computing fractal (";
  std::sort(channels.begin(), channels.end());
  double total_cost = 0.0;
  for (auto& ch : channels) {
    total_cost += ch.cost;
  }
  size_t approx_num_jobs = num_threads * 3 + static_cast<size_t>(std::log2(total_cost));
  jobs_total = 0;
  jobs_finished = 0;
  last_notification_elapsed = 0;
  for (auto& ch : channels) {
    if (ch.data.inner_iterations < 2) {
      std::cout<<"Channel " + ch.name + " has less than 2 inner iterations, the rendering would never end\n";
      continue;
    }
    auto it = result.channels.emplace_hint(result.channels.end(), ch.name, NebulabrotChannelBuffer(width, height));
    ch.buf = &it->second;
    size_t ch_jobs = std::max((size_t) 1, (size_t) (ch.cost / total_cost * approx_num_jobs));
    size_t iterations_per_job_base = ch.data.renderer_iterations / ch_jobs;
    size_t iterations_per_job_rem = ch.data.renderer_iterations % ch_jobs;
    ch.iteration_jobs.resize(ch_jobs);
    ch.unfinished_jobs = ch_jobs;
    ch.threads_on_channel = 0;
    for (size_t i = 0; i < ch_jobs; ++i) {
      if (i < iterations_per_job_rem) {
        ch.iteration_jobs[i] = iterations_per_job_base + 1;
      } else {
        ch.iteration_jobs[i] = iterations_per_job_base;
      }
    }
    if (jobs_total > 0) {
      starting_message += ", ";
    }
    starting_message += ch.name;
    jobs_total += ch_jobs;
  }
  starting_message += ")\n";
  if (jobs_total == 0) {
    std::cout<<"No channels to render\n";
    return result;
  } else {
    std::cout<<starting_message;
  }
#ifdef RENDERING_DEBUG
  std::cout<<std::endl<<"Number of segments: "<<jobs_total<<" ("<<approx_num_jobs<<")\n";
#endif
  std::vector<std::thread> threads;
  size_t temp_channel_num = channels.size() - 1;
  leave_mutex.lock();
  for (size_t i = 0; i < num_threads; ++i) {
    channels[temp_channel_num].threads_on_channel++;
    threads.emplace_back(&NebulabrotRenderingManager::threadFunction, this, temp_channel_num, i);
    if (temp_channel_num == 0) {
      temp_channel_num = channels.size() - 1;
    } else {
      temp_channel_num--;
    }
  }
  leave_mutex.unlock();
  for (size_t i = 0; i < num_threads; ++i) {
    threads[i].join();
  }
  double time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - render_start).count();
  std::cout<<"Computing ended in "<<time<<std::endl;
  return result;
}

const size_t NO_CHANNEL = (size_t) -1;

void NebulabrotRenderingManager::threadFunction(size_t start_channel, size_t thread_num) {
  std::unique_ptr<BuddhabrotRenderer<double>> renderer;
  size_t previous_channel = NO_CHANNEL;
  NebulabrotChannelBuffer buf(width, height);
#ifdef RENDERING_DEBUG
  auto time_begin = std::chrono::high_resolution_clock::now();
  std::cout<<"Thread " + std::to_string(thread_num) + " started on channel " + std::to_string(start_channel) + "\n";
#endif
  while(true) {
    IterJobData job = getAJob(start_channel);
    if (job.iter_data.renderer_iterations == 0) {
      if (previous_channel < channels.size()) {
        leaveChannel(previous_channel, NO_CHANNEL, thread_num, buf);
      }
#ifdef RENDERING_DEBUG
      std::cout<<"Thread " + std::to_string(thread_num) + " terminated (no more jobs)\n";
#endif
      return;
    }
    start_channel = job.num_channel;
    if (previous_channel != start_channel) {
      renderer.reset(new BuddhabrotRenderer<double>
               (width, height, job.iter_data.inner_iterations, 16, job.iter_data.func.ptr));
      renderer->setArea(xmid, ymid, factor);
      try {
        renderer->prepareInitialPoints();
      } catch (const std::runtime_error& e) {
        std::cout<<std::string(e.what()) + "\n";
        return;
      }
      if (previous_channel != NO_CHANNEL) {
        leaveChannel(previous_channel, start_channel, thread_num, buf);
        buf.clear();
#ifdef RENDERING_DEBUG
        std::cout<<"Thread " + std::to_string(thread_num) + " changed channel " + std::to_string(previous_channel) + " -> " + std::to_string(start_channel) + "\n";
#endif
      }
    }
#ifdef RENDERING_DEBUG
    auto time_begin = std::chrono::high_resolution_clock::now();
#endif
    renderer->outputPointValues(buf.getData(), job.iter_data.renderer_iterations);
    buf.completed_iterations += job.iter_data.renderer_iterations;
#ifdef RENDERING_DEBUG
    double time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - time_begin).count();
    std::cout<<"Thread " + std::to_string(thread_num) + " completed job; channel: " + std::to_string(start_channel)
      + ", inner it: " + std::to_string(job.iter_data.inner_iterations) + ", it:" + std::to_string(job.iter_data.renderer_iterations)
      + " in " + std::to_string(time) + "s\n";
#endif
    previous_channel = start_channel;
    notifyJobCompletion(start_channel);
  }
}

IterJobData::IterJobData()
    : iter_data(0, 0, InnerFunctionData(nullptr, 0)) {}

IterJobData NebulabrotRenderingManager::getAJob(size_t preferred_channel) {
  std::lock_guard<std::mutex> lock(job_getter_mutex);
  IterJobData result;
  bool found = false;
  size_t channels_size = channels.size();
  for (size_t i = 0; i < channels_size; ++i) {
    size_t vec_size = channels[preferred_channel].iteration_jobs.size();
    if (vec_size > 0) {
      result.iter_data.renderer_iterations = channels[preferred_channel].iteration_jobs[vec_size-1];
      channels[preferred_channel].iteration_jobs.pop_back();
      found = true;
      break;
    }
    if (preferred_channel > 0) {
      preferred_channel--;
    } else {
      preferred_channel = channels_size - 1;
    }
  }
  if (!found) {
    return result;
  }
  result.iter_data.inner_iterations = channels[preferred_channel].data.inner_iterations;
  result.iter_data.func = channels[preferred_channel].data.func;
  result.num_channel = preferred_channel;
  result.buf = channels[preferred_channel].buf;
  return result;
}

void NebulabrotRenderingManager::notifyJobCompletion(size_t channel_id) {
  std::lock_guard<std::mutex> lock(notify_mutex);
  channels[channel_id].unfinished_jobs--;
  jobs_finished++;
  double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - render_start).count();
  int elapsed_int = static_cast<int>(elapsed);
  if (elapsed_int != last_notification_elapsed) {
    last_notification_elapsed = elapsed_int;
    if (jobs_finished < jobs_total) {double approx_job_proggress = jobs_finished + std::min(num_threads, jobs_total - jobs_finished) / 3.0;
      double estimated = elapsed * (jobs_total - approx_job_proggress) / approx_job_proggress;
      std::cout<<"("<<jobs_finished<<"/"<<jobs_total<<") Elapsed time: "<<elapsed<<", estimated remaining time: "<<estimated<<"\n";
    }
  }
}

void NebulabrotRenderingManager::leaveChannel(size_t previous_channel, size_t new_channel, size_t thread_num, const NebulabrotChannelBuffer& buf) {
  channels[previous_channel].buf->mergeWith(buf);
#ifdef RENDERING_DEBUG
  std::cout<<"Thread " + std::to_string(thread_num) + " merged channel " + std::to_string(previous_channel) + "\n";
#endif
  leave_mutex.lock();
  if (new_channel != NO_CHANNEL) {
    channels[new_channel].threads_on_channel++;
  }
  if (previous_channel != NO_CHANNEL) {
    channels[previous_channel].threads_on_channel--;
    if (channels[previous_channel].threads_on_channel == 0 && channels[previous_channel].unfinished_jobs == 0) {
      leave_mutex.unlock();
      channels[previous_channel].buf->updateMaxValue();
    } else {
      leave_mutex.unlock();
    }
  }
}

ImageColorBuffer::ImageColorBuffer(size_t width, size_t height)
    : width(width), height(height), data(new uint32_t[width*height]) {}

ImageColorBuffer::ImageColorBuffer(ImageColorBuffer&& other) noexcept {
  width = other.width;
  height = other.height;
  data = other.data;
  other.data = nullptr;
}

ImageColorBuffer& ImageColorBuffer::operator=(ImageColorBuffer&& other) noexcept {
  width = other.width;
  height = other.height;
  data = other.data;
  other.data = nullptr;
  return *this;
}

ImageColorBuffer::~ImageColorBuffer() {
  delete[] data;
}

bool ImageColorBuffer::saveFile(const std::string& filename) {
  std::string actual_filename = filename;
  while(file_exists(actual_filename)) {
    actual_filename += '_';
  }
  bool success =  (bool) stbi_write_png((actual_filename + ".png").c_str(), (int) width, (int) height, 4, data, (int) width*4);
  if (success) {
    std::cout<<"Saved image " + actual_filename + "\n";
  } else {
    std::cout<<"Failed to save image " + actual_filename + "\n";
  }
  return success;
}

ImageFunctionData::ImageFunctionData(ImagePixelFunc ptr, const std::vector<std::string>& channel_names,
                                     const std::vector<double>& desired_max, double cost)
    : mode(ImageMode::PIXEL_FUNC), channel_names(channel_names), desired_max(desired_max), cost(cost) {
  this->ptr.pixel = ptr;
}

ImageFunctionData::ImageFunctionData(WholeImageFunc ptr, const std::vector<std::string>& channel_names,
                                     const std::vector<double>& desired_max, double cost)
    : mode(ImageMode::IMAGE_FUNC), channel_names(channel_names), desired_max(desired_max), cost(cost) {
  this->ptr.whole = ptr;
}

ImageOutputData::ImageOutputData(const ImageFunctionData& func, NebulabrotChannelCollection* channels)
    : func(func), channels(channels) {}

double ImageOutputData::getCost() const {
  return channels->getWidth() * channels->getHeight() * func.cost;
}

ImageRenderChannel::ImageRenderChannel(const ImageOutputData& output_data, const std::string& filename)
    : cost(output_data.getCost()), filename(filename), output_data(output_data), failed(false) {}

bool ImageRenderChannel::operator<(const ImageRenderChannel& other) const {
  if (output_data.func.mode != other.output_data.func.mode) {
    return output_data.func.mode < other.output_data.func.mode;
  }
  return cost < other.cost;
}

ImageJobData::ImageJobData()
    : output_data(ImageFunctionData((ImagePixelFunc) 0, {}, {}), nullptr), start_index(0), end_index(0) {}

ImageRenderingManager::ImageRenderingManager(size_t num_threads)
    : num_threads(num_threads) {}

bool ImageRenderingManager::add(const std::string& filename, const ImageOutputData& image_data) {
  auto insert_it = images.begin();
  for (auto it = insert_it; it != images.end(); ++it) {
    int comp_result = filename.compare(it->filename);
    if (comp_result == 0) {
      std::cout<<"Error while adding image to rendering manager: name conflict (" + filename + ")\n";
      return false;
    } else if (comp_result < 0) {
      insert_it = it + 1;
    }
  }
  images.emplace(insert_it, image_data, filename);
  return true;
}

void ImageRenderingManager::execute() {
  std::lock_guard<std::mutex> lock(execute_mutex);
  std::vector<ImageColorBuffer> image_buffers;
  if (images.empty()) {
    return;
  }
  std::cout<<"Saving images:\n";
  render_start = std::chrono::high_resolution_clock::now();

  std::sort(images.begin(), images.end());
  double total_cost = 0.0;
  for (auto& im : images) {
    total_cost += im.cost;
    image_buffers.emplace_back(im.output_data.channels->getWidth(), im.output_data.channels->getHeight());
  }
  size_t approx_num_jobs = num_threads * 3 + static_cast<size_t>(std::log2(total_cost));

  jobs_total = 0;
  jobs_finished = 0;
  last_notification_elapsed = 0;
  size_t num = 0;
  for (auto& im : images) {
    size_t pixel_count = im.output_data.channels->getWidth() * im.output_data.channels->getHeight();
    im.buf = &image_buffers[num];
    num++;
    if (im.output_data.func.mode == ImageMode::IMAGE_FUNC) {
      im.render_jobs.emplace_back(0, pixel_count);
      im.unfinished_jobs = 1;
      jobs_total++;
    } else {
      size_t ch_jobs = std::max((size_t) 1, (size_t) (im.cost / total_cost * approx_num_jobs));
      size_t pixels_per_job_base = pixel_count / ch_jobs;
      size_t pixels_per_job_rem = pixel_count % ch_jobs;
      im.render_jobs.resize(ch_jobs);
      im.unfinished_jobs = ch_jobs;
      im.threads_on_channel = 0;
      size_t temp1 = 0;
      size_t temp2 = 0;
      for (size_t i = 0; i < ch_jobs; ++i) {
        if (i < pixels_per_job_rem) {
          temp2 += pixels_per_job_base + 1;
          im.render_jobs[i] = std::pair<size_t, size_t>(temp1, temp2);
          temp1 = temp2;
        } else {
          temp2 += pixels_per_job_base;
          im.render_jobs[i] = std::pair<size_t, size_t>(temp1, temp2);
          temp1 = temp2;
        }
      }
      jobs_total += ch_jobs;
    }
  }
#ifdef IMAGE_DEBUG
  std::cout<<std::endl<<"Number of segments: "<<jobs_total<<" ("<<approx_num_jobs<<")\n";
#endif
  std::vector<std::thread> threads;
  size_t temp_channel_num = images.size() - 1;
  for (size_t i = 0; i < num_threads; ++i) {
    images[temp_channel_num].threads_on_channel++;
    threads.emplace_back(&ImageRenderingManager::threadFunction, this, temp_channel_num, i);
    if (temp_channel_num == 0) {
      temp_channel_num = images.size() - 1;
    } else {
      temp_channel_num--;
    }
  }
  for (size_t i = 0; i < num_threads; ++i) {
    threads[i].join();
  }
  double time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - render_start).count();
  std::cout<<"Saving images ended in "<<time<<std::endl;
}

void ImageRenderingManager::threadFunction(size_t start_image, size_t thread_num) {
  size_t previous_image = NO_CHANNEL;
#ifdef IMAGE_DEBUG
  std::cout<<"Thread " + std::to_string(thread_num) + " started on channel " + std::to_string(start_image) + "\n";
#endif
  while(true) {
    ImageJobData job = getAJob(start_image);
    if (job.start_index == job.end_index) {
#ifdef IMAGE_DEBUG
      std::cout<<"Thread " + std::to_string(thread_num) + " terminated (no more jobs)\n";
#endif
      return;
    }
    start_image = job.num_image;
    if (previous_image != start_image) {
      if (previous_image != NO_CHANNEL) {
#ifdef IMAGE_DEBUG
        std::cout<<"Thread " + std::to_string(thread_num) + " changed channel " + std::to_string(previous_image) + " -> " + std::to_string(start_image) + "\n";
#endif
      }
    }
#ifdef IMAGE_DEBUG
    auto time_begin = std::chrono::high_resolution_clock::now();
#endif
    doJob(job, start_image);
#ifdef IMAGE_DEBUG
    double time = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::high_resolution_clock::now() - time_begin).count();
    std::cout<<"Thread " + std::to_string(thread_num) + " completed job; channel: " + std::to_string(start_image)
      + ", pixels: " + std::to_string(job.end_index - job.start_index)
      + " in " + std::to_string(time) + "s\n";
#endif
    previous_image = start_image;
    notifyJobCompletion(start_image);
  }
}

void ImageRenderingManager::doJob(const ImageJobData& job, size_t image_num) {
  size_t num_channels = job.output_data.func.channel_names.size();
  std::vector<uint32_t*> input_channels;
  std::vector<uint32_t> maximum_values;
  std::vector<double> completed_iterations;
  std::vector<double> desired_max = job.output_data.func.desired_max;
  input_channels.reserve(num_channels);
  maximum_values.reserve(num_channels);
  completed_iterations.reserve(num_channels);
  if (desired_max.empty()) {
    desired_max.resize(job.output_data.func.channel_names.size());
  } else if (desired_max.size() != job.output_data.func.channel_names.size()) {
    if (!images[image_num].failed) {
      failImage(image_num);
      std::cout << "Error while saving image " + images[image_num].filename
                   + ": desired_max vector has wrong size\n";
    }
    return;
  }
  for (auto& ch_name : job.output_data.func.channel_names) {
    auto it = job.output_data.channels->channels.find(ch_name);
    if (it == job.output_data.channels->channels.end()) {
      if (!images[image_num].failed) {
        failImage(image_num);
        std::cout<<"Error while saving image " + images[image_num].filename
                   + ": no channel named " + ch_name + "\n";
      }
      return;
    } else {
      input_channels.push_back(it->second.getData() + job.start_index);
      maximum_values.push_back(it->second.getMaxValue());
      completed_iterations.push_back(it->second.completed_iterations);
    }
  }
  if (job.output_data.func.mode == ImageMode::IMAGE_FUNC) {
    job.output_data.func.ptr.whole(job.end_index-job.start_index, input_channels.data(), maximum_values.data(),
                                   images[image_num].buf->getData() + job.start_index);
  } else {
    std::vector<double> current_values(num_channels);
    std::vector<double> multiplier(num_channels);
    uint32_t* output = images[image_num].buf->getData() + job.start_index;
    for (size_t j = 0; j < num_channels; ++j) {
      if (desired_max[j] <= 0.0) {
        multiplier[j] = 1;
      } else {
        multiplier[j] =  desired_max[j] * completed_iterations[j] / maximum_values[j];
      }
    }

    size_t len = job.end_index - job.start_index;
    for (size_t i = 0; i < len; ++i) {
      for (size_t j = 0; j < num_channels; ++j) {
        current_values[j] = multiplier[j] * input_channels[j][i] / maximum_values[j];
      }
      output[i] = job.output_data.func.ptr.pixel(current_values.data());
    }
  }
}

ImageJobData ImageRenderingManager::getAJob(size_t preferred_image) {
  std::lock_guard<std::mutex> lock(job_getter_mutex);
  ImageJobData result;
  bool found = false;
  size_t images_size = images.size();
  for (size_t i = 0; i < images_size; ++i) {
    size_t vec_size = images[preferred_image].render_jobs.size();
    if (vec_size > 0) {
      auto p = images[preferred_image].render_jobs[vec_size-1];
      result.start_index = p.first;
      result.end_index = p.second;
      images[preferred_image].render_jobs.pop_back();
      found = true;
      break;
    }
    if (preferred_image > 0) {
      preferred_image--;
    } else {
      preferred_image = images_size - 1;
    }
  }
  if (!found) {
    return result;
  }
  result.output_data = images[preferred_image].output_data;
  result.num_image = preferred_image;
  return result;
}

void ImageRenderingManager::notifyJobCompletion(size_t image_id) {
  notify_mutex.lock();
  images[image_id].unfinished_jobs--;
  jobs_finished++;
  double elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
      std::chrono::high_resolution_clock::now() - render_start).count();
  int elapsed_int = static_cast<int>(elapsed);
  if (elapsed_int != last_notification_elapsed) {
    last_notification_elapsed = elapsed_int;
    if (jobs_finished < jobs_total) {
      double approx_job_proggress = jobs_finished + std::min(num_threads, jobs_total - jobs_finished) / 3.0;
      double estimated = elapsed * (jobs_total - approx_job_proggress) / approx_job_proggress;
      std::cout << "(" << jobs_finished << "/" << jobs_total << ") Elapsed time: " << elapsed
                << ", estimated remaining time: " << estimated << "\n";
    }
  }
  if (images[image_id].unfinished_jobs == 0 && !images[image_id].failed) {
    notify_mutex.unlock();
    images[image_id].buf->saveFile(images[image_id].filename);
  } else {
    notify_mutex.unlock();
  }
}

void ImageRenderingManager::failImage(size_t image_id) {
  std::lock_guard<std::mutex> lock(job_getter_mutex);
  std::lock_guard<std::mutex> lock2(notify_mutex);
  images[image_id].failed = true;
  images[image_id].render_jobs.clear();
}
