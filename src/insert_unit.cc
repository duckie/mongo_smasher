#include "insert_unit.h"
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

InsertUnit::InsertUnit(Randomizer &randomizer, typename DocumentBatch::queue_t &queue,
                       bsoncxx::stdx::string_view name, bsoncxx::document::view const &collection,
                       double normalized_weight)
    : randomizer_{randomizer},
      queue_{queue},
      name_{name},
      model_{collection["schema"]},
      weight_{normalized_weight},
      bulk_size_{LooseElement(collection)["actions"]["insert"]["bulk_size"].get<size_t>(1)} {
  bulk_docs_.reserve(bulk_size_);
  log(log_level::debug, "Insert unit for \"%s\" created with bulk_size=%lu, weight=%f.\n",
      name.data(), bulk_size_, weight_);
}

KeyParams &InsertUnit::get_key_params(bsoncxx::stdx::string_view key) {
  auto key_it = key_params_.find(key);
  auto key_name = key;
  if (end(key_params_) == key_it) {
    // Read proba
    bool inserted = false;
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
      auto size_generator = randomizer_.make_range_size_generator(range_expr);
      if (size_generator) {
        std::tie(key_it, inserted) = key_params_.emplace(
            key, KeyParams{key_category::array, key.substr(0, range_start).to_string(),
                           std::move(size_generator), probability});
      }
    }

    if (!inserted) {
      // Inserts a simple key
      log(log_level::debug, "Extracting simple key name \"%s\"\n", key_name.to_string().c_str());
      std::tie(key_it, inserted) = key_params_.emplace(
          key, KeyParams{key_category::simple, key_name.to_string(), nullptr, probability});
    }
  }
  return key_it->second;
}

ValueParams &InsertUnit::get_value_params(bsoncxx::stdx::string_view value) {
  auto value_it = value_params_.find(value);
  bool inserted = false;
  if (end(value_params_) == value_it) {
    std::string to_match = value.to_string();
    if (std::regex_match(to_match, value_regex)) {
      // Try to get the pusher
      auto content = value.substr(1);
      auto pusher = randomizer_.make_value_pusher(content);
      if (pusher) {
        log(log_level::debug, "Value \"%s\" registered as a simple value.\n", value.data());
        std::tie(value_it, inserted) =
            value_params_.emplace(value, ValueParams{value_category::simple, {}});
        value_it->second.values.emplace_back(
            TokenParams{token_type::value, content.to_string(), std::move(pusher)});
      }
    } else {
      // Try a search to see if it is a compound
      size_t nb_found{0};
      std::smatch base_match;
      std::vector<TokenParams> values_sequence;
      std::string buffer = to_match;
      while (std::regex_search(buffer, base_match, value_regex)) {
        // Try to get the pusher
        auto content = base_match[0].str().substr(1);
        auto pusher = randomizer_.make_value_pusher(content);
        if (pusher) {
          // Ok this a interpretable key
          values_sequence.emplace_back(
              TokenParams{token_type::stale, base_match.prefix().str(), nullptr});
          values_sequence.emplace_back(
              TokenParams{token_type::value, base_match[0].str().substr(1), std::move(pusher)});
          ++nb_found;
        } else {
          // Key not referenced, just a string then
          // Not bad to add strings here because it is a one time cost
          values_sequence.emplace_back(TokenParams{
              token_type::stale, base_match.prefix().str() + base_match[0].str(), nullptr});
        }
        buffer = base_match.suffix().str();
      }

      // TODO: Pack consecutive token_type::stale for better performances

      if (0 < nb_found) {
        values_sequence.emplace_back(TokenParams{token_type::stale, buffer, nullptr});
        log(log_level::debug, "Value \"%s\" registered as a compound value.\n", value.data());
        std::tie(value_it, inserted) = value_params_.emplace(
            value, ValueParams{value_category::compound, std::move(values_sequence)});
      }
    }

    if (!inserted) {
      // Finally it is a stale one
      log(log_level::debug, "Value \"%s\" registered as a stale value.\n", value.data());
      std::tie(value_it, inserted) =
          value_params_.emplace(value, ValueParams{value_category::stale, {}});
      value_it->second.values.emplace_back(
          TokenParams{token_type::stale, value.to_string(), nullptr});
    }
  }
  return value_it->second;
}

template <class T>
void InsertUnit::process_value(T &ctx, ValueParams &value_params) {
  switch (value_params.category) {
    case value_category::stale:
      ctx << value_params.values[0].content;
      break;
    case value_category::simple:
      ctx << value_params.values[0].pusher->get_pusher();
      break;
    case value_category::compound: {
      fmt::MemoryWriter result;
      for (auto &value_seq_elem : value_params.values) {
        switch (value_seq_elem.type) {
          case token_type::stale:
            result << value_seq_elem.content;
            break;
          case token_type::value:
            result << value_seq_elem.pusher->get_as_string();
            break;
        }
      }
      ctx << result.str();
    } break;
  }
}

template <class T>
void InsertUnit::process_element(T const &element, bsoncxx::builder::stream::array &ctx) {
  if (element.type() == bsx::type::k_utf8) {
    auto value = to_str_view(element);
    // get_value_params(value);
    process_value(ctx, get_value_params(value));
    // ctx << randomizer_.get_value_pusher(name_, value.substr(1)).get_pusher();
    //// First stage are statements over the key
    // if ('$' == value[0]) {
    //}
  } else if (element.type() == bsx::type::k_document) {
    bsx::builder::stream::document child;
    for (auto value : element.get_document().view()) {
      process_element(value, child);
    }
    ctx << bsx::types::b_document{child.view()};
  } else {
    ctx << element.get_value();
  }
}

void InsertUnit::process_element(bsoncxx::array::element const &element,
                                 bsx::builder::stream::array &ctx) {
  return process_element<bsoncxx::array::element>(element, ctx);
}

void InsertUnit::process_element(bsoncxx::document::element const &element,
                                 bsx::builder::stream::array &ctx) {
  return process_element<bsoncxx::document::element>(element, ctx);
}

void InsertUnit::process_element(bsoncxx::document::element const &element,
                                 bsx::builder::stream::document &ctx) {
  auto const &key = element.key();
  auto &key_type = get_key_params(key);

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
        auto inner_ctx = (ctx << key_type.name);
        process_value(inner_ctx, get_value_params(value));
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
      } else {
        ctx << element.key() << element.get_value();
      }
    }
  }
}

typename DocumentBatch::queue_t::duration_t InsertUnit::process_tick() {
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
      auto idle_time = queue_.push({bsx::stdx::string_view{"test"}, name_, std::move(bulk_docs_)});
      nb_instances_ += bulk_size_;
      bulk_docs_.clear();
      return idle_time;
    }
  }
  return {};
}

size_t InsertUnit::nb_inserted() const {
  return nb_instances_;
}

}  // namespace mongo_smasher
