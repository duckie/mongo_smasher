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
#include <boost/filesystem/path.hpp>
#include <cppformat/format.h>
#include <regex>

using namespace std;
namespace bsx = bsoncxx;

namespace mongo_smasher {
namespace {
inline str_view to_str_view(bsoncxx::document::element elem) {
  return elem.get_utf8().value;
}


// Select a value in a vector
template <class Generator> class ValuePickPusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  std::vector<std::string> const &values_;

public:
  ValuePickPusher(Generator &gen, std::vector<std::string> const &values)
      : gen_(gen), distrib_(0, values.size() - 1), values_(values) {}

  void operator() (bsoncxx::builder::stream::single_context ctx) override {
    ctx << values_[distrib_(gen_)];
  }
};

struct StringPusherData {
  static str_view const alnums;
};
str_view const StringPusherData::alnums =
    "abcdefghijklmnopqrstuvwxyz0123456789";

template <class Generator>
class StringPusher : public ValuePusher, private StringPusherData {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  // The char_chooser is kept here because it cannot stay const
  std::uniform_int_distribution<size_t> char_chooser_{
      0u, StringPusherData::alnums.size() - 1u};

public:
  StringPusher(Generator &gen, size_t min_size, size_t max_size)
      : gen_(gen), distrib_(min_size, max_size) {}

