#pragma once
#include <bsoncxx/builder/stream/document.hpp>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <random>
#include "value_pusher.h"

namespace mongo_smasher {

class Randomizer {
  std::string const alnums = "abcdefghijklmnopqrstuvwxyz0123456789";
  size_t const alnums_size = 36u;
  std::random_device rd_;
  mutable std::mt19937 gen_;
  mutable std::uniform_int_distribution<unsigned int> char_chooser_ =
      std::uniform_int_distribution<unsigned int>(0u, alnums_size - 1u);

  std::map<bsoncxx::stdx::string_view, std::vector<std::string>> value_lists_;
  std::map<bsoncxx::stdx::string_view, std::unique_ptr<ValuePusher>> generators_;

public:
  Randomizer(bsoncxx::document::view, bsoncxx::stdx::string_view root_path);
  ~Randomizer() = default;

  std::function<void(bsoncxx::builder::stream::single_context)>& get_value_pusher(bsoncxx::stdx::string_view name);
  std::string getRandomString(size_t min, size_t max) const;
  template <class T> T const &getRandomPick(std::vector<T> const &values) const;
  template <class T>
  T getRandomInteger(T min = std::numeric_limits<T>::lowest(),
                     T max = std::numeric_limits<T>::max()) {
    static_assert(std::numeric_limits<T>::is_integer,
                  "The type used for Randomizer::getRandomInteger must be an "
                  "integer type.");
    std::uniform_int_distribution<T> int_chooser(min, max);
    return int_chooser(gen_);
  }
};

};
