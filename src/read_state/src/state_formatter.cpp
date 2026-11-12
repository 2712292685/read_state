#include "read_state/state_formatter.hpp"

#include <array>
#include <cstddef>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace read_state
{
namespace
{

template <typename EnumT>
std::string unknown_value(const EnumT & value)
{
  return "未知(值=" + std::to_string(static_cast<int>(value)) + ")";
}

std::string number(float value, int precision, int width = 0)
{
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(precision);
  if (width > 0) {
    stream << std::setw(width);
  }
  stream << value;
  return stream.str();
}

constexpr std::array<const char *, 4> kLegLabels{
  "左前(LF)", "右前(RF)", "左后(LR)", "右后(RR)"};

}  // namespace

std::string model_name(zsibot::Model value)
{
  switch (value) {
    case zsibot::Model::MODEL_XG:
      return "小狗点足(MODEL_XG)";
    case zsibot::Model::MODEL_XGW:
      return "小狗轮足(MODEL_XGW)";
    case zsibot::Model::MODEL_XGWHSPD:
      return "小狗高速轮足(MODEL_XGWHSPD)";
  }
  return unknown_value(value);
}

std::string speed_level_name(zsibot::SpeedLevel value)
{
  switch (value) {
    case zsibot::SpeedLevel::SL_NULL:
      return "无效(SL_NULL)";
    case zsibot::SpeedLevel::SL_SLOW:
      return "慢速(SL_SLOW)";
    case zsibot::SpeedLevel::SL_NORMAL:
      return "正常速度(SL_NORMAL)";
    case zsibot::SpeedLevel::SL_FAST:
      return "快速(SL_FAST)";
  }
  return unknown_value(value);
}

std::string function_mode_name(zsibot::FunctionMode value)
{
  switch (value) {
    case zsibot::FunctionMode::FM_NULL:
      return "无效(FM_NULL)";
    case zsibot::FunctionMode::FM_REMOTE:
      return "遥控模式(FM_REMOTE)";
    case zsibot::FunctionMode::FM_TRACE:
      return "追踪模式(FM_TRACE)";
    case zsibot::FunctionMode::FM_SDK:
      return "SDK模式(FM_SDK)";
    case zsibot::FunctionMode::FM_GENERAL_SDK:
      return "通用SDK模式(FM_GENERAL_SDK)";
    case zsibot::FunctionMode::FM_ROAMER:
      return "漫游模式(FM_ROAMER)";
  }
  return unknown_value(value);
}

std::string control_mode_name(zsibot::ControlMode value)
{
  switch (value) {
    case zsibot::ControlMode::CM_NULL:
      return "无效(CM_NULL)";
    case zsibot::ControlMode::CM_STAND_UP:
      return "站立(CM_STAND_UP)";
    case zsibot::ControlMode::CM_SIT_DOWN:
      return "趴下(CM_SIT_DOWN)";
    case zsibot::ControlMode::CM_LOCK_MODE:
      return "锁定模式(CM_LOCK_MODE)";
    case zsibot::ControlMode::CM_EMERGENCY_STOP:
      return "紧急停止(CM_EMERGENCY_STOP)";
    case zsibot::ControlMode::CM_MOVE_MODE:
      return "移动模式(CM_MOVE_MODE)";
    case zsibot::ControlMode::CM_BALANCE_STAND_MODE:
      return "平衡站立模式(CM_BALANCE_STAND_MODE)";
    case zsibot::ControlMode::CM_INIT:
      return "初始化(CM_INIT)";
  }
  return unknown_value(value);
}

std::string motion_mode_name(zsibot::MotionMode value)
{
  switch (value) {
    case zsibot::MotionMode::MM_NULL:
      return "无效(MM_NULL)";
    case zsibot::MotionMode::MM_RUNNING:
      return "执行中(MM_RUNNING)";
    case zsibot::MotionMode::MM_REST:
      return "休息(MM_REST)";
    case zsibot::MotionMode::MM_FORBID:
      return "禁止(MM_FORBID)";
  }
  return unknown_value(value);
}

std::string motion_type_name(zsibot::MotionType value)
{
  switch (value) {
    case zsibot::MotionType::MT_NULL:
      return "无(MT_NULL)";
    case zsibot::MotionType::MT_JUMP:
      return "跳跃(MT_JUMP)";
    case zsibot::MotionType::MT_FORWARD_JUMP:
      return "前跳(MT_FORWARD_JUMP)";
    case zsibot::MotionType::MT_BACK_FLIP:
      return "后空翻(MT_BACK_FLIP)";
    case zsibot::MotionType::MT_GREET:
      return "招手(MT_GREET)";
    case zsibot::MotionType::MT_TWO_LEG_STAND:
      return "双腿站立(MT_TWO_LEG_STAND)";
    case zsibot::MotionType::MT_FORBID:
      return "禁止(MT_FORBID)";
    case zsibot::MotionType::MT_UNKNOWN:
      return "未知(MT_UNKNOWN)";
    case zsibot::MotionType::MT_UNLOADING_SQUAT:
      return "卸货下蹲(MT_UNLOADING_SQUAT)";
  }
  return unknown_value(value);
}

std::string format_snapshot(const zsibot::ZsibotExecutor & exec, const SnapshotContext & ctx)
{
  // 每个 getter 每次快照只调用一次，先缓存再格式化，尽量减少跨帧拼接。
  const zsibot::BatteryInfo battery = exec.GetBatteryInfo();
  const float body_temp = exec.GetTemperature();
  const std::string sn = exec.GetSn();
  const std::string dev_name = exec.GetDevName();
  const std::string wifi_ssid = exec.GetWifiSsid();
  const zsibot::VersionInfo version = exec.GetVersion();
  const zsibot::Model model = exec.GetModel();
  const zsibot::SpeedLevel speed_level = exec.GetSpeedLevel();
  const zsibot::FunctionMode function_mode = exec.GetFunctionMode();
  const zsibot::ControlMode control_mode = exec.GetControlMode();
  const zsibot::MotionMode motion_mode = exec.GetMotionMode();
  const zsibot::MotionType motion_type = exec.GetMotionType();
  const zsibot::SpeedInfo speed = exec.GetSpeed();
  const std::array<float, 4> quaternion = exec.GetQuaternion();
  const std::array<float, 3> rpy = exec.GetRPY();
  const std::array<float, 3> acc = exec.GetBodyAcc();
  const std::array<float, 3> gyro = exec.GetBodyGyro();
  const std::array<float, 3> position = exec.GetPosition();
  const std::array<float, 3> world_velocity = exec.GetWorldVelocity();
  const std::array<float, 3> body_velocity = exec.GetBodyVelocity();
  const std::array<float, 16> motor_temp = exec.GetMotorTemp();
  const std::array<float, 4> abad = exec.GetLegAbadJoint();
  const std::array<float, 4> hip = exec.GetLegHipJoint();
  const std::array<float, 4> knee = exec.GetLegKneeJoint();
  const std::array<float, 4> foot = exec.GetLegFootJoint();
  const std::array<float, 4> abad_vel = exec.GetLegAbadJointVel();
  const std::array<float, 4> hip_vel = exec.GetLegHipJointVel();
  const std::array<float, 4> knee_vel = exec.GetLegKneeJointVel();
  const std::array<float, 4> foot_vel = exec.GetLegFootJointVel();
  const std::array<float, 4> abad_torque = exec.GetLegAbadJointTorque();
  const std::array<float, 4> hip_torque = exec.GetLegHipJointTorque();
  const std::array<float, 4> knee_torque = exec.GetLegKneeJointTorque();
  const std::array<float, 4> foot_torque = exec.GetLegFootJointTorque();
  const std::vector<zsibot::FaultInfo> faults = exec.GetFaultInfo();

  const std::array<std::array<float, 4>, 4> angles{abad, hip, knee, foot};
  const std::array<std::array<float, 4>, 4> velocities{
    abad_vel, hip_vel, knee_vel, foot_vel};
  const std::array<std::array<float, 4>, 4> torques{
    abad_torque, hip_torque, knee_torque, foot_torque};

  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::fixed;

  out << "====================== 机器狗状态快照 ======================\n";
  out << "[连接] 已连接 | 目标 " << ctx.robot_ip << ":" << ctx.send_port
      << " | 接收端口 " << ctx.recv_port << " | 角色 " << ctx.role_label << "\n";
  out << "[设备] SN=" << sn << " | 名称=" << dev_name << " | 型号=" << model_name(model) << "\n";
  out << "[设备] WiFi=" << wifi_ssid << " | 运控版本=" << version.mc_version
      << " | 任务版本=" << version.dog_task_version << "\n";
  out << "[电池] 电量=" << battery.power << "% | 电压=" << number(battery.volt, 2)
      << " V | 电流=" << number(battery.current, 2) << " A | 温度="
      << number(battery.temp, 1) << " °C | 错误码=" << battery.error << "\n";
  out << "[温度] 机身温度=" << number(body_temp, 1) << " °C\n";
  out << "[模式] 速度等级=" << speed_level_name(speed_level)
      << " | 功能模式=" << function_mode_name(function_mode)
      << " | 控制模式=" << control_mode_name(control_mode) << "\n";
  out << "[模式] 运动模式=" << motion_mode_name(motion_mode)
      << " | 运动类型=" << motion_type_name(motion_type) << "\n";
  out << "[速度] 前后=" << number(speed.speed, 3) << " m/s | 左右="
      << number(speed.shift_speed, 3) << " m/s | 角速度="
      << number(speed.angle_speed, 3) << " rad/s | 头部角度="
      << number(speed.angle, 3) << " rad\n";
  out << "[IMU] 四元数 w=" << number(quaternion[0], 4) << " x="
      << number(quaternion[1], 4) << " y=" << number(quaternion[2], 4) << " z="
      << number(quaternion[3], 4) << "\n";
  out << "[IMU] 姿态 roll=" << number(rpy[0], 4) << " pitch=" << number(rpy[1], 4)
      << " yaw=" << number(rpy[2], 4) << " rad\n";
  out << "[IMU] 加速度 x=" << number(acc[0], 3) << " y=" << number(acc[1], 3)
      << " z=" << number(acc[2], 3) << " m/s² | 角速度 x=" << number(gyro[0], 3)
      << " y=" << number(gyro[1], 3) << " z=" << number(gyro[2], 3) << " rad/s\n";
  out << "[里程] 位置 x=" << number(position[0], 3) << " y=" << number(position[1], 3)
      << " z=" << number(position[2], 3) << " m\n";
  out << "[里程] 世界速度 x=" << number(world_velocity[0], 3) << " y="
      << number(world_velocity[1], 3) << " z=" << number(world_velocity[2], 3) << " m/s\n";
  out << "[里程] 机体速度 x=" << number(body_velocity[0], 3) << " y="
      << number(body_velocity[1], 3) << " z=" << number(body_velocity[2], 3) << " m/s\n";

  out << "[电机] 温度(°C): ";
  for (std::size_t i = 0; i < motor_temp.size(); ++i) {
    if (i > 0 && i % 8 == 0) {
      out << "\n[电机]           ";
    }
    out << "M" << i << "=" << number(motor_temp[i], 1) << " ";
  }
  out << "\n";

  out << "[关节] 每腿顺序 abad/hip/knee/foot；单位 角=rad 速=rad/s 矩=N·m；腿序 LF/RF/LR/RR\n";
  for (std::size_t leg = 0; leg < kLegLabels.size(); ++leg) {
    out << "[关节] " << kLegLabels[leg] << " 角["
        << number(angles[0][leg], 4, 8) << number(angles[1][leg], 4, 8)
        << number(angles[2][leg], 4, 8) << number(angles[3][leg], 4, 8) << "] 速["
        << number(velocities[0][leg], 4, 8) << number(velocities[1][leg], 4, 8)
        << number(velocities[2][leg], 4, 8) << number(velocities[3][leg], 4, 8)
        << "] 矩[" << number(torques[0][leg], 3, 8) << number(torques[1][leg], 3, 8)
        << number(torques[2][leg], 3, 8) << number(torques[3][leg], 3, 8) << "]\n";
  }

  if (faults.empty()) {
    out << "[故障] 无故障\n";
  } else {
    out << "[故障] 共 " << faults.size() << " 条\n";
    for (std::size_t i = 0; i < faults.size(); ++i) {
      const zsibot::FaultInfo & fault = faults[i];
      std::ostringstream code;
      code.imbue(std::locale::classic());
      code << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
           << fault.error_code;
      out << "[故障]  " << (i + 1) << ") 模块=" << fault.module
          << " 子模块=" << fault.submodule << " 错误码=" << code.str() << "("
          << fault.error_code << ")"
          << " 等级=" << fault.level << " 信息=" << fault.info << "\n";
    }
  }

  out << "==========================================================\n";
  return out.str();
}

}  // namespace read_state
