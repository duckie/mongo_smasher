#pragma once
#include <functional>
#include <bsoncxx/builder/stream/document.hpp>

namespace mongo_smasher {

class ValuePusher {
  std::function<void(bsoncxx::builder::stream::single_context)> function_;

 public:
  ValuePusher() = default;
  virtual ~ValuePusher() = default;
  virtual void operator()(bsoncxx::builder::stream::single_context ctx) = 0;

  // We are forced to use this indirection here because bsoncxx::utils::is_functor
  // does not accept polymorphic functors (more accurately, functors with a virtual
  // destructor)
  std::function<void(bsoncxx::builder::stream::single_context)> const & get_pusher();
};

}  // namespace mongo_smasher
