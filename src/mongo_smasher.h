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

class CollectionHub {
  mongocxx::client db_conn_;
  std::string db_name_;
  std::map<std::string, mongocxx::collection> collections_;

 public:
  CollectionHub(std::string db_uri, std::string db_name);
  mongocxx::collection& operator[](std::string const& collection_name);
};

struct ThreadPilot {
  std::atomic<bool> run = true;;
};

void run_stream(Config const& config);

}  // namespace mongo_smasher
