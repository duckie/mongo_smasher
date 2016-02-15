#pragma once
#include <functional>
#include <bsoncxx/builder/stream/document.hpp>

namespace mongo_smasher {

class ValuePusher {
  std::function<void(bsoncxx::builder::stream::single_context)> function_;

 public:
  ValuePusher();
  virtual ~ValuePusher() = default;
  virtual void operator()(bsoncxx::builder::stream::single_context ctx) = 0;
  std::function<void(bsoncxx::builder::stream::single_context)>& get_pusher();
};

}  // namespace mongo_smasher
