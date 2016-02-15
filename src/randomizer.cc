#include "randomizer.h"
#include <boost/filesystem/path.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#include <sstream>
#include <fstream>
#include "logger.h"

namespace bsx = bsoncxx;
using namespace std;
using str_view = bsx::stdx::string_view;
using bsx::stdx::make_unique;

namespace mongo_smasher {

namespace {

// Select a value in a vector
template <class Generator> class ValuePickPusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  std::vector<std::string> const &values_;

public:
  ValuePickPusher(Generator &gen, std::vector<std::string> const &values)
      : gen_(gen), distrib_(0, values.size() - 1), values_(values) {}

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << values_[distrib_(gen_)];
  }
};

struct StringPusherData {
  static str_view const alnums;
};
str_view const StringPusherData::alnums =
    "abcdefghijklmnopqrstuvwxyz0123456789";

template <class Generator>
class StringPusher : public ValuePusher, private StringPusherData {
  Generator &gen_;
  std::uniform_int_distribution<size_t> distrib_;
  // The char_chooser is kept here because it cannot stay const
  std::uniform_int_distribution<size_t> char_chooser_{
      0u, StringPusherData::alnums.size() - 1u};

public:
  StringPusher(Generator &gen, size_t min_size, size_t max_size)
      : gen_(gen), distrib_(min_size, max_size) {}

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ostringstream value_stream;
    size_t size = distrib_(gen_);
    for (size_t index = 0u; index < size; ++index)
      value_stream << alnums[char_chooser_(gen_)];
    ctx << value_stream.str();
  }
};

template <class Generator> class IntPusher : public ValuePusher {
  Generator &gen_;
  std::uniform_int_distribution<int> distrib_;

public:
  IntPusher(Generator &gen, int min, int max) : gen_(gen), distrib_(min, max) {}

  void operator()(bsoncxx::builder::stream::single_context ctx) override {
    ctx << distrib_(gen_);
  }
};
}

Randomizer::Randomizer(bsoncxx::document::view model, str_view root_path)
    : gen_(rd_()) {
  namespace bsx = bsoncxx;
  using bsx::document::view;
  using bsx::document::element;
  using bsx::stdx::string_view;

  boost::filesystem::path system_root_path(root_path.data());
  auto values = model["values"];

  // if (values.type() != bsx::type::k_array) {
  // log(log_level::fatal, "The \"values\" must be an array of objects.");
  // throw exception();
  //}

  auto values_view = values.get_document().view();
  for (auto values_it = values_view.cbegin(); values_it != values_view.cend();
       ++values_it) {
    auto value = values_it->get_document().view();
    auto type = to_str_view(value["type"]);
    auto name = values_it->key();
    if (name.empty() || type.empty()) {
      log(log_level::warning,
          "Neither name or type of a generator can be empty.\n");
      continue;
    }

    if (type == str_view("string")) {
      int min = value["min_size"].get_int32();
      int max = value["max_size"].get_int32();
      generators_.emplace(
          name, make_unique<StringPusher<decltype(gen_)>>(
                    gen_, static_cast<size_t>(min), static_cast<size_t>(max)));
    } else if (type == str_view("int")) {
      int min = value["min"].get_int32();
      int max = value["max"].get_int32();
      generators_.emplace(
          name, make_unique<IntPusher<decltype(gen_)>>(gen_, min, max));
    } else if (type == str_view("filepick")) {
      // Cache the file if not already done
      auto filename = to_str_view(value["file"]);
      auto value_list_it = value_lists_.find(filename);
      if (end(value_lists_) == value_list_it) {
        auto file_path = system_root_path;
        file_path.append(filename.data());
        std::ifstream file_in(file_path.string().c_str());
        if (!file_in) {
          log(log_level::fatal, "Cannot open file \"%s\".\n",
              file_path.string().c_str());
          throw exception();
        }

        std::string line;
        auto &new_vector = value_lists_[filename];
        while (std::getline(file_in, line))
          new_vector.emplace_back(line.c_str());

        log(log_level::debug, "File \"%s\" cached with %lu lines.\n",
            file_path.string().c_str(), new_vector.size());
      }

      // Register the type
      auto const &possible_values = value_lists_[filename];
      generators_.emplace(name, make_unique<ValuePickPusher<decltype(gen_)>>(
                                    gen_, possible_values));
    } else {
      log(log_level::error, "Unknown value type \"%s\".\n", type.data());
    }
  }

  log(log_level::info, "Randomizer caching finished.\n");
  return;
}

std::function<void(bsx::builder::stream::single_context)> const &
Randomizer::get_value_pusher(str_view name) {
  return generators_[name]->get_pusher();
}

}  // namespace mongo_smasher
