#include <catch.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/element.hpp>
#include <bsoncxx/array/value.hpp>
#include <bsoncxx/array/element.hpp>
#include "../utils.h"
#include "../logger.h"

using namespace std;
using namespace mongo_smasher;
using namespace bsoncxx;

TEST_CASE("Document model","[loose_model]") {
  document::value model = bsoncxx::from_json(R"json({"str":"Roger","int":1,"double":1.0})json");
  LooseElement model_view(model.view());

  REQUIRE(model_view["str"].get<std::string>() == "Roger");
  REQUIRE(model_view["str"].get<stdx::string_view>() == stdx::string_view("Roger"));
  REQUIRE(model_view["int"].get<int>() == 1);
  REQUIRE(model_view["double"].get<double>() == 1.);
  REQUIRE(model_view["str2"].get<std::string>() == "");
  REQUIRE(model_view["str2"].get<stdx::string_view>() == stdx::string_view(""));
  REQUIRE(model_view["int2"].get<int>() == 0);
  REQUIRE(model_view["double2"].get<double>() == 0.);

}

TEST_CASE("Array model","[loose_model]") {
  document::value model = bsoncxx::from_json(R"json({"array":["Roger",1,1.0,null]})json");
  LooseElement root_view(model.view());
  LooseElement model_view(root_view["array"]);

  REQUIRE(model_view[0].get<std::string>() == "Roger");
  REQUIRE(model_view[0].get<stdx::string_view>() == stdx::string_view("Roger"));
  REQUIRE(model_view[1].get<int>() == 1);
  REQUIRE(model_view[2].get<double>() == 1.);
  REQUIRE(model_view[3].get<std::string>() == "null");
  REQUIRE(model_view[3].get<stdx::string_view>() == stdx::string_view(""));
  REQUIRE(model_view[3].get<int>() == 0);
  REQUIRE(model_view[3].get<double>() == 0.);
}

