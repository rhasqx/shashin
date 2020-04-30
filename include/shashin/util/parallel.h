#pragma once

#include <thread>
#include <functional>

namespace shashin {
namespace util {

auto process_parallel(std::function<void(int worker_number, int lower_bound, int upper_bound)> process_func, int list_size, int worker_size = int(std::thread::hardware_concurrency())) -> void;

} // namespace util
} // namespace shashin
