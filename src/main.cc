#include <boost/program_options.hpp>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "mongo_smasher.h"

int main(int argc, char * argv[]) {
  namespace po = boost::program_options;
  namespace ms = mongo_smasher;

  std::string data_model;
  ms::Config config;

  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("host,h", po::value<std::string>(&config.host)->default_value("127.0.0.1"), "")
    ("port,p", po::value<size_t>(&config.port)->default_value(27017), "")
    ("fnames,m", po::value<std::string>(&config.fnames_file)->default_value(std::getenv("MONGO_SMASHER_FNAMES_DICT")), "File containing the first names list")
    ("names,m", po::value<std::string>(&config.names_file)->default_value(std::getenv("MONGO_SMASHER_NAMES_DICT")), "File containing the names list")
    ("places,m", po::value<std::string>(&config.places_file)->default_value(std::getenv("MONGO_SMASHER_PLACES_DICT")), "File containing the places list")
    ("threads,t", "Number of threads")
    ;
  
  po::positional_options_description p;
  p.add("input-file", -1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
  po::notify(vm);    

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }


  return 0;
}

