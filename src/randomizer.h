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

// Struct to hold lists of pickable data
struct ValueList {
  // Holds ownership of an array, needed because said array
  // could be generated from a file
  bsoncxx::array::value array_;

  // Elements, strings if extracted from a file but could be
  // anything else if extracted from the json file.
  //
  // The array value should not be used because access time is O(n)
  // instead of O(1) for the vector
  std::vector<bsoncxx::array::element> values_;
};

class Randomizer {
 public:
  using random_engine_t = std::minstd_rand;

 private:
  bsoncxx::document::view values_;
  std::random_device rd_;
  mutable random_engine_t gen_;
  boost::filesystem::path system_root_path_;
  std::uniform_real_distribution<double> key_existence_{0., 1.};
  std::map<bsoncxx::stdx::string_view, ValueList> value_lists_;

 public:
  Randomizer(bsoncxx::document::view, bsoncxx::stdx::string_view root_path);
  Randomizer(Randomizer&&) = default;
  ~Randomizer() = default;
  random_engine_t& random_generator();
  double existence_draw();
  std::unique_ptr<RangeSizeGenerator> make_range_size_generator(
      bsoncxx::stdx::string_view range_expression);
  std::unique_ptr<ValuePusher> make_value_pusher(bsoncxx::stdx::string_view name);
};
};
