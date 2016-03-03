#include "randomizer.h"
#include <bsoncxx/stdx/make_unique.hpp>
#include <sstream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <cppformat/format.h>
#include <chrono>
#include "logger.h"
#include "utils.h"

namespace bsx = bsoncxx;
using namespace std;
using namespace std::chrono;
using str_view = bsx::stdx::string_view;
using bsx::stdx::make_unique;

namespace mongo_smasher {

namespace {

std::regex simple_range_regex{"(\\d+):(\\d+)"};

template <class Generator>
class SimpleRangeSizeGenerator : public RangeSizeGenerator {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;

 public:
  SimpleRangeSizeGenerator(Generator &gen, size_t min, size_t max) : gen_(gen), distrib_(min, max) {
  }
  size_t generate_size() override {
    return distrib_(gen_);
  };
};

// Select a value in a vector
template <class Generator>
class ValuePickPusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  std::vector<std::string> const &values_;

 public:
  ValuePickPusher(Generator &gen, std::vector<std::string> const &values)
      : gen_(gen), distrib_(0, values.size() - 1), values_(values) {
  }

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << values_[distrib_(gen_)];
  }

  std::string get_as_string() override {
    return values_[distrib_(gen_)];
  }
};

struct StringPusherData {
  static str_view const alnums;
};
str_view const StringPusherData::alnums = "abcdefghijklmnopqrstuvwxyz0123456789";

template <class Generator>
class StringPusher : public ValuePusher, private StringPusherData {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  // The char_chooser is kept here because it cannot stay const
  std::uniform_int_distribution<size_t> char_chooser_{0u, StringPusherData::alnums.size() - 1u};

 public:
  StringPusher(Generator &gen, size_t min_size, size_t max_size)
      : gen_(gen), distrib_(min_size, max_size) {
  }

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << get_as_string();
  }

  std::string get_as_string() override {
    fmt::MemoryWriter value_stream;
    size_t size = distrib_(gen_);
    for (size_t index = 0u; index < size; ++index) value_stream << alnums[char_chooser_(gen_)];
    return value_stream.str();
  }
};

template <class Generator>
class AsciiStringPusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  // The char_chooser is kept here because it cannot stay const
  std::uniform_int_distribution<unsigned char> char_chooser_{1u, 255u};

 public:
  AsciiStringPusher(Generator &gen, size_t min_size, size_t max_size)
      : gen_(gen), distrib_(min_size, max_size) {
  }

  std::string get_as_string() override {
    fmt::MemoryWriter value_stream;
    size_t size = distrib_(gen_);
    for (size_t index = 0u; index < size; ++index)
      value_stream << static_cast<char>(char_chooser_(gen_));
    return value_stream.str();
  }

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << get_as_string();
  }
};

template <class Generator>
class IntPusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<int> distrib_;

 public:
  IntPusher(Generator &gen, int min, int max) : gen_(gen), distrib_(min, max) {
  }

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << distrib_(gen_);
  }

  std::string get_as_string() override {
    return fmt::format("{}", distrib_(gen_));
  }
};

template <class Generator>
class DoublePusher : public ValuePusher {
  Generator &gen_;
  std::uniform_real_distribution<double> distrib_;

 public:
  DoublePusher(Generator &gen, double min, double max) : gen_(gen), distrib_(min, max) {
  }

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << bsx::types::b_double{distrib_(gen_)};
  }

  std::string get_as_string() override {
    return fmt::format("{}", distrib_(gen_));
  }
};

class IncrementalIDPusher : public ValuePusher {
  int64_t id_ = 0;

 public:
  IncrementalIDPusher() = default;
  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << bsx::types::b_utf8{std::to_string(++id_)};
  }

  std::string get_as_string() override {
    return fmt::format("{}", ++id_);
  }
};

template <class Generator>
class DatePusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;

 public:
  DatePusher(Generator &gen, high_resolution_clock::time_point const &min,
             high_resolution_clock::time_point const &max)
      : gen_(gen),
        distrib_(duration_cast<seconds>(min.time_since_epoch()).count(),
                 duration_cast<seconds>(max.time_since_epoch()).count()) {
  }

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << bsx::types::b_date{
        static_cast<int64_t>(1000u * distrib_(gen_))};  // b_date takes milliseconds
  }

  std::string get_as_string() override {
    ostringstream value_stream;
    time_t time{static_cast<time_t>(distrib_(gen_))};
    value_stream << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return value_stream.str();
  }
};
}

Randomizer::Randomizer(bsoncxx::document::view model, str_view root_path)
    : values_(model), gen_(rd_()), system_root_path_(root_path.data()) {
  using bsx::document::view;
  using bsx::document::element;
  using bsx::stdx::string_view;

  // Doesnt exist we have to insert it
  boost::filesystem::path system_root_path(root_path.data());
  for (auto values_it = values_.cbegin(); values_it != values_.cend(); ++values_it) {
    auto value = values_it->get_document().view();
    auto type = to_str_view(value["type"]);
    auto name = values_it->key();
    if (name.empty() || type.empty()) {
      log(log_level::warning, "Neither name or type of a generator can be empty.\n");
      continue;
    }

    if (type == str_view("string")) {
    } else if (type == str_view("ascii_string")) {
    } else if (type == str_view("int")) {
    } else if (type == str_view("double")) {
    } else if (type == str_view("incremental_id")) {
    } else if (type == str_view("date")) {
    } else if (type == str_view("pick")) {
      // Is it a file ?
      auto file_it = value.find("file");
      if (file_it != value.end()) {
        // Cache the file if not already done
        auto filename = to_str_view(*file_it);
        auto value_list_it = value_lists_.find(name);
        if (end(value_lists_) == value_list_it) {
          auto file_path = system_root_path;
          file_path.append(filename.data());
          std::ifstream file_in(file_path.string().c_str());
          if (!file_in) {
            log(log_level::fatal, "Cannot open file \"%s\".\n", file_path.string().c_str());
            throw exception();
          }

          std::string line;
          auto &new_vector = value_lists_[name];
          while (std::getline(file_in, line)) new_vector.emplace_back(line.c_str());

          log(log_level::debug, "File \"%s\" cached with %lu lines.\n", file_path.string().c_str(),
              new_vector.size());
        }
      } else {
        auto local_values_it = value.find("values");
        if (local_values_it != value.end()) {
          if (local_values_it->type() == bsx::type::k_array) {
            auto &new_vector = value_lists_[name];
            for (auto local_value : local_values_it->get_array().value) {
              new_vector.push_back(local_value.get_utf8().value.to_string());
            }
          }
        } else {
          log(log_level::error, "Value \"%s\" should either have a file or a list of values.\n",
              name.data());
        }
      }
    } else {
      log(log_level::error, "Unknown value type \"%s\".\n", type.data());
    }
  }
  log(log_level::info, "Randomizer caching finished.\n");
}

