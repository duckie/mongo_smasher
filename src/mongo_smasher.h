#pragma once
#include <string>
#include <map>
#include <vector>
#include <random>
#include <cstdio>
#include <limits>
#include <algorithm>
#include <exception>
#include <bsoncxx/stdx/make_unique.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "utils.h"
#include <atomic>
#include <thread>

namespace mongo_smasher {

struct Config {
  std::string model_file;
  std::string host;
  size_t port;
  size_t nb_producers;
  size_t nb_consumers;
};

enum class frequency_type : size_t { linear, cyclic_gaussian, sinusoidal, FREQUENCY_TYPE_MAX };

template <>
struct enum_view_size<frequency_type> {
  static constexpr size_t const value = static_cast<size_t>(frequency_type::FREQUENCY_TYPE_MAX);
};

struct ThreadPilot {
  std::atomic<bool> run{true};
};

template <class T>
struct ThreadRunner {
  T hosted;
  std::thread thread;
  template <class... Args>
  ThreadRunner(Args&&... args)
      : hosted{std::forward<Args>(args)...}, thread{[this]() -> void { this->hosted.run(); }} {
  }
};

void run_stream(Config const& config);

}  // namespace mongo_smasher
