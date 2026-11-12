#ifndef READ_STATE__STATE_FORMATTER_HPP_
#define READ_STATE__STATE_FORMATTER_HPP_

#include <cstdint>
#include <string>

#include "zsibot_sdk/zsibot_api.h"

namespace read_state
{

struct SnapshotContext
{
  std::string robot_ip;
  uint32_t send_port{8081};
  uint32_t recv_port{8080};
  std::string role_label;
};

/// 读取一次完整状态并格式化为可直接打印的多行文本。
/// 调用前应确认 exec.IsConnected() 为 true。
std::string format_snapshot(const zsibot::ZsibotExecutor & exec, const SnapshotContext & ctx);

std::string model_name(zsibot::Model value);
std::string speed_level_name(zsibot::SpeedLevel value);
std::string function_mode_name(zsibot::FunctionMode value);
std::string control_mode_name(zsibot::ControlMode value);
std::string motion_mode_name(zsibot::MotionMode value);
std::string motion_type_name(zsibot::MotionType value);

}  // namespace read_state

#endif  // READ_STATE__STATE_FORMATTER_HPP_