RangeSizeGenerator &Randomizer::get_range_size_generator(
    bsoncxx::stdx::string_view range_expression) {
  auto gen_it = range_size_generators_.find(range_expression.to_string());
  if (end(range_size_generators_) == gen_it) {
    std::unique_ptr<RangeSizeGenerator> new_generator;
    std::smatch base_match;
    std::string str_range_expression = range_expression.to_string();
    if (std::regex_match(str_range_expression, base_match, simple_range_regex)) {
      new_generator.reset(new SimpleRangeSizeGenerator<decltype(gen_)>(
          gen_, std::stoul(base_match[1].str(), nullptr, 10),
          std::stoul(base_match[2].str(), nullptr, 10)));
    }
    if (new_generator) {
      auto insert_result =
          range_size_generators_.emplace(str_range_expression, std::move(new_generator));
      if (insert_result.second)
        return *insert_result.first->second;
      else
        throw exception();
    }
  }
  return *gen_it->second;
}

auto Randomizer::random_generator() -> random_engine_t & {
  return gen_;
}

double Randomizer::existence_draw() {
  return key_existence_(gen_);
}

ValuePusher *Randomizer::get_value_pusher(str_view col_name, str_view name) {
  namespace bsx = bsoncxx;
  using bsx::document::view;
  using bsx::document::element;
  using bsx::stdx::string_view;

  auto& col_generators = generators_[col_name.to_string()];
  auto name_str = name.to_string();
  auto value_pusher_it = col_generators.find(name_str);
  bool inserted = false;
  if (std::end(col_generators) == value_pusher_it) {
    // Doesnt exist we have to insert it
    auto value_it = values_.find(name);
    if (value_it != end(values_)) {
      auto &value = *value_it;
      auto type = to_str_view(value["type"]);
      if (name.empty() || type.empty()) {
        log(log_level::warning, "Neither name or type of a generator can be empty.\n");
        throw exception();
      }

      // log(log_level::error, fmt::format("Try {} => {}", type.data(), type ==
      // str_view("string")).data());
      if (type == str_view("string")) {
        int min = value["min_size"].get_int32();
        int max = value["max_size"].get_int32();
        std::tie(value_pusher_it, inserted) = col_generators.emplace(
            name_str, make_unique<StringPusher<decltype(gen_)>>(gen_, static_cast<size_t>(min),
                                                            static_cast<size_t>(max)));
      } else if (type == str_view("ascii_string")) {
        int min = value["min_size"].get_int32();
        int max = value["max_size"].get_int32();
        std::tie(value_pusher_it, inserted) = col_generators.emplace(
            name_str, make_unique<AsciiStringPusher<decltype(gen_)>>(gen_, static_cast<size_t>(min),
                                                                 static_cast<size_t>(max)));
      } else if (type == str_view("int")) {
        int min = value["min"].get_int32();
        int max = value["max"].get_int32();
        std::tie(value_pusher_it, inserted) =
            col_generators.emplace(name_str, make_unique<IntPusher<decltype(gen_)>>(gen_, min, max));
      } else if (type == str_view("date")) {
        auto min = parse_iso_date(value["min"].get_utf8().value);
        auto max = parse_iso_date(value["max"].get_utf8().value);
        std::tie(value_pusher_it, inserted) =
            col_generators.emplace(name_str, make_unique<DatePusher<decltype(gen_)>>(gen_, min, max));
      } else if (type == str_view("double")) {
        double min = value["min"].get_double();
        double max = value["max"].get_double();
        std::tie(value_pusher_it, inserted) =
            col_generators.emplace(name_str, make_unique<DoublePusher<decltype(gen_)>>(gen_, min, max));
      } else if (type == str_view("incremental_id")) {
        std::tie(value_pusher_it, inserted) =
            col_generators.emplace(name_str, make_unique<IncrementalIDPusher>());
      } else if (type == str_view("pick")) {
        // Register the type
        auto const &possible_values = value_lists_[name];
        std::tie(value_pusher_it, inserted) = col_generators.emplace(
            name_str, make_unique<ValuePickPusher<decltype(gen_)>>(gen_, possible_values));
      }

      if (!inserted) {
        log(log_level::error, "Unknown value type \"%s\".\n", type.data());
      }
    }

    // Not an error in case the key does not exist
    if (!inserted) {
      std::tie(value_pusher_it, inserted) = col_generators.emplace(name_str, nullptr);
    }
  }

  return value_pusher_it->second.get();
}

}  // namespace mongo_smasher
