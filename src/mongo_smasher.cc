#include "mongo_smasher.h"
#include <list>
#include <functional>
#include <fstream>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <sstream>

using namespace std;

namespace mongo_smasher {

template <>
typename enum_view_definition<log_level>::type
    enum_view_definition<log_level>::str_array = {"debug", "info",  "warning",
                                                  "error", "fatal", "quiet"};

using jv = json_backbone::container;

log_level &mutable_global_log_level() {
  static log_level global_level{log_level::info};
  return global_level;
}

log_level global_log_level() { return mutable_global_log_level(); }

Randomizer::Randomizer(json_backbone::container const &model) : gen_(rd_()) {
  if (model.is_array()) {
    list<reference_wrapper<json_backbone::container const>> remaining_values;
    for (auto const &collec_config : model.ref_array()) {
      remaining_values.emplace_back(collec_config["schema"]);
    }
    while (!remaining_values.empty()) {
      jv const &current = remaining_values.front();
      remaining_values.pop_front();
      if (!current.is_object() && !current.is_array()) {
        log(log_level::error,
            "Randomizer caching encountered a unexpected scalar value.\n");
        continue;
      }

      if (current.is_object()) {
        auto ms_type = current["_ms_type"];
        if (ms_type.is_string()) {
          loadValue(current);
        } else {
          for (auto const &child_pair : current.ref_object()) {
            remaining_values.emplace_back(child_pair.second);
          }
        }
      } else {
        for (auto const &child : current.ref_array()) {
          remaining_values.emplace_back(child);
        }
      }
    }
  }
  log(log_level::info, "Randomizer caching finished.\n");
}

void Randomizer::loadValue(json_backbone::container const &value) {
  string type = value["_ms_type"];
  log(log_level::debug, "Randomizer caching a \"%s\" element.\n", type.c_str());
  if (type == "random_pick" || type == "random_multipick") {
    loadPick(value);
  }
}

void Randomizer::loadPick(json_backbone::container const &value) {
  jv data = value; // Copy to make access easier on non existing keys
  jv source = value["source"];
  if (source.is_string()) {
    log(log_level::info, "Loading random picking file %s.\n",
        source.ref_string().c_str());
    ifstream file_stream(source.ref_string());
    if (file_stream) {
      string line;
      vector<string> &collec = value_lists_[source];
      if (collec.size()) {
        log(log_level::debug, "Picking file already loaded.\n");
        return;
      }
      while (getline(file_stream, line)) {
        if (!line.empty() && *line.rbegin() == '\r') {
          line.erase(line.length() - 1, 1);
        }
        collec.push_back(line);
      }

      log(log_level::info, "%lu elements loaded.\n", collec.size());
    }
  }
}

string const &Randomizer::getRandomPick(string const &filename) const {
  auto value_it = value_lists_.find(filename);
  if (end(value_lists_) == value_it)
    throw runtime_error("Pickable collection does not exist.");

  vector<string> const &values(value_it->second);
  uniform_int_distribution<unsigned int> index_chooser(0u, values.size() - 1);
  size_t chosen_index = index_chooser(gen_);
  log(log_level::debug, "Index chosen : %lu %lu.\n", chosen_index,
      values.size());
  return values.at(index_chooser(gen_));
}

string Randomizer::getRandomString(size_t min, size_t max) const {
  ostringstream output;
  for (size_t index = min; index < max; ++index)
    output << alnums.at(char_chooser_(gen_));
  return output.str();
}

void run_stream(Config const &config) {

  // Stage 1 - Load the file
  std::ifstream model_file_stream(config.model_file,
                                  std::ios::in | std::ios::binary);
  if (!model_file_stream) {
    log(log_level::fatal, "Cannot read file \"%s\".\n",
        config.model_file.c_str());
    return;
  }

  std::string json_data;
  model_file_stream.seekg(0, std::ios::end);
  json_data.resize(model_file_stream.tellg());
  model_file_stream.seekg(0, std::ios::beg);
  model_file_stream.read(&json_data.front(), json_data.size());

  log(log_level::debug, "Json file contains:\n---\n%s\n---\n",
      json_data.c_str());
};

} // namespace mongo_smasher
