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
 public:
  using random_engine_t = std::minstd_rand;

 private:
  bsoncxx::document::view values_;
  std::random_device rd_;
  mutable random_engine_t gen_;
  boost::filesystem::path system_root_path_;
  std::uniform_real_distribution<double> key_existence_ {0.,1.};

  std::map<std::string, std::unique_ptr<RangeSizeGenerator>> range_size_generators_;
  std::map<bsoncxx::stdx::string_view, std::vector<std::string>> value_lists_;
  std::map<bsoncxx::stdx::string_view,
           std::map<bsoncxx::stdx::string_view, std::unique_ptr<ValuePusher>>> generators_;

 public:
  Randomizer(bsoncxx::document::view, bsoncxx::stdx::string_view root_path);
  Randomizer(Randomizer&&) = default;
  ~Randomizer() = default;
  random_engine_t& random_generator();
  RangeSizeGenerator& get_range_size_generator(bsoncxx::stdx::string_view range_expression);
  double existence_draw();
  std::function<void(bsoncxx::builder::stream::single_context)> const& get_value_pusher(
      bsoncxx::stdx::string_view col_name, bsoncxx::stdx::string_view name);
};
};
