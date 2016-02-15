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
  std::function<void(bsoncxx::builder::stream::single_context)> const & get_value_pusher(bsoncxx::stdx::string_view name);
};

};
