#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <rapidjson/reader.h> 
#include <rapidjson/filereadstream.h> 
#include <json_backbone/extensions/rapidjson/rapidjson.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include "mongo_smasher.h"

using namespace std;

int main(int argc, char * argv[]) {
  namespace po = boost::program_options;
  namespace ms = mongo_smasher;

  ms::Config config;

  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("host,h", po::value<string>(&config.host)->default_value("127.0.0.1"), "")
    ("port,p", po::value<size_t>(&config.port)->default_value(27017), "")
    ("threads,t", po::value<size_t>(&config.threads)->default_value(1), "Number of threads")
    ("model-file", po::value<vector<string>>(), "Model file to use.")
    ("verbosity,v", po::value<string>()->default_value("info"), "Verbosity of the output (debug,info,warning,error,fatal or quiet)")
    ;
  
  po::positional_options_description p;
  p.add("model-file", -1);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
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
  ms::mutable_global_log_level() = ms::enum_view<ms::log_level>::from_string(vm["verbosity"].as<string>());
  

  ms::run_stream(config); 

  return 0;
  json_backbone::container data_model;
  // Parsing model file
  {
    //ifstream input_stream();
    FILE* fp = fopen(config.model_file.c_str(),"r");
    if (!fp) {
      mongo_smasher::log(mongo_smasher::log_level::error, "Cannot open file \"%s\" as a model.\n", config.model_file.c_str());
      return 1;
    }

    auto handler = json_backbone::extensions::rapidjson::make_reader_handler(data_model);
    char readBuffer[256];
    rapidjson::FileReadStream bis(fp, readBuffer, sizeof(readBuffer));
    rapidjson::Reader reader;
    //rapidjson::StringStream ss(input_stream.c_str());
    if(!reader.Parse(bis, handler)) {
      cout << "Failed \n";
    }
  }

  // Caching list stored into files  
  mongo_smasher::Randomizer randomize(data_model);
  // Filling up the DBs

  ostringstream uri_ss;
  uri_ss << "mongodb://" << config.host << ":" << config.port;
  mongo_smasher::log(mongo_smasher::log_level::info, "Connecting to %s.\n", uri_ss.str().c_str());
  mongocxx::instance inst{};
  mongocxx::client conn{mongocxx::uri{uri_ss.str()}};


  size_t id_gen = 0u;
  map<string, vector<size_t>> existing_ids;

  for(json_backbone::container collection_desc : data_model.ref_array()) { 
    string db_name = collection_desc["db"];
    string name = collection_desc["collection"];
    auto collection = conn[db_name][name];
    size_t nb_records = collection_desc["nb_records"].as_uint();
    for (size_t index = 0; index < nb_records; ++index) {
      bsoncxx::builder::stream::document document{};
      for (auto field_pair : collection_desc["schema"].ref_object()) {
        if (field_pair.first == "_id") {
          document << "_id" << static_cast<int>(++id_gen);
          existing_ids["name"].push_back(id_gen);
        }
        else {
          json_backbone::container field_desc = field_pair.second;
          string type = field_desc["_ms_type"];
          mongo_smasher::log(mongo_smasher::log_level::debug, "Generate field %s of type %s.\n", field_pair.first.c_str(), type.c_str());
          if (type == "random_int") {
            document << field_pair.first << randomize.getRandomInteger<int>(field_desc["min"].as_int(), field_desc["max"].as_int());
          }
          else if (type == "random_pick") {
            auto source = field_desc["source"];
            if (source.is_string()) {
              document << field_pair.first << randomize.getRandomPick(source);
            }
          }
          else if (type == "random_multipick") {
            auto source = field_desc["source"];
            vector<string> results;
            if (source.is_string()) {
              int nb_elem = randomize.getRandomInteger<size_t>(field_desc["min"].as_uint(), field_desc["max"].as_uint());
              for (int index = 0; index < nb_elem; ++index)
                results.push_back(randomize.getRandomPick(source));
            }
            string multi_type = field_desc["type"];
            if (multi_type == "string") {
              ostringstream out;
              bool first = true;
              for (auto& value : results) {
                out << (first?"":" ") << value;
                first = false;
              }
              document << field_pair.first << out.str();
            }
            else if (multi_type == "array") {
               auto sub_array = (document << field_pair.first << bsoncxx::builder::stream::open_array_type());
               for (auto& value : results) {
                 sub_array << value;
               }
               sub_array << bsoncxx::builder::stream::close_array_type();
            }
          }
        }
      }
      collection.insert_one(document.view());
    }
  }

  return 0;
}

