#include "collection_consumer.h"
#include "queue.h"
#include <bsoncxx/stdx/string_view.hpp>
#include <bsoncxx/builder/stream/document.hpp>

namespace mongo_smasher {

struct DocumentBatch {
  using batch_t = std::vector<bsoncxx::builder::stream::document>;
  using queue_t = Queue<DocumentBatch>;

  bsoncxx::stdx::string_view db_name;
  bsoncxx::stdx::string_view db_col;
  batch_t batch;
};



}  // namespace mongo_smasher
