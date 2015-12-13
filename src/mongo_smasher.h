#pragma once
#ifndef MONGO_SMASHER_HEADER
#define MONGO_SMASHER_HEADER
#include <string>
#include <map>
#include <vector>
#include <random>
#include <json_backbone/container.hpp>
#include <cstdio>

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
  mutable std::discrete_distribution<> bool_chooser_ = {1, 1};
  mutable std::uniform_int_distribution<unsigned int> char_chooser_ =
      std::uniform_int_distribution<unsigned int>(0u, alnums_size - 1u);
  mutable std::uniform_int_distribution<int> int_chooser_ =
      std::uniform_int_distribution<int>(
          std::numeric_limits<int>::lowest(),
          std::numeric_limits<int>::max());
  mutable std::uniform_int_distribution<size_t> uint_chooser_ =
      std::uniform_int_distribution<size_t>(
          std::numeric_limits<size_t>::lowest(),
          std::numeric_limits<size_t>::max());
  mutable std::uniform_real_distribution<float> float_chooser_ =
      std::uniform_real_distribution<float>(
          std::numeric_limits<float>::lowest() / 2,
          std::numeric_limits<float>::max() / 2);

  std::map<std::string, std::vector<std::string>> value_lists_;

  void loadValue(json_backbone::container const &value);
  void loadPick(json_backbone::container const &value);

public:
  Randomizer(json_backbone::container const &model);
  ~Randomizer() = default;

  std::string const & getRandomPick(std::string const &filename) const;
  std::string getRandomString(size_t min, size_t max) const;
  int getRandomIndex(int min, int max) const;
  int getRandomInt() const;
  size_t getRandomUnsigned() const;
  template <class T> T const &getRandomPick(std::vector<T> const &values) const;
};

// class MongoSmasher {
// size_t nb_records_sent_ = 0;
// Randomizer& generator_;
// json_backbone::container const& data_model_;
//
// public:
// MongoSmasher(Randomizer const& generator, json_backbone::container const&
// data_model);
// MongoSmasher(MongoSmasher&&) = default;
//~MongoSmasher() = default;
//
// void run();
//};

} // namespace mongo_smasher

#endif // MONGO_SMASHER_HEADER
