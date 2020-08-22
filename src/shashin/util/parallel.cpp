#include <shashin/util/parallel.h>
#include <vector>

namespace shashin {
namespace util {

auto process_parallel(std::function<void(int worker_number, int lower_bound, int upper_bound)> process_func, int list_size, int worker_size) -> void {
    auto const chunk_size{list_size / worker_size};
    std::vector<std::thread> workers;
    for (auto worker_number{0}; worker_number < worker_size; ++worker_number) {
        auto const lower_bound{chunk_size * worker_number};
        auto const upper_bound{(worker_number < worker_size - 1) ? (chunk_size * worker_number + (chunk_size > 0 ? chunk_size : 0)) : list_size};
        if (lower_bound == 0 && upper_bound == 0) {
            continue;
        }
        workers.push_back(std::thread(process_func, worker_number, lower_bound, upper_bound));
    }
    for (auto& worker : workers) {
        worker.join();
    }
}

} // namespace util
} // namespace shashin
