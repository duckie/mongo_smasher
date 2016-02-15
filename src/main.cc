#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "mongo_smasher.h"
#include "logger.h"

using namespace std;

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  namespace ms = mongo_smasher;

  ms::Config config;

  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message")(
      "host,h", po::value<string>(&config.host)->default_value("127.0.0.1"),
      "")("port,p", po::value<size_t>(&config.port)->default_value(27017),
          "")("threads,t", po::value<size_t>(&config.threads)->default_value(1),
              "Number of threads")("model-file", po::value<vector<string>>(),
                                   "Model file to use.")(
      "verbosity,v", po::value<string>()->default_value("info"),
      "Verbosity of the output (debug,info,warning,error,fatal or quiet)");

  po::positional_options_description p;
  p.add("model-file", -1);

  po::variables_map vm;
  po::store(
      po::command_line_parser(argc, argv).options(desc).positional(p).run(),
      vm);
  po::notify(vm);

  if (vm.count("help")) {
    cout << desc << "\n";
    return 1;
  }
  if (!vm.count("model-file")) {
    cout << "Missing model file name\n" << desc << "\n";
    return 1;
  }

  config.model_file = vm["model-file"].as<vector<string>>().front();
  ms::mutable_global_log_level() =
      ms::enum_view<ms::log_level>::from_string(vm["verbosity"].as<string>());

  ms::run_stream(config);

  return 0;
}