  void operator() (bsoncxx::builder::stream::single_context ctx) override {
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
  IntPusher(Generator &gen, int min, int max) : gen_(gen), distrib_(min, max) {}

  void operator() (bsoncxx::builder::stream::single_context ctx) override {
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

template <>
typename enum_view_definition<frequency_type>::type
    enum_view_definition<frequency_type>::str_array = {
        "linear", "cyclic_gaussian", "sinusoidal"};

log_level &mutable_global_log_level() {
  static log_level global_level{log_level::info};
  return global_level;
}

log_level global_log_level() { return mutable_global_log_level(); }

ValuePusher::ValuePusher() :
  function_ { [this](bsx::builder::stream::single_context s) { return (*this)(s); }}
{}

std::function<void(bsx::builder::stream::single_context)>& ValuePusher::get_pusher() {return function_; }

Randomizer::Randomizer(bsoncxx::document::view model, str_view root_path)
    : gen_(rd_()) {
  namespace bsx = bsoncxx;
  using bsx::document::view;
  using bsx::document::element;
  using bsx::stdx::string_view;

  boost::filesystem::path system_root_path(root_path.data());
  auto values = model["values"];

  // if (values.type() != bsx::type::k_array) {
  // log(log_level::fatal, "The \"values\" must be an array of objects.");
  // throw exception();
  //}

  auto values_view = values.get_document().view();
  for (auto values_it = values_view.cbegin(); values_it != values_view.cend();
       ++values_it) {
    auto value = values_it->get_document().view();
    auto type = to_str_view(value["type"]);
    auto name = values_it->key();
    if (name.empty() || type.empty()) {
      log(log_level::warning,
          "Neither name or type of a generator can be empty.\n");
      continue;
    }

    if (type == str_view("string")) {
      int min = value["min_size"].get_int32();
      int max = value["max_size"].get_int32();
      generators_.emplace(
          name, make_unique<StringPusher<decltype(gen_)>>(
                    gen_, static_cast<size_t>(min), static_cast<size_t>(max)));
    } else if (type == str_view("int")) {
      int min = value["min"].get_int32();
      int max = value["max"].get_int32();
      generators_.emplace(
          name, make_unique<IntPusher<decltype(gen_)>>(gen_, min, max));
    } else if (type == str_view("filepick")) {
      // Cache the file if not already done
      auto filename = to_str_view(value["file"]);
      auto value_list_it = value_lists_.find(filename);
      if (end(value_lists_) == value_list_it) {
        auto file_path = system_root_path;
        file_path.append(filename.data());
        std::ifstream file_in(file_path.string().c_str());
        if (!file_in) {
          log(log_level::fatal, "Cannot open file \"%s\".\n",
              file_path.string().c_str());
          throw exception();
        }

        std::string line;
        auto &new_vector = value_lists_[filename];
        while (std::getline(file_in, line))
          new_vector.emplace_back(line.c_str());

        log(log_level::debug, "File \"%s\" cached with %lu lines.\n",
            file_path.string().c_str(), new_vector.size());
      }

      // Register the type
      auto const &possible_values = value_lists_[filename];
      generators_.emplace(name, make_unique<ValuePickPusher<decltype(gen_)>>(
                                    gen_, possible_values));
    } else {
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

std::function<void(bsx::builder::stream::single_context)>& Randomizer::get_value_pusher(str_view name) {
  return generators_[name]->get_pusher();
}

ProcessingUnit::ProcessingUnit(Randomizer& randomizer, std::string const& db_uri, bsoncxx::document::view collections) :
  db_conn_{mongocxx::uri{db_uri}}, randomizer_{randomizer}
{
  // First scan of the collections to determine the frequencies
  // TODO: use a SAT solver for dependencies
  for (auto collection_view : collections) {
    auto name = collection_view.key();
    float weight = 1.;
    //float weight = collection_view["weight"].get_double();
    collections_.emplace_back(Collection{name, collection_view["schema"], weight});
  }
}

void ProcessingUnit::process_element(bsoncxx::document::element const& element, bsx::builder::stream::array& ctx) {
}

void ProcessingUnit::process_element(bsoncxx::document::element const& element, bsx::builder::stream::document& ctx) {
  if (element.type() == bsx::type::k_utf8) {
    auto value = to_str_view(element);
    if ('$' == value[0]) {

      ctx << element.key() << randomizer_.get_value_pusher(value.substr(1));
        //[&value,this](bsx::builder::stream::single_context s) { randomizer_.get_value_pusher(value.substr(1))(s); };
    }
  }
  else if (element.type() == bsx::type::k_document) {
    bsx::builder::stream::document child;
    for(auto value : element.get_document().view()) {
      process_element(value, child);
    }
    ctx << element.key() << bsx::types::b_document{child.view()};
  }
}

void ProcessingUnit::process_tick() {
  for (auto& collection : collections_) {
    std::list<bsoncxx::document::element*> to_process;
    to_process.push_back(&collection.model);
    bsx::builder::stream::document document;
    for(auto value : collection.model.get_document().view()) {
      process_element(value, document);
    }
    auto db_collection = db_conn_["test"][collection.name];
    db_collection.insert_one(document.view());
  }
}

void run_stream(Config const &config) {
  // Load the file
  std::ifstream model_file_stream(config.model_file,
                                  std::ios::in | std::ios::binary);
  if (!model_file_stream) {
    log(log_level::fatal, "Cannot read file \"%s\".\n",
        config.model_file.c_str());
    return;
  }

  std::string root_path =
      boost::filesystem::path(config.model_file).parent_path().string();
  log(log_level::debug, "Path to search dictionaries is \"%s\"\n",
      root_path.c_str());

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
  auto model = bsoncxx::from_json(json_data);
  log(log_level::debug, "Json data parsed successfully\n");
  auto view = model.view();

  // Building randomizer to cache the file contents
  Randomizer randomizer(view, root_path.data());

  // Connect to data base
  // TODO: Manage failures properly.
  std::string db_uri = fmt::format("mongodb://{}:{}", config.host, config.port);
  mongo_smasher::log(mongo_smasher::log_level::info, "Connecting to %s.\n",
                     db_uri.c_str());

  // TODO add the logger here
  mongocxx::instance inst {};
  if (view["collections"].type() != bsx::type::k_document) {
    log(log_level::fatal, "what");
  }
  ProcessingUnit unit {randomizer, db_uri,  view["collections"].get_document().view()};

  for (;;) {
    unit.process_tick();
  }
  //static_assert(bsx::util::is_functor<std::function<void(bsoncxx::builder::stream::single_context)>&,void(bsx::builder::stream::single_context)>::value, "Nope");

};

} // namespace mongo_smasher
