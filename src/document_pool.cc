#include "document_pool.h"

namespace mongo_smasher {
namespace document_pool {

}  // namespace document_pool

template <>
typename enum_view_definition<document_pool::update_method>::type
    enum_view_definition<document_pool::update_method>::str_array = {"latests", "sample"};
}  // namespace mongo_smasher
