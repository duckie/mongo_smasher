#pragma once
#ifndef MONGO_SMASHER_HEADER
#define MONGO_SMASHER_HEADER
#include <string>
#include <map>
#include <vector>
#include <random>
#include <json_backbone/container.hpp>
#include <cstdio>
#include <limits>


namespace mongo_smasher {

enum class log_level { debug, info, warning, error, fatal };

template <typename... T>
void log(log_level level, char const *format, T &&... args) {
  std::printf(format, args...);
}

struct Config {
  std::string model_file;
  std::string host;
  // std::string config_file;
  size_t port;
  size_t threads;
};

class Randomizer {
  std::string const alnums = "abcdefghijklmnopqrstuvwxyz0123456789";
  size_t const alnums_size = 36u;
  std::random_device rd_;
  mutable std::mt19937 gen_;
  mutable std::uniform_int_distribution<unsigned int> char_chooser_ =
      std::uniform_int_distribution<unsigned int>(0u, alnums_size - 1u);

  std::map<std::string, std::vector<std::string>> value_lists_;

  void loadValue(json_backbone::container const &value);
  void loadPick(json_backbone::container const &value);

public:
  Randomizer(json_backbone::container const &model);
  ~Randomizer() = default;

  std::string const & getRandomPick(std::string const &filename) const;
  std::string getRandomString(size_t min, size_t max) const;
  template <class T> T const &getRandomPick(std::vector<T> const &values) const;
  template <class T> T getRandomInteger(T min = std::numeric_limits<T>::lowest(), T max = std::numeric_limits<T>::max()) {
    static_assert(std::numeric_limits<T>::is_integer, "The type used for Randomizer::getRandomInteger must be an integer type.");
    std::uniform_int_distribution<T> int_chooser(min, max);
    return int_chooser(gen_);
  }
};


} // namespace mongo_smasher

#endif // MONGO_SMASHER_HEADER
