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
#include "logger.h"
#include "randomizer.h"
#include "process_unit.h"
#include <chrono>

using namespace std;
namespace bsx = bsoncxx;
using str_view = bsoncxx::stdx::string_view;
using bsoncxx::stdx::make_unique;

namespace mongo_smasher {

template <>
typename enum_view_definition<frequency_type>::type
    enum_view_definition<frequency_type>::str_array = {
        "linear", "cyclic_gaussian", "sinusoidal"};


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
  mongocxx::instance inst{};
  if (view["collections"].type() != bsx::type::k_document) {
    log(log_level::fatal, "what");
  }
  std::vector<ProcessingUnit> units;
  //ProcessingUnit unit{randomizer, db_uri,
                      //view["collections"].get_document().view()};
  for(auto collection_view : view["collections"].get_document().view()) {
    log(log_level::debug,"Registering %s\n", collection_view.key().data());
    units.emplace_back(randomizer, db_uri, collection_view.key(), collection_view.get_document().view());
  }

  // Start a clock
  auto start = std::chrono::high_resolution_clock::now();
  for (;;) {
    for(auto& unit : units) {
      unit.process_tick();
    }
    auto stop = std::chrono::high_resolution_clock::now();
    if (std::chrono::milliseconds(1000) < (stop - start)) {
      // Update status line
      std::ostringstream status_line;
      for(auto& unit : units) {
        status_line << unit.name() << '[' << unit.nb_inserted() << "] ";
      }
      log(log_level::info,"%s\n", status_line.str().c_str());
      start = stop;
    }

  }
  // static_assert(bsx::util::is_functor<std::function<void(bsoncxx::builder::stream::single_context)>&,void(bsx::builder::stream::single_context)>::value,
  // "Nope");
};

} // namespace mongo_smasher
