#include "../document_pool.h"
#include "../logger.h"
#include "../randomizer.h"
#include "../utils.h"
#include <bsoncxx/array/element.hpp>
#include <bsoncxx/array/value.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/document/element.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/json.hpp>
#include <catch.hpp>
#include <future>
#include <mongocxx/client.hpp>
#include <thread>

using namespace std;
using namespace mongo_smasher;
using namespace bsoncxx;

namespace {
  static std::string const default_uri {"mongodb://127.0.0.1:27017"};
  static std::string const db_name {"test"};
  static std::string const col_name {"mongo_smasher_entities"};
}

TEST_CASE("Document Pool - Simple retrieval", "[document_pool]") {
  // connect to local db
  mongocxx::client db {mongocxx::uri{default_uri}};
  auto collection = db[db_name][col_name];
  collection.drop();

  SECTION("One document") {
    // Insert a record
    builder::stream::document d1;
    d1 << "value" << 1;
    collection.insert_one(d1.view());

    // Create a document pool
    Randomizer randomizer;
    document_pool::DocumentPool pool {randomizer, default_uri, document_pool::update_method::latest, 1, 1};
    pool.draw_document(db_name,col_name);
    std::this_thread::sleep_for(chrono::milliseconds {100});
    
    // Insert a new one
    builder::stream::document d2;
    d2 << "value" << 2;
    collection.insert_one(d2.view());

    // Check cached document
    auto doc = pool.draw_document(db_name,col_name);
    REQUIRE(doc);
    REQUIRE(LooseElement(doc->view())["value"].get<int>() == 1);

    // Wait for due update
    std::this_thread::sleep_for(chrono::milliseconds {100});

    // Check new one
    doc = pool.draw_document(db_name,col_name);
    REQUIRE(doc);
    REQUIRE(LooseElement(doc->view())["value"].get<int>() == 2);
  }

  SECTION("Several documents, multiple uses") {
    size_t const nb_instances = 5;

    // Insert records
    for(int i=0; i < nb_instances; ++i) {
      builder::stream::document d;
      d << "index" << i << "value" << 1;
      collection.insert_one(d.view());
    }

    // Create a document pool
    Randomizer randomizer;
    document_pool::DocumentPool pool {randomizer, default_uri, document_pool::update_method::latest, nb_instances, nb_instances};
    // Launch a retrieval by trying to get a document
    pool.draw_document(db_name,col_name);
    std::this_thread::sleep_for(chrono::milliseconds {100});
    
    // Insert new instances
    for(int i=0; i < nb_instances; ++i) {
      builder::stream::document d;
      d << "index" << i << "value" << 2;
      collection.insert_one(d.view());
    }

    // Check cached document
    for(int i=0; i < nb_instances*nb_instances; ++i) {
      auto doc = pool.draw_document(db_name,col_name);
      REQUIRE(doc);
      REQUIRE(LooseElement(doc->view())["value"].get<int>() == 1);
    }

    // Wait for due update
    std::this_thread::sleep_for(chrono::milliseconds {100});

    // Check new one
    for(int i=0; i < nb_instances*nb_instances; ++i) {
      auto doc = pool.draw_document(db_name,col_name);
      REQUIRE(doc);
      REQUIRE(LooseElement(doc->view())["value"].get<int>() == 2);
    }
  }

  SECTION("Concurrent consumption") {
    size_t const nb_instances = 5;

    // Insert records
    for(int i=0; i < nb_instances; ++i) {
      builder::stream::document d;
      d << "index" << i << "value" << 1;
      collection.insert_one(d.view());
    }

    // Create a document pool
    Randomizer randomizer;
    document_pool::DocumentPool pool {randomizer, default_uri, document_pool::update_method::latest, nb_instances, nb_instances};
    pool.draw_document(db_name,col_name);
    std::this_thread::sleep_for(chrono::milliseconds {100});
    
    // Insert new instances to be fetched later
    for(int i=0; i < nb_instances; ++i) {
      builder::stream::document d;
      d << "index" << i << "value" << 2;
      collection.insert_one(d.view());
    }

    // Check cached document
    // For the test to work, execution of all threads must exceed the update
    vector<future<int>> futures;
    vector<thread> readers;

    for(int i=0; i < nb_instances; ++i) {
      std::promise<int> read_valid_promise;
      futures.emplace_back(read_valid_promise.get_future());

      thread reader {[&](decltype(read_valid_promise)&& result) {
        int max = 0;
        for(int i=0; i < nb_instances*nb_instances*1e2; ++i) {
          auto doc = pool.draw_document(db_name,col_name);
          if (doc) {
            auto value = LooseElement(doc->view())["value"].get<int>();
            if (max < value)
              max = value;
          }
        }
        result.set_value(max);
      }, std::move(read_valid_promise)};

      reader.detach();
      readers.emplace_back(std::move(reader));
    }

    int max_read = 0;
    for (auto& future : futures) {
      int value = future.get();
      if (max_read < value)
        max_read = value;
    }
    REQUIRE(max_read == 2);
  }

  // Clean
  collection.drop();
}

