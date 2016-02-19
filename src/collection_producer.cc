#include "collection_producer.h"


namespace mongo_smasher {

CollectionProducer::CollectionProducer(
    Queue<std::vector<bsoncxx::document::view>>& queue,
    bsoncxx::document::view model,
    bsoncxx::stdx::string_view root_path) :
  queue_{queue}, randomizer(model["values"].get_document().view(),root_path)
{}

void CollectionProducer::run() {
  //std::vector<ProcessingUnit> units;
  //// ProcessingUnit unit{randomizer, db_uri,
  //// view["collections"].get_document().view()};
  //for (auto collection_view : view["collections"].get_document().view()) {
    //log(log_level::debug, "Registering %s\n", collection_view.key().data());
    //units.emplace_back(randomizer, collections, collection_view.key(),
                       //collection_view.get_document().view());
  //}
}

}  // namespace mongo_smasher
