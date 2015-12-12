#include "mongo_smasher.h"

namespace mongo_smasher {
//MongoSmasher::MongoSmasher(json_backbone::container const& data_model) : data_model_(data_model) {}

Randomizer::Randomizer(std::vector<std::string>&& fnames, std::vector<std::string>&& names, std::vector<std::string>&& places)
  : fnames_(std::move(fnames)), names_(std::move(names)),places_(std::move(places))
          {}

}  // namespace mongo_smasher
