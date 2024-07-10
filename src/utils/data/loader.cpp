#include <utils/data/loader.hpp>

// template <typename ContainerType, typename StoreType>
// void Loader::load(ContainerType &container, std::vector<std::string> files) {
//   for (auto &file : files) {
//     log.info("Loading {}", file);
//     std::ifstream ifs(file, std::ios::in | std::ios::binary);
//     cereal::BinaryInputArchive iarchive(ifs);
//     auto store_name = file.substr(file.find_last_of("/") + 1);
//     StoreType store =
//         StoreType(container.type, store_name, file, container.version);
//     iarchive(store);
//     container.add(store);
//   }
// }

void Loader::init(LibLog::Logger parentLog) {
  // log.setParent(&parentLog);
  log.setAsync(true);
}
