#include "value_pusher.h"

namespace mongo_smasher {

namespace bsx = bsoncxx;

ValuePusher::ValuePusher()
    : function_{[this](bsx::builder::stream::single_context s) {
        return (*this)(s);
      }} {}

std::function<void(bsx::builder::stream::single_context)> &
ValuePusher::get_pusher() {
  return function_;
}

}  // namespace mongo_smasher
