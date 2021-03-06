#ifndef MLPERF_LOADGEN_LOGGING_H_
#define MLPERF_LOADGEN_LOGGING_H_

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <future>
#include <list>
#include <ostream>
#include <mutex>
#include <thread>
#include <set>
#include <vector>
#include <unordered_set>

namespace mlperf {

class AsyncLog;
class Logger;
class TlsLogger;

using AsyncLogEntry = std::function<void(AsyncLog&)>;
using PerfClock = std::chrono::high_resolution_clock;

// AsyncTrace is passed as an argument to the custom log lambda on the
// recording thread to serialize the data captured by the lambda and
// forward it to the output stream.
class AsyncLog {
 public:
  AsyncLog(std::ostream *out_stream) : out_stream_(*out_stream) {}

  template <typename ...Args>
  void FullEvent(const std::string& trace_name, uint64_t ts, uint64_t dur,
                 const Args... args) {
    out_stream_ << "{ \"name\": \"" << trace_name << "\", ";
    out_stream_ << "\"ph\": \"X\", ";
    out_stream_ << "\"pid\": 0, ";  // TODO
    out_stream_ << "\"tid\": 0, ";  // TODO
    out_stream_ << "\"ts\": " << ts << ", ";
    out_stream_ << "\"dur\": " << dur << ", ";
    out_stream_ << "\"args\": { ";
    LogArgs(args...);
    out_stream_ << " }},\n";
    out_stream_.flush();
  }

  template <typename ...Args>
  void AsyncEvent(const std::string& trace_name, uint64_t id,
                  uint64_t ts, uint64_t dur,
                  const Args... args) {
    out_stream_ << "{\"name\": \"" << trace_name << "\", ";
    out_stream_ << "\"cat\": \"default\", ";
    out_stream_ << "\"ph\": \"b\", ";
    out_stream_ << "\"id\": " << id << ", ";
    out_stream_ << "\"pid\": 0, ";  // TODO
    out_stream_ << "\"tid\": 0, ";  // TODO

    //out_stream_ << "\"ts\": " << ts << " },\n";
    out_stream_ << "\"ts\": " << ts << ", ";
    out_stream_ << "\"args\": { ";
    LogArgs(args...);
    out_stream_ << " }},\n";

    out_stream_ << "{ \"name\": \"" << trace_name << "\", ";
    out_stream_ << "\"cat\": \"default\", ";
    out_stream_ << "\"ph\": \"e\", ";
    out_stream_ << "\"id\": " << id << ", ";
    out_stream_ << "\"pid\": 0, ";  // TODO
    out_stream_ << "\"tid\": 0, ";  // TODO
    out_stream_ << "\"ts\": " << ts + dur << " },\n";

    out_stream_.flush();

    // The trace duration currently corresponds to response latency.
    // Trace id corresponds to the sample sequence id.
    std::unique_lock<std::mutex> lock(latencies_mutex_);
    if (latencies_.size() < id + 1) {
      latencies_.resize(id + 1, std::chrono::nanoseconds(0));
    }
    latencies_[id] = std::chrono::nanoseconds(dur);
    latencies_recorded_++;
    if (AllLatenciesRecorded()) {
      all_latencies_recorded_.notify_all();
    }
  }

  void RestartLatencyRecording() {
    std::unique_lock<std::mutex> lock(latencies_mutex_);
    assert(latencies_.empty());
    assert(latencies_recorded_ == latencies_expected_);
    latencies_recorded_ = 0;
    latencies_expected_ = 0;
  }

  std::vector<std::chrono::nanoseconds> GetLatenciesBlocking(
          size_t expected_count) {
    std::vector<std::chrono::nanoseconds> latencies;
    std::unique_lock<std::mutex> lock(latencies_mutex_);
    latencies_expected_ = expected_count;
    all_latencies_recorded_.wait(lock, [&]{ return AllLatenciesRecorded(); });
    latencies.swap(latencies_);
    return latencies;
  }

 private:
  void LogArgs() {}

  template <typename T>
  void LogArgs(const std::string& arg_name, const T &arg_value) {
    out_stream_ << "\"" << arg_name << "\" : " << arg_value;
  }

  template <typename T, typename ...Args>
  void LogArgs(const std::string& arg_name, const T& arg_value,
               const Args... args) {
    out_stream_ << "\"" << arg_name << "\" : " << arg_value << ", ";
    LogArgs(args...);
  }

  std::ostream &out_stream_;  

  std::mutex latencies_mutex_;
  std::condition_variable all_latencies_recorded_;
  std::vector<std::chrono::nanoseconds> latencies_;
  size_t latencies_recorded_ = 0;
  size_t latencies_expected_ = 0;
  bool AllLatenciesRecorded() {
    return latencies_recorded_ == latencies_expected_;
  }
};

// Logs all threads belonging to a run.
class Logger {
 public:
  Logger(std::ostream *out_stream,
         std::chrono::duration<double> poll_period,
         size_t max_threads_to_log);
  ~Logger();

  void RequestSwapBuffers(TlsLogger* tls_logger);

  PerfClock::time_point origin_time() {
    return origin_time_;
  }

  void RegisterTlsLogger(TlsLogger* tls_logger);
  void UnRegisterTlsLogger(TlsLogger* tls_logger);

  void RestartLatencyRecording();
  std::vector<std::chrono::nanoseconds> GetLatenciesBlocking(
      size_t expected_count);

 private:
  TlsLogger* GetTlsLoggerThatRequestedSwap(size_t slot, size_t next_id);
  void GatherRetrySwapRequests(std::vector<TlsLogger*>* threads_to_swap);
  void GatherNewSwapRequests(std::vector<TlsLogger*>* threads_to_swap);

  // The main logging thread function that handles the serialization
  // and I/O to the stream or file.
  void IOThread();

  std::ostream &out_stream_;
  const size_t max_threads_to_log_;
  std::thread io_thread_;
  const PerfClock::time_point origin_time_;

  // Accessed by IOThead only.
  const std::chrono::duration<double> poll_period_;
  AsyncLog async_logger_;

  // Accessed by producers and IOThead during thread registration and
  // destruction. Protected by io_thread_mutex_.
  std::mutex io_thread_mutex_;
  std::condition_variable io_thread_cv_;
  bool keep_io_thread_alive_ = true;

  std::mutex tls_loggers_registerd_mutex_;
  std::unordered_set<TlsLogger*> tls_loggers_registerd_;

  // Accessed by producers and IOThead atomically.
  std::atomic<size_t> swap_request_id_ { 0 };
  std::vector<std::atomic<uintptr_t>> thread_swap_request_slots_;

  // Accessed by IOThead only.
  size_t swap_request_id_read_ { 0 };
  struct SlotRetry { size_t slot; uintptr_t next_id; };
  std::vector<SlotRetry> swap_request_slots_to_retry_;
  std::vector<TlsLogger*> threads_to_swap_deferred_;
  std::vector<TlsLogger*> threads_to_read_;
  std::vector<std::function<void()>> thread_cleanup_tasks_;
};

void Log(Logger *logger, AsyncLogEntry &&entry);

}  // namespace mlperf

#endif  // MLPERF_LOADGEN_LOGGING_H_
