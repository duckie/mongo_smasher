#include "value_pusher.h"

namespace mongo_smasher {

namespace bsx = bsoncxx;

std::function<void(bsx::builder::stream::single_context)> const& ValuePusher::get_pusher() {
  // Function creation is deferred here to make sure this points
  // correctly in case of multiple inheritance
  if (!function_) function_ = [this](bsx::builder::stream::single_context s) { return (*this)(s); };
  return function_;
}

}  // namespace mongo_smasher
