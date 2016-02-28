#include "process_unit.h"
#include "mongo_smasher.h"
#include "logger.h"
#include <mongocxx/pipeline.hpp>
#include <sstream>

namespace mongo_smasher {

namespace bsx = bsoncxx;

namespace {}

ProcessingUnit::ProcessingUnit(Randomizer &randomizer, typename DocumentBatch::queue_t &queue,
                               bsoncxx::stdx::string_view name,
                               bsoncxx::document::view const &collection, double normalized_weight)
    : randomizer_{randomizer},
      queue_{queue},
      name_{name},
      model_{collection["schema"]},
      weight_{normalized_weight},
      bulk_size_{to_int<size_t>(collection, "bulk_size", 1u)} {
  bulk_docs_.reserve(bulk_size_);
}

KeyParams &ProcessingUnit::get_key_params(bsoncxx::stdx::string_view key) {
  auto key_it = key_params_.find(key);
  auto key_name = key;
  if (end(key_params_) == key_it) {
    // Read proba
    double probability{1.};
    auto probability_pos = key.find_last_of('?');
    if (probability_pos != bsx::stdx::string_view::npos) {
      std::istringstream probability_value(key.substr(probability_pos + 1).to_string());
      key_name = key.substr(0, probability_pos);
      double probability_read;
      if (probability_value >> probability_read) probability = probability_read;
    }
    if (probability < 0.) probability = 0.;
    if (1. < probability) probability = 1.;

    // Read array
    auto range_end = key.find_last_of(']');
    if (range_end != bsx::stdx::string_view::npos) {
      auto range_start = key.find_last_of('[');
      std::string range_expr = key.substr(range_start + 1, range_end - range_start - 1).to_string();
      try {
        RangeSizeGenerator *generator = &randomizer_.get_range_size_generator(range_expr);
        if (generator) {
          auto insert_result = key_params_.emplace(
              key, KeyParams{key_category::array, key.substr(0, range_start).to_string(), generator,
                             probability});
          return insert_result.first->second;
        }
      } catch (exception e) {
        // Nothing, it is a simple key then
      }
    }

    // Inserts a simple key
    log(log_level::debug, "Extracting simple key name \"%s\"\n", key_name.to_string().c_str());
    auto insert_result = key_params_.emplace(
        key, KeyParams{key_category::simple, key_name.to_string(), nullptr, probability});
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
  auto const &key = element.key();
  auto key_type = get_key_params(key);

  // Check if key should be inserted
  bool insert = true;
  if (key_type.probability < 1.) {
    insert = (randomizer_.existence_draw() <= key_type.probability);
  }

  if (insert) {
    if (key_category::array == key_type.category) {
      bsx::builder::stream::array child;
      auto array_size = key_type.range_size_generator->generate_size();
      for (size_t index = 0; index < array_size; ++index) {
        process_element(element, child);
      }
      ctx << key_type.name << bsx::types::b_array{child.view()};
    } else {
      if (element.type() == bsx::type::k_utf8) {
        auto value = to_str_view(element);
        // First stage are statments over the key
        if ('$' == value[0]) {
          ctx << key_type.name << randomizer_.get_value_pusher(name_, value.substr(1));
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
}

typename DocumentBatch::queue_t::duration_t ProcessingUnit::process_tick() {
  if (1. <= weight_ || randomizer_.existence_draw() <= weight_) {
    bsx::builder::stream::document document;
    for (auto value : model_.get_document().view()) {
      try {
        process_element(value, document);
      } catch (exception e) {
        // TODO: Only catch failures due to foreign refs here
        return {};
      }
    }
    bulk_docs_.emplace_back(document.extract());
    if (bulk_size_ <= bulk_docs_.size()) {
      // db_col_.insert_many(bulk_views_);
      auto idle_time = queue_.push({bsx::stdx::string_view{"test"}, name_, std::move(bulk_docs_)});
      nb_instances_ += bulk_size_;
      bulk_docs_.clear();
      return idle_time;
    }
  }
  return {};
}

bsx::stdx::string_view ProcessingUnit::name() const {
  return name_;
}

size_t ProcessingUnit::nb_inserted() const {
  return nb_instances_;
}

}  // namespace mongo_smasher
