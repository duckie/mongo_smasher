#include "process_unit.h"
#include "logger.h"
#include <mongocxx/pipeline.hpp>
#include "mongo_smasher.h"

namespace mongo_smasher {

namespace bsx = bsoncxx;

namespace {}

ProcessingUnit::ProcessingUnit(Randomizer &randomizer, CollectionHub &collections,
                               bsx::stdx::string_view name,
                               bsoncxx::document::view const &collection)
    : randomizer_{randomizer},
      collections_{collections},
      db_col_{collections[name.to_string()]},
      name_{name},
      model_(collection["schema"]),
      bulk_size_(to_int<size_t>(collection, "bulk_size", 1u)) {
  bulk_docs_.reserve(bulk_size_);
  bulk_views_.reserve(bulk_size_);
}

auto ProcessingUnit::get_key_params(bsoncxx::stdx::string_view key) -> key_params & {
  auto key_it = key_params_.find(key);
  if (end(key_params_) == key_it) {
    if (']' == key.back()) {
      auto range_start = key.find_last_of('[');
      auto range_end = key.find_last_of(']');
      std::string range_expr = key.substr(range_start + 1, range_end - range_start - 1).to_string();
      try {
        RangeSizeGenerator *generator = &randomizer_.get_range_size_generator(range_expr);
        if (generator) {
          auto insert_result = key_params_.emplace(
              key,
              key_params{key_category::array, key.substr(0, range_start).to_string(), generator});
          return insert_result.first->second;
        }
      } catch (exception e) {
        // Nothing, it is a simple key then
      }
    }

    // Inserts a simple key
    auto insert_result =
        key_params_.emplace(key, key_params{key_category::simple, key.to_string(), nullptr});
    return insert_result.first->second;
  }
  return key_it->second;
}

// TODO: Does not work, dunno why for now
// bsoncxx::document::view ProcessingUnit::get_foreign_view(bsoncxx::stdx::string_view foreign_name)
// {
// auto cursors_it = foreign_cursors_.find(foreign_name.to_string());
//// If the curosr does not exist or is expired, lets renew it
// if (end(foreign_cursors_) == cursors_it || cursors_it->second.second ==
// cursors_it->second.first.end()) {
// int step = 1;
// log(log_level::debug, "Aggregagte step %d\n", step++);
// auto foreign_col = collections_[foreign_name.to_string()];
// auto new_cursor = foreign_col.aggregate(mongocxx::pipeline().sample(1));
//
// if (std::begin(new_cursor) != std::end(new_cursor)) {
// log(log_level::debug, "Aggregagte step %d\n", step++);
// if (end(foreign_cursors_) != cursors_it) {
// log(log_level::debug, "Aggregagte erase step %d\n", step++);
// foreign_cursors_.erase(cursors_it);
//}
//
// auto inserted_cursor_result = foreign_cursors_.emplace(foreign_name.to_string(),
// std::make_pair(std::move(new_cursor), new_cursor.begin()));
// log(log_level::debug, "Aggregagte step %d\n", step++);
// if (inserted_cursor_result.second) {
// log(log_level::debug, "Aggregagte step %d\n", step++);
// auto& inserted_cursor = inserted_cursor_result.first;
// inserted_cursor->second.second = inserted_cursor->second.first.begin();
// auto view = *inserted_cursor->second.first.begin();
////auto view = *inserted_cursor->second.second;
////++(inserted_cursor->second.second);
// return view;
//}
//}
// throw exception();
//}
//
// auto view = *cursors_it->second.second;
//++cursors_it->second.second;
// return view;
//}

void ProcessingUnit::process_element(bsoncxx::array::element const &element,
                                     bsx::builder::stream::array &ctx) {
  if (element.type() == bsx::type::k_utf8) {
    auto value = to_str_view(element);
    // First stage are statements over the key
    if ('$' == value[0]) {
      ctx << randomizer_.get_value_pusher(name_, value.substr(1));
    }
  } else if (element.type() == bsx::type::k_document) {
    bsx::builder::stream::document child;
    for (auto value : element.get_document().view()) {
      process_element(value, child);
    }
    ctx << bsx::types::b_document{child.view()};
  }
}

void ProcessingUnit::process_element(bsoncxx::document::element const &element,
                                     bsx::builder::stream::array &ctx) {
  if (element.type() == bsx::type::k_utf8) {
    auto value = to_str_view(element);
    // First stage are statements over the key
    if ('$' == value[0]) {
      ctx << randomizer_.get_value_pusher(name_, value.substr(1));
    }
  } else if (element.type() == bsx::type::k_document) {
    bsx::builder::stream::document child;
    for (auto value : element.get_document().view()) {
      process_element(value, child);
    }
    ctx << bsx::types::b_document{child.view()};
  }
}

void ProcessingUnit::process_element(bsoncxx::document::element const &element,
                                     bsx::builder::stream::document &ctx) {
  auto key = element.key();
  auto key_type = get_key_params(key);
  if (key_category::array == std::get<0>(key_type)) {
    bsx::builder::stream::array child;
    auto array_size = std::get<2>(key_type)->generate_size();
    for (size_t index = 0; index < array_size; ++index) {
      process_element(element, child);
    }
    ctx << std::get<1>(key_type) << bsx::types::b_array{child.view()};
  } else {
    if (element.type() == bsx::type::k_utf8) {
      auto value = to_str_view(element);
      // First stage are statments over the key
      if ('$' == value[0]) {
        ctx << element.key() << randomizer_.get_value_pusher(name_, value.substr(1));
      }
      // TODO: References to foreign table disabled for now
      // else if ('&' == value[0]) {
      // try {
      // ctx << element.key() << bsx::types::b_document{get_foreign_view(value.substr(1))};
      //}
      // catch(exception e) {
      //// Retrhow
      // throw e;
      //}
      //}
    } else if (element.type() == bsx::type::k_document) {
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
    }
  }
}

void ProcessingUnit::process_tick() {
  // auto db_collection = db_conn_["test"][name_];
  bsx::builder::stream::document document;
  for (auto value : model_.get_document().view()) {
    try {
      process_element(value, document);
    } catch (exception e) {
      // TODO: Only catch failures due to foreign refs here
      return;
    }
  }
  bulk_docs_.emplace_back(std::move(document));
  bulk_views_.emplace_back(bulk_docs_.back().view());
  if (bulk_size_ <= bulk_docs_.size()) {
    db_col_.insert_many(bulk_views_);
    nb_instances_ += bulk_size_;
    bulk_docs_.clear();
    bulk_views_.clear();
  }
}

bsx::stdx::string_view ProcessingUnit::name() const {
  return name_;
}

size_t ProcessingUnit::nb_inserted() const {
  return nb_instances_;
}

}  // namespace mongo_smasher
