#pragma once
#ifndef MONGO_SMASHER_HEADER
#define MONGO_SMASHER_HEADER
#include <string>
#include <map>
#include <vector>
#include <random>
#include <cstdio>
#include <limits>
#include <algorithm>
#include <exception>
#include <bsoncxx/stdx/make_unique.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

namespace mongo_smasher {
using str_view = bsoncxx::stdx::string_view;
using bsoncxx::stdx::make_unique;

// Snippet used to simplify the enum <-> string relationship without macros

template <class EnumType> struct enum_view_size;

template <class EnumType> struct enum_view_definition {
  using type = std::array<std::string, enum_view_size<EnumType>::value> const;
  static type str_array;
};

template <class EnumType> struct enum_view {
  static_assert(
      std::is_enum<EnumType>::value &&
          std::is_convertible<typename std::underlying_type<EnumType>::type,
                              size_t>::value,
      "The enum underlying type must be convertible to size_t");

  static EnumType from_string(std::string const &value) {
    using namespace std;
    auto const &str_array = enum_view_definition<EnumType>::str_array;
    return static_cast<EnumType>(
        distance(begin(str_array), find(begin(str_array), end(str_array), value)));
  }

  static std::string to_string(EnumType value) {
    return enum_view_definition<EnumType>::str_array[static_cast<size_t>(
        value)];
  }
};

enum class log_level : size_t {
  debug,
  info,
  warning,
  error,
  fatal,
  quiet,
  LOG_LEVEL_MAX
};

template <> struct enum_view_size<log_level> {
  static constexpr size_t const value = static_cast<size_t>(log_level::LOG_LEVEL_MAX);
};

template <typename... T>
void raw_log(log_level level, char const *format, T &&... args) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
  std::printf("[%s] ", enum_view<log_level>::to_string(level).c_str());
  std::printf(format, args...);
#pragma clang diagnostic pop
}

log_level& mutable_global_log_level();
log_level global_log_level();

template <typename... T>
void log(log_level level, char const *format, T &&... args) {
  if (static_cast<size_t>(global_log_level()) <= static_cast<size_t>(level))
    raw_log(level,format,args...);
}

struct exception : public std::exception {
};

struct Config {
  std::string model_file;
  std::string host;
  size_t port;
  size_t threads;
};

class ValuePusher {
  std::function<void(bsoncxx::builder::stream::single_context)> function_;

 public:
  ValuePusher();
  virtual ~ValuePusher() = default;
  virtual void operator()(bsoncxx::builder::stream::single_context ctx) = 0;
  std::function<void(bsoncxx::builder::stream::single_context)>& get_pusher();
};

class Randomizer {
  std::string const alnums = "abcdefghijklmnopqrstuvwxyz0123456789";
  size_t const alnums_size = 36u;
  std::random_device rd_;
  mutable std::mt19937 gen_;
  mutable std::uniform_int_distribution<unsigned int> char_chooser_ =
      std::uniform_int_distribution<unsigned int>(0u, alnums_size - 1u);

  std::map<str_view, std::vector<std::string>> value_lists_;
  std::map<str_view, std::unique_ptr<ValuePusher>> generators_;

public:
  Randomizer(bsoncxx::document::view, bsoncxx::stdx::string_view root_path);
  ~Randomizer() = default;

  std::function<void(bsoncxx::builder::stream::single_context)>& get_value_pusher(str_view name);
  std::string getRandomString(size_t min, size_t max) const;
  template <class T> T const &getRandomPick(std::vector<T> const &values) const;
  template <class T>
  T getRandomInteger(T min = std::numeric_limits<T>::lowest(),
                     T max = std::numeric_limits<T>::max()) {
    static_assert(std::numeric_limits<T>::is_integer,
                  "The type used for Randomizer::getRandomInteger must be an "
                  "integer type.");
    std::uniform_int_distribution<T> int_chooser(min, max);
    return int_chooser(gen_);
  }
};

enum class frequency_type : size_t {
  linear,
  cyclic_gaussian,
  sinusoidal,
  FREQUENCY_TYPE_MAX
};

template <> struct enum_view_size<frequency_type> {
  static constexpr size_t const value = static_cast<size_t>(frequency_type::FREQUENCY_TYPE_MAX);
};

struct Collection {
  str_view name;
  bsoncxx::document::element model;
  double weight;
};

class ProcessingUnit {
  mongocxx::client db_conn_; 
  Randomizer& randomizer_;
  std::vector<Collection> collections_;

  void process_element(bsoncxx::document::element const& element, bsoncxx::builder::stream::array& ctx);
  void process_element(bsoncxx::document::element const& element, bsoncxx::builder::stream::document& ctx);

 public:
  ProcessingUnit(Randomizer& randomizer, std::string const& db_uri, bsoncxx::document::view collections);
  void process_tick();
};

void process_element(bsoncxx::document::element const& element, bsoncxx::builder::stream::single_context& ctx);
void run_stream(Config const& config);

} // namespace mongo_smasher

#endif // MONGO_SMASHER_HEADER
