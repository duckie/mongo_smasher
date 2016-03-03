#include "document_pool.h"

namespace mongo_smasher {
namespace document_pool {

template <>
typename enum_view_definition<update_method>::type
    enum_view_definition<update_method>::str_array = {"latests", "sample"};


// TODO not implemented yet, be done after find

}  // namespace document_pool
}  // namespace mongo_smasher
