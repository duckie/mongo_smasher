#include "mongo_smasher.h"
#include <list>
#include <functional>
#include <fstream>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <bsoncxx/json.hpp>
#include <bsoncxx/view_or_value.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/types/value.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

using namespace std;

namespace mongo_smasher {
namespace {
inline str_view to_str_view(bsoncxx::document::element elem) {
  return elem.get_utf8().value;
}

// Can be used with either std::string or str_view
template <class String> String dirname(String const& filename) {
  auto last_slash = filename.find_last_of('/');
  auto last_backslash = filename.find_last_of('\\');
  if (last_slash == std::string::npos && last_backslash == std::string::npos)
    return {};
  auto pos = std::max<int>(last_slash, last_backslash);  // std::max does not resolve to signed properly
  return filename.substr(0,pos+1);
}


// Select a value in a vector
template <class Generator> class ValuePickPusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  std::vector<str_view> const &values_;

public:
  ValuePickPusher(Generator &gen, std::vector<str_view> const &values)
      : gen_(gen), distrib_(0, values.size() - 1), values_(values) {}

  void push(bsoncxx::builder::stream::single_context ctx) override {
    ctx << values_[distrib_(gen_)];
  }
};

struct StringPusherData {
  static str_view const alnums;
};
str_view const StringPusherData::alnums = "abcdefghijklmnopqrstuvwxyz0123456789";

template <class Generator> class StringPusher : public ValuePusher, private StringPusherData {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  // The char_chooser is kept here because it cannot stay const
  std::uniform_int_distribution<size_t> char_chooser_ {0u, StringPusherData::alnums.size() - 1u };

public:
  StringPusher(Generator &gen, size_t min_size, size_t max_size)
      : gen_(gen), distrib_(min_size,max_size) {}

  void push(bsoncxx::builder::stream::single_context ctx) override {
    ostringstream value_stream;
    size_t size = distrib_(gen_);
    for (size_t index = 0u; index < size; ++index)
      value_stream << alnums[char_chooser_(gen_)];
    ctx << value_stream.str();
  }

};

template <class Generator> class IntPusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<int> distrib_;

public:
  IntPusher(Generator &gen, int min, int max)
      : gen_(gen), distrib_(min,max) {}

  void push(bsoncxx::builder::stream::single_context ctx) override {
    ctx << distrib_(gen_);
  }
};
}
}

namespace mongo_smasher {

template <>
typename enum_view_definition<log_level>::type
    enum_view_definition<log_level>::str_array = {"debug", "info",  "warning",
                                                  "error", "fatal", "quiet"};

log_level &mutable_global_log_level() {
  static log_level global_level{log_level::info};
  return global_level;
}

log_level global_log_level() { return mutable_global_log_level(); }

Randomizer::Randomizer(bsoncxx::document::view model, str_view root_path) : gen_(rd_()) {
  namespace bsx = bsoncxx;
  using bsx::document::view;
  using bsx::document::element;
  using bsx::stdx::string_view;

  auto values = model["values"];

  if (values.type() != bsx::type::k_array) {
    log(log_level::fatal, "The \"values\" must be an array of objects.");
    throw exception();
  }

  for (auto value : values.get_array().value) {
    auto type = to_str_view(value["type"]);
    auto name = to_str_view(value["name"]);
    if (name.empty() || type.empty()) {
      log(log_level::warning,
          "Neither name or type of a generator can be empty.\n");
      continue;
    }

    if (type == str_view("string")) {
      int min = value["min_size"].get_int32();
      int max = value["max_size"].get_int32();
      generators_.emplace(name, make_unique<StringPusher<decltype(gen_)>>(gen_, static_cast<size_t>(min), static_cast<size_t>(max)));
    }
    else if (type == str_view("string")) {
      int min = value["min"].get_int32();
      int max = value["max"].get_int32();
      generators_.emplace(name, make_unique<IntPusher<decltype(gen_)>>(gen_, min, max));
    }
    else if (type == str_view("filepick")) {
      // Cache the file if not already done
      auto filename = to_str_view(value["file"]);
      auto value_list_it = value_lists_.find(filename);
      if (end(value_lists_) == value_list_it) {
        

        std::ifstream file_in(filename.data());
        if (!file_in) {
          log(log_level::fatal, "Cannot open file \"%s\".\n", filename.data());
          throw exception();
        }

        std::string line;
        auto &new_vector = value_lists_[filename];
        while (std::getline(file_in, line))
          new_vector.emplace_back(line.c_str());
      }

      // Register the type
      auto const &possible_values = value_lists_[filename];
      generators_.emplace(name, make_unique<ValuePickPusher<decltype(gen_)>>(gen_, possible_values));
    }
    else {
      log(log_level::error, "Unknown value type \"%s\".\n", type.data());
    }
  }

  log(log_level::info, "Randomizer caching finished.\n");
  return;
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

  std::string root_path = dirname(config.model_file);
  log(log_level::info, "Path to search dictionaries is \"%s\"\n", root_path.c_str());

  std::string json_data;
  model_file_stream.seekg(0, std::ios::end);
  json_data.resize(model_file_stream.tellg());
  model_file_stream.seekg(0, std::ios::beg);
  model_file_stream.read(&json_data.front(), json_data.size());

  log(log_level::debug, "Json file contains:\n---\n%s\n---\n",
      json_data.c_str());

  // This parsed model must be kept alive til the end of run_stream
  // because objects use string_views pointing inside it 
  // TODO: enforce it one way or another
  auto const model = bsoncxx::from_json(json_data);
  log(log_level::debug, "Json data parsed successfully\n");
  auto const &view = model.view();

  // Building randomizer to cache the file contents
  Randomizer randomizer(view, root_path.data());

  // Connect to data base
  ostringstream uri_ss;
  uri_ss << "mongodb://" << config.host << ":" << config.port;
  mongo_smasher::log(mongo_smasher::log_level::info, "Connecting to %s.\n", uri_ss.str().c_str());
  mongocxx::instance inst{};
  mongocxx::client conn{mongocxx::uri{uri_ss.str()}};


  // for (auto collection_it = view.cbegin(); collection_it != view.cend();
  // ++collection_it) {
  // auto const & collection_view = *collection_it;
  // std::string name = collection_view["name"].get_utf8().value.to_string();
  //}
  // for (auto elem : model) {
  //}
};

} // namespace mongo_smasher
