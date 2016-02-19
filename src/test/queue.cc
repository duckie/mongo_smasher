#include <catch.hpp>
#include "../queue.h"
#include <thread>
#include <future>
#include "../logger.h"

using namespace std;
using namespace mongo_smasher;

namespace {
struct Movable {
  bool valid {true};
  int const value {0};
  Movable() = default;
  Movable(Movable&& other) {
    other.valid = false;
  }
  Movable(Movable const&) = delete;
  Movable& operator=(Movable&& other) {
    valid = true;
    other.valid = false;
    return *this;
  }
  Movable& operator=(Movable const&) = delete;
  bool is_valid() const { return valid; }
};

bool run_writers_readers(Queue<Movable>& queue, size_t nb_writers, size_t nb_writes, size_t nb_readers) {
  // Prepare number of iterations
  size_t nb_reads = nb_writers * nb_writes / nb_readers;
  if (0 == nb_reads) {
    // failure : test would block
    return false;
  }

  // Launch writers
  vector<thread> writers;
  for (int w=0; w < nb_writers; ++w) {
    thread writer {[&queue,&nb_writes]() -> void { 
      for (int i=0; i< nb_writes; ++i) {
        queue.push(Movable {});
        //log(log_level::info,"Push one\n");
      }
    }};
    writer.detach();
    writers.emplace_back(move(writer));
  }

  vector<future<bool>> futures;
  vector<thread> readers;

  for (int r=0; r < nb_readers; ++r) {
    std::promise<bool> read_valid_promise;
    futures.emplace_back(read_valid_promise.get_future());

    thread reader {[&queue,&nb_reads] (decltype(read_valid_promise)&& result) -> void {
      bool valid = true;
      for (int i=0; i< nb_reads; ++i) {
        Movable m = queue.pop();
        //log(log_level::info,"Pop one\n");
        valid = valid && m.is_valid();
      }
      result.set_value(valid); 
    }, std::move(read_valid_promise) };
    reader.detach();
    readers.emplace_back(move(reader));
  }

  bool aggreg_result = true;
  for (auto& fut : futures)
    aggreg_result = aggreg_result && fut.get();

  return aggreg_result;
}
}

TEST_CASE("Single sized queue","[queue]") {
  Queue<Movable> queue { 1u };

  REQUIRE(queue.size() == 0);

  SECTION("Simple push/pop") {
   Movable a;
   queue.push(std::move(a));
   REQUIRE(1 == queue.size()); 
   REQUIRE(!a.is_valid());

   Movable b = queue.pop();
   REQUIRE(0 == queue.size());
   REQUIRE(b.is_valid());
  }

  SECTION("One writer one reader") {
    REQUIRE(run_writers_readers(queue,1,1000,1));
  }

  SECTION("Many writers one reader") {
    REQUIRE(run_writers_readers(queue,10,1000,1));
  }

  SECTION("One writer many readers") {
    REQUIRE(run_writers_readers(queue,1,1000,10));
  }

  SECTION("Many writers many readers - Case 1") {
    REQUIRE(run_writers_readers(queue,10,10000,10));
  }

  SECTION("Many writers many readers - Case 2") {
    REQUIRE(run_writers_readers(queue,5,10000,10));
  }

  SECTION("Many writers many readers - Case 3") {
    REQUIRE(run_writers_readers(queue,10,10000,5));
  }
}

TEST_CASE("Multi sized queue","[queue]") {
  Queue<Movable> queue { 10u };

  REQUIRE(queue.size() == 0);

  SECTION("Simple push/pop") {
   Movable a;
   queue.push(std::move(a));
   REQUIRE(1 == queue.size()); 
   REQUIRE(!a.is_valid());

   Movable b = queue.pop();
   REQUIRE(0 == queue.size());
   REQUIRE(b.is_valid());
  }

  SECTION("One writer one reader") {
    REQUIRE(run_writers_readers(queue,1,1000,1));
  }

  SECTION("Many writers one reader") {
    REQUIRE(run_writers_readers(queue,10,1000,1));
  }

  SECTION("One writer many readers") {
    REQUIRE(run_writers_readers(queue,1,1000,10));
  }

  SECTION("Many writers many readers - Case 1") {
    REQUIRE(run_writers_readers(queue,10,10000,10));
  }

  SECTION("Many writers many readers - Case 2") {
    REQUIRE(run_writers_readers(queue,5,10000,10));
  }

  SECTION("Many writers many readers - Case 3") {
    REQUIRE(run_writers_readers(queue,10,10000,5));
  }
}

