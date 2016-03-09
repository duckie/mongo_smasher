#include <catch.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/element.hpp>
#include <bsoncxx/array/value.hpp>
#include <bsoncxx/array/element.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client.hpp>
#include "../utils.h"
#include "../logger.h"
#include "../randomizer.h"
#include "../document_pool.h"

using namespace std;
using namespace mongo_smasher;
using namespace bsoncxx;

namespace {
  static std::string const default_uri {"mongodb://127.0.0.1:27017"};
  static std::string const db_name {"test"};
  static std::string const col_name {"mongo_smasher_entities"};
}

TEST_CASE("Document Pool - Simple retrieval", "[document_pool]") {
  // connect to local db and clean previous state
  mongocxx::client db {mongocxx::uri{default_uri}};
  auto collection = db[db_name][col_name];
  collection.drop();

  // Insert a record
  builder::stream::document d1;
  d1 << "value" << 1;
  collection.insert_one(d1.view());

  // Create a document pool
  Randomizer randomizer;
  document_pool::DocumentPool pool {randomizer, default_uri, db_name, col_name, document_pool::update_method::latest, 1, 1};
  std::this_thread::sleep_for(chrono::milliseconds {100});

  auto doc = pool.draw_document();
  REQUIRE(doc);
  REQUIRE(LooseElement(doc->view())["value"].get<int>() == 1);


  //collection.drop();
}

