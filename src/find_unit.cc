#include "find_unit.h"
#include "mongo_smasher.h"
#include "logger.h"
#include <mongocxx/pipeline.hpp>
#include <sstream>
#include <regex>
#include <cppformat/format.h>

namespace mongo_smasher {

namespace bsx = bsoncxx;

namespace {
std::regex value_regex{"\\$[a-zA-Z0-9_-]+"};
}

FindUnit::FindUnit(document_pool::DocumentPool &pool, typename ConsumerCommand::queue_t &queue,
                   bsoncxx::stdx::string_view name, bsoncxx::document::view const &pattern,
                   double normalized_weight)
    : pool_{pool},
      queue_{queue},
      name_{name},
      model_{pattern},
      weight_{normalized_weight},
      bulk_size_{LooseElement(pattern)["bulk_size"].get<size_t>(1)} {
  log(log_level::debug, "Find unit for \"%s\" created with bulk_size=%lu, weight=%f.\n",
      name.data(), bulk_size_, weight_);
}

double FindUnit::weight() const {
  return weight_;
}

// template <class T>
// void FindUnit::process_value(T &ctx, ValueParams &value_params) {
// switch (value_params.category) {
// case value_category::stale:
// ctx << value_params.values[0].content;
// break;
// case value_category::simple:
// ctx << value_params.values[0].pusher->get_pusher();
// break;
// case value_category::compound: {
// fmt::MemoryWriter result;
// for (auto &value_seq_elem : value_params.values) {
// switch (value_seq_elem.type) {
// case token_type::stale:
// result << value_seq_elem.content;
// break;
// case token_type::value:
// result << value_seq_elem.pusher->get_as_string();
// break;
//}
//}
// ctx << result.str();
//} break;
//}
//}

template <class T>
void FindUnit::process_element(T const &element, bsoncxx::builder::stream::array &ctx) {
  if (element.type() == bsx::type::k_document) {
    bsx::builder::stream::document child;
    for (auto value : element.get_document().view()) {
      process_element(value, child);
    }
    ctx << bsx::types::b_document{child.view()};
  } else if (element.type() == bsx::type::k_array) {
    bsx::builder::stream::array child;
    for (auto value : element.get_array().value) {
      process_element(value, child);
    }
    ctx << bsx::types::b_array{child.view()};
  } else {
    ctx << element.get_value();
  }
}

void FindUnit::process_element(bsoncxx::array::element const &element,
                               bsx::builder::stream::array &ctx) {
  return process_element<bsoncxx::array::element>(element, ctx);
}

void FindUnit::process_element(bsoncxx::document::element const &element,
                               bsx::builder::stream::array &ctx) {
  return process_element<bsoncxx::document::element>(element, ctx);
}

void FindUnit::process_element(bsoncxx::document::element const &element,
                               bsx::builder::stream::document &ctx) {
  auto const &key = element.key();

  if (element.type() == bsoncxx::type::k_utf8) {
    ctx << key << element.get_value();
  } 
  else if (element.type() == bsx::type::k_document) {
    bsx::builder::stream::document child;
    for (auto value : element.get_document().view()) {
      process_element(value, child);
    }
    ctx << element.key() << bsx::types::b_document{child.view()};
  } else if (element.type() == bsx::type::k_array) {
    bsx::builder::stream::array child;
    for (auto value : element.get_array().value) {
      process_element(value, child);
    }
    ctx << element.key() << bsx::types::b_array{child.view()};
  } else {
    ctx << element.key() << element.get_value();
  }
}

typename ConsumerCommand::queue_t::duration_t FindUnit::process_tick() {
  // if (1. <= weight_ || randomizer_.existence_draw() <= weight_) {

  bulk_docs_.clear();
  bulk_docs_.reserve(2);

  // Builds criteria
  bsx::builder::stream::document document;
  for (auto value : model_["criteria"].get_document().view()) {
    try {
      process_element(value, document);
    } catch (exception e) {
      // TODO: Only catch failures due to foreign refs here
      return {};
    }
  }
  bulk_docs_.emplace_back(document.extract());

  mongocxx::options::find options;
  options.projection(model_["projection"].get_document().view());
  options.limit(LooseElement(model_)["limit"].get<size_t>(1));
   //if (bulk_size_ <= bulk_docs_.size()) {
  // TODO: Do not hard code db name here
   auto idle_time = queue_.push({command_type::find, bsx::stdx::string_view{"test"}, name_, std::move(bulk_docs_), {}, {}, std::move(options), {}});
   //nb_instances_ += bulk_size_;
   bulk_docs_.clear();
   return idle_time;
  //}
  //}
  //return {};
}

}  // namespace mongo_smasher
