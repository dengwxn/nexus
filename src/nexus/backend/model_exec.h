#ifndef NEXUS_BACKEND_MODEL_EXEC_H_
#define NEXUS_BACKEND_MODEL_EXEC_H_

#include <atomic>
#include <memory>
#include <mutex>

#include "nexus/backend/model_ins.h"
#include "nexus/common/block_queue.h"
#include "nexus/common/metric.h"
#include "nexus/common/model_db.h"

namespace nexus {
namespace backend {

class ModelWorker {
 public:
  ModelWorker(ModelInstance* model, BlockQueue<Task>& in_queue, BlockQueue<Task>& out_queue) :
      model_(model), input_queue_(in_queue), output_queue_(out_queue),
      running_(false) {}

  void Start() {
    running_ = true;
    thread_ = std::thread(&ModelWorker::Run, this);
  }

  void Stop() {
    running_ = false;
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void Run() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto timeout = std::chrono::milliseconds(50);
    while (running_) {
      std::shared_ptr<Task> task = input_queue_.pop(timeout);
      if (task == nullptr) {
        continue;
      }
      model_->Preprocess(task);
      output_queue_.push(task);
    }
  }

 private:
  ModelInstance* model_;
  BlockQueue<Task>& input_queue_;
  BlockQueue<Task>& output_queue_;
  std::thread thread_;
  std::atomic<bool> running_;
};

class ModelExecutor {
 public:
  ModelExecutor(int gpu_id, const ModelInstanceConfig& config,
                BlockPriorityQueue<Task>& task_queue);

  ~ModelExecutor();

  ModelInstance* model() { return model_.get(); }

  const ModelInstance* model() const { return model_.get(); }
  /*! \brief Return whether this model is a backup model. */
  bool backup() const { return backup_; }

  const ModelProfile* profile() const { return profile_; }

  void SetBatch(uint32_t batch) { model_->set_batch(batch); }

  double GetRequestRate();

  double GetDropRate();

  bool IsSharePrefixModel() const;
  bool IsTFShareModel() const;

  bool HasBackup();

  std::vector<uint32_t> BackupBackends();

  void UpdateBackupBackends(const ModelInstanceConfig& config);

  void Enqueue(std::shared_ptr<Task> task);

  bool Preprocess(std::shared_ptr<Task> task, bool force=false);

  bool AddPreprocessedTask(std::shared_ptr<Task> task, bool force=false);

  void Postprocess(std::shared_ptr<Task> task);

  uint64_t Execute(uint32_t batch = 0);

  TimePoint LastExecuteFinishTime();

  int NumberOfOpenRequests() const;

 private:
  std::pair<std::shared_ptr<BatchTask>, int> GetBatchTaskSlidingWindow(uint32_t batch_size);
  std::pair<std::shared_ptr<BatchTask>, int> GetBatchTaskEarliest(uint32_t batch_size);

  bool IncreaseOpenRequests(int cnt, bool limit_max_batch);

  void DecreaseOpenRequests(int cnt);
  /*!
   * \brief Get batch task from the task queue.
   * \param batch_size Expected batch size in the batch task.
   * \return Batch task and the number of inputs dequeued from input queue.
   */
  std::pair<std::shared_ptr<BatchTask>, int> GetBatchTask(uint32_t batch_size);

  void RemoveTask(std::shared_ptr<Task> task);

  std::unique_ptr<ModelInstance> model_;
  bool backup_;
  const ModelProfile* profile_;
  BlockPriorityQueue<Task>& global_task_queue_;
  /*!
   * \brief Map from task id to current processing tasks.
   * Guarded by task_mu_.
   */
  std::unordered_map<uint64_t, std::shared_ptr<Task> > processing_tasks_;
  /*! \brief Priority queue of inputs based on deadline. Guarded by task_mu_. */
  std::priority_queue<std::shared_ptr<Task>,
                      std::vector<std::shared_ptr<Task> >,
                      CompareDeadlineItem> task_queue_;
  /*! \brief Input array allocated in GPU memory to hold batch inputs. */
  std::shared_ptr<Array> input_array_;
  /*! \brief Batch index. */
  std::atomic<uint64_t> batch_id_;
  /*! \brief Number of open requests. */
  std::atomic<int> open_requests_;
  /*! \brief Interval counter to count number of requests within each interval.
   */
  std::shared_ptr<IntervalCounter> req_counter_;
  std::shared_ptr<IntervalCounter> drop_counter_;

  EWMA req_rate_;
  EWMA drop_rate_;

  std::vector<uint32_t> backup_backends_;
  /*!
   * \brief Last time point that finishes the batch execution.
   * Guarded by time_mu_.
   */
  TimePoint last_exec_finish_;
  /*! \brief Mutex to proect processing_tasks_ and input_queue_. */
  std::mutex task_mu_;
  /*! \brief Mutex to proect last_exec_finish_. */
  std::mutex time_mu_;

  std::mutex backup_mu_;

  int num_workers_;
  BlockQueue<Task> worker_in_queue_;
  BlockQueue<Task> worker_out_queue_;
  std::vector<std::shared_ptr<ModelWorker>> workers_;
};

using ModelExecutorPtr = std::shared_ptr<ModelExecutor>;

} // namespace backend
} // namespace nexus

#endif // NEXUS_BACKEND_MODEL_EXEC_H_
