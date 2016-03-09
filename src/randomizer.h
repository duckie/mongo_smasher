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

//
// Randomizer produces every random data
//
// Random complexity is all concentrated in that object
// responsible for consuming its pseudo-random generator.
// This generator can be consumed by ValuePushers objects, thus
// ValuePushers are built by the Randomizer.
// 
// Randomizer is responsible for parsing and caching $root_object.values
//
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
  // Basic ctor, equivalent to having an empty "values" array
  Randomizer();

  // 
  // Builds a randomizer from a config ffile
  //
  // @param view is a view of the "values" array extracted from the config file
  // @param root_path is the directory containing said file
  //
  Randomizer(bsoncxx::document::view, bsoncxx::stdx::string_view root_path);

  // move ctor
  Randomizer(Randomizer&&) = default;
  ~Randomizer() = default;

  // Returns a randomized double from [0,1] used by the caller
  // to implement a Bernouilli baw based on a weight  does not
  // know off.
  double existence_draw();

  //
  // Returns a random index between in [0,size[
  //
  // @param size is the maximum value the index could take plus 1. Supposed to 
  //             be the size of a random access container
  //
  size_t index_draw(size_t size);

  // Interprets a string as an array range (ex: "0:10") and creates the matching
  // proxy to generate a size in that range
  std::unique_ptr<RangeSizeGenerator> make_range_size_generator(
      bsoncxx::stdx::string_view range_expression);

  std::unique_ptr<ValuePusher> make_value_pusher(bsoncxx::stdx::string_view name);
};
};
