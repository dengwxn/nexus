#ifndef NEXUS_SCHEDULER_FRONTEND_DELEGATE_H_
#define NEXUS_SCHEDULER_FRONTEND_DELEGATE_H_

#include <grpc++/grpc++.h>

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "nexus/proto/control.grpc.pb.h"

namespace nexus {
namespace scheduler {

class Scheduler;

class FrontendDelegate {
 public:
  FrontendDelegate(uint32_t node_id, const std::string& ip,
                   const std::string& server_port, const std::string& rpc_addr,
                   int beacon_sec, bool is_dispatcher);

  uint32_t node_id() const { return node_id_; }

  std::time_t LastAliveTime();

  void Tick();

  bool IsAlive();

  void SubscribeModel(const std::string& model_session_id);

  const std::unordered_set<std::string>& subscribe_models() const {
    return subscribe_models_;
  }

  CtrlStatus UpdateModelRoutesRpc(const ModelRouteUpdates& request);

  bool IsDispatcher() const { return is_dispatcher_; }

 private:
  uint32_t node_id_;
  std::string ip_;
  std::string server_port_;
  std::string rpc_port_;
  int beacon_sec_;
  long timeout_ms_;
  std::unique_ptr<FrontendCtrl::Stub> stub_;
  std::chrono::time_point<std::chrono::system_clock> last_time_;
  std::unordered_set<std::string> subscribe_models_;
  bool is_dispatcher_;
};

}  // namespace scheduler
}  // namespace nexus

#endif  // NEXUS_SCHEDULER_FRONTEND_DELEGATE_H_
