#pragma once
#ifndef MONGO_SMASHER_HEADER
#define MONGO_SMASHER_HEADER
#include <random>
#include <json_backbone/container.hpp>

namespace mongo_smasher {

struct Config {
  std::string model_file;
  std::string host;
  std::string fnames_file;
  std::string names_file;
  std::string places_file;
  size_t port;
  size_t threads;
};

class Randomizer {
  std::vector<std::string> fnames_;
  std::vector<std::string> names_;
  std::vector<std::string> places_;

  Randomizer(std::vector<std::string>&& fnames, std::vector<std::string>&& names, std::vector<std::string>&& places);
  ~Randomizer() = default;
};

//class MongoSmasher {
  //size_t nb_records_sent_ = 0;
  //Randomizer& generator_;
  //json_backbone::container const& data_model_;
//
 //public:
  //MongoSmasher(Randomizer const& generator, json_backbone::container const& data_model);
  //MongoSmasher(MongoSmasher&&) = default;
  //~MongoSmasher() = default;
//
  //void run();
//};



} // namespace mongo_smasher


#endif  // MONGO_SMASHER_HEADER
