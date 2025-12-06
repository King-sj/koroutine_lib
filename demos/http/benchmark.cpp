#include <atomic>
#include <chrono>
#include <iostream>
#include <vector>

#include "koroutine/async_io/httplib.h"
#include "koroutine/koroutine.h"

using namespace koroutine;
using namespace httplib;

std::atomic<int> success_count{0};
std::atomic<int> fail_count{0};

Task<void> worker(int id, int requests_per_worker) {
  Client cli("http://localhost:8080");
  // Keep-alive is enabled by default in Client if server supports it

  for (int i = 0; i < requests_per_worker; ++i) {
    auto res = co_await cli.Get("/hi");
    if (res && res->status == 200) {
      success_count++;
    } else {
      fail_count++;
    }
  }
}

Task<void> run_benchmark(int concurrency, int total_requests) {
  auto start = std::chrono::high_resolution_clock::now();

  int requests_per_worker = total_requests / concurrency;
  std::vector<Task<void>> tasks;
  tasks.reserve(concurrency);

  for (int i = 0; i < concurrency; ++i) {
    tasks.push_back(worker(i, requests_per_worker));
  }

  // Wait for all workers
  // We don't have a `when_all(vector)` helper yet in standard koroutine
  // examples, but we can use a simple loop or TaskManager if available. Let's
  // assume we can just await them sequentially for simplicity of code, but that
  // would serialize them if they are not spawned. To run concurrently, we
  // should spawn them or use `when_all` with variadic args if concurrency is
  // small. For dynamic concurrency, we need a dynamic `when_all`. Or we can use
  // TaskManager.

  koroutine::TaskManager tm;
  for (auto& t : tasks) {
    // We need to make shared_ptr<Task<void>> for TaskManager
    // But Task is movable, not copyable. TaskManager takes shared_ptr.
    // Let's wrap it.
    auto shared_task = std::make_shared<Task<void>>(std::move(t));
    tm.submit_to_group("bench", shared_task);
  }

  co_await tm.join_group("bench");

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count();

  std::cout << "Benchmark finished in " << duration << " ms" << std::endl;
  std::cout << "Total requests: " << total_requests << std::endl;
  std::cout << "Concurrency: " << concurrency << std::endl;
  std::cout << "Success: " << success_count << std::endl;
  std::cout << "Failed: " << fail_count << std::endl;
  std::cout << "QPS: " << (total_requests * 1000.0 / duration) << std::endl;
}

int main(int argc, char** argv) {
  int concurrency = 100;
  int total_requests = 10000;

  if (argc > 1) concurrency = std::atoi(argv[1]);
  if (argc > 2) total_requests = std::atoi(argv[2]);

  std::cout << "Starting benchmark with " << concurrency
            << " concurrent workers, " << total_requests << " total requests..."
            << std::endl;

  Runtime::block_on(run_benchmark(concurrency, total_requests));
  return 0;
}
