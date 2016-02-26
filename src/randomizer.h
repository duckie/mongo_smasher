#pragma once
#include <bsoncxx/builder/stream/document.hpp>
#include <boost/filesystem/path.hpp>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <random>
#include "value_pusher.h"

namespace mongo_smasher {

struct RangeSizeGenerator {
  virtual ~RangeSizeGenerator() = default;
  virtual size_t generate_size() = 0;
};

class Randomizer {
  bsoncxx::document::view values_;
  std::random_device rd_;
  mutable std::mt19937 gen_;
  boost::filesystem::path system_root_path_;

  std::map<std::string, std::unique_ptr<RangeSizeGenerator>> range_size_generators_;
  std::map<bsoncxx::stdx::string_view, std::vector<std::string>> value_lists_;
  std::map<bsoncxx::stdx::string_view,
           std::map<bsoncxx::stdx::string_view, std::unique_ptr<ValuePusher>>> generators_;

 public:
  Randomizer(bsoncxx::document::view, bsoncxx::stdx::string_view root_path);
  Randomizer(Randomizer&&) = default;
  ~Randomizer() = default;
  std::mt19937& random_generator();
  RangeSizeGenerator& get_range_size_generator(bsoncxx::stdx::string_view range_expression);
  std::function<void(bsoncxx::builder::stream::single_context)> const& get_value_pusher(
      bsoncxx::stdx::string_view col_name, bsoncxx::stdx::string_view name);
};
};
