#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "rcl_interfaces/msg/parameter_descriptor.hpp"
#include "rclcpp/rclcpp.hpp"
#include "read_state/state_formatter.hpp"
#include "zsibot_sdk/zsibot_api.h"
#include "zsibot_sdk/zsibot_exception.h"

namespace
{

constexpr int64_t kMinPort = 1;
constexpr int64_t kMaxPort = 65535;
constexpr double kMinPrintPeriodSec = 0.1;
constexpr double kMaxPrintPeriodSec = 60.0;
constexpr double kMinWarnPeriodSec = 0.5;
constexpr double kMaxWarnPeriodSec = 60.0;
constexpr auto kExecutorRetryDelay = std::chrono::seconds(5);

rcl_interfaces::msg::ParameterDescriptor make_descriptor(const std::string & description)
{
  rcl_interfaces::msg::ParameterDescriptor descriptor;
  descriptor.description = description;
  descriptor.read_only = true;
  return descriptor;
}

std::string to_lower(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

/// ZsibotExecutor 的构造函数在收到机器狗 model 信息前不会返回（内部每 10 ms 轮询一次），
/// 因此把它放到后台线程创建，避免阻塞 ROS 节点启动和 Ctrl-C。
struct ExecutorHolder
{
  std::mutex mutex;
  std::unique_ptr<zsibot::ZsibotExecutor> executor;
  bool init_in_progress{false};
  bool init_failed{false};
  bool stop_requested{false};
  std::string init_error;
  std::chrono::steady_clock::time_point next_retry{};
};

class ReadStateNode final : public rclcpp::Node
{
   public:
    ReadStateNode()
    : rclcpp::Node("read_state"), holder_(std::make_shared<ExecutorHolder>())
    {
        robot_ip_ = declare_parameter<std::string>(
            "robot_ip", "192.168.234.1",
            make_descriptor("机器狗 IP 地址（默认 192.168.234.1）"));
        send_port_ = declare_parameter<int64_t>(
            "send_port", 8081, make_descriptor("UDP 发送端口（默认 8081）"));
        recv_port_ = declare_parameter<int64_t>(
            "recv_port", 8080, make_descriptor("UDP 接收端口（默认 8080）"));
        role_ = declare_parameter<std::string>(
            "role", "remote", make_descriptor("心跳角色：remote 或 sdk（默认 remote）"));
        print_period_sec_ = declare_parameter<double>(
            "print_period_sec", 1.0,
            make_descriptor("状态打印周期（秒），范围 0.1~60.0"));
        warn_period_sec_ = declare_parameter<double>(
            "warn_period_sec", 5.0,
            make_descriptor("未连接警告限流周期（秒），范围 0.5~60.0"));

        validate_parameters();
        warn_period_ms_ = static_cast<int64_t>(warn_period_sec_ * 1000.0);

        RCLCPP_INFO(
            get_logger(),
            "启动配置: robot_ip=%s send_port=%lld recv_port=%lld role=%s "
            "打印周期=%.2f s 警告周期=%.2f s（仅打印，不发布话题）",
            robot_ip_.c_str(), static_cast<long long>(send_port_),
            static_cast<long long>(recv_port_), role_.c_str(), print_period_sec_,
            warn_period_sec_);

        // 首次尝试异步创建 executor；未连接时定时器会持续重试。
        ensure_executor();

        const auto period =
            std::chrono::milliseconds(static_cast<int64_t>(print_period_sec_ * 1000.0));
        timer_ = create_wall_timer(period, std::bind(&ReadStateNode::on_timer, this));
    }

    ~ReadStateNode() override
    {
        if (timer_) {
            timer_->cancel();
        }
        std::lock_guard<std::mutex> lock(holder_->mutex);
        holder_->stop_requested = true;
        // 若 executor 已创建，其析构会 join SDK 的发送/接收/心跳线程。
        holder_->executor.reset();
    }

   private:
    void validate_parameters() const
    {
        if (robot_ip_.empty() ||
            std::any_of(robot_ip_.begin(), robot_ip_.end(), [](unsigned char c) {
                return std::isspace(c) != 0;
            }))
        {
            throw std::invalid_argument(
                "参数 robot_ip 不能为空或包含空白字符，当前值: '" + robot_ip_ + "'");
        }
        if (send_port_ < kMinPort || send_port_ > kMaxPort) {
            throw std::invalid_argument(
                "参数 send_port 必须在 1~65535 之间，当前值: " + std::to_string(send_port_));
        }
        if (recv_port_ < kMinPort || recv_port_ > kMaxPort) {
            throw std::invalid_argument(
                "参数 recv_port 必须在 1~65535 之间，当前值: " + std::to_string(recv_port_));
        }
        if (send_port_ == recv_port_) {
            throw std::invalid_argument(
                "参数 send_port 和 recv_port 不能相同，当前均为: " +
                std::to_string(send_port_));
        }
        const std::string role = to_lower(role_);
        if (role != "remote" && role != "sdk") {
            throw std::invalid_argument(
                "参数 role 只能是 remote 或 sdk，当前值: '" + role_ + "'");
        }
        if (print_period_sec_ < kMinPrintPeriodSec ||
            print_period_sec_ > kMaxPrintPeriodSec)
        {
            throw std::invalid_argument(
                "参数 print_period_sec 必须在 0.1~60.0 之间，当前值: " +
                std::to_string(print_period_sec_));
        }
        if (warn_period_sec_ < kMinWarnPeriodSec ||
            warn_period_sec_ > kMaxWarnPeriodSec)
        {
            throw std::invalid_argument(
                "参数 warn_period_sec 必须在 0.5~60.0 之间，当前值: " +
                std::to_string(warn_period_sec_));
        }
    }

    zsibot::Role parse_role() const
    {
        return to_lower(role_) == "sdk" ? zsibot::Role::ROLE_SDK
                                        : zsibot::Role::ROLE_REMOTE;
    }

    std::string role_label() const
    {
        return to_lower(role_) == "sdk" ? "sdk (ROLE_SDK)" : "remote (ROLE_REMOTE)";
    }

    read_state::SnapshotContext make_context() const
    {
        return read_state::SnapshotContext{
            robot_ip_, static_cast<uint32_t>(send_port_),
            static_cast<uint32_t>(recv_port_), role_label()};
    }

    bool ensure_executor()
    {
        bool start_init = false;
        {
            std::lock_guard<std::mutex> lock(holder_->mutex);
            if (holder_->executor) {
                if (!executor_ready_logged_) {
                    executor_ready_logged_ = true;
                    RCLCPP_INFO(get_logger(), "ZsibotExecutor 初始化成功");
                }
                return true;
            }
            if (holder_->init_failed) {
                holder_->init_failed = false;
                RCLCPP_ERROR_THROTTLE(
                    get_logger(), *get_clock(), warn_period_ms_,
                    "创建 ZsibotExecutor 失败: %s（约 5 秒后重试）",
                    holder_->init_error.c_str());
                return false;
            }
            if (holder_->init_in_progress) {
                RCLCPP_WARN_THROTTLE(
                    get_logger(), *get_clock(), warn_period_ms_,
                    "正在等待机器狗连接（SDK 初始化中）...");
                return false;
            }
            if (std::chrono::steady_clock::now() < holder_->next_retry) {
                return false;
            }
            holder_->init_in_progress = true;
            start_init = true;
        }

        if (!start_init) {
            return false;
        }

        RCLCPP_INFO(
            get_logger(),
            "正在后台创建 ZsibotExecutor（等待机器狗 model 信息，SDK 会打印 "
            "\"model is empty, please wait..\"）");

        auto holder = holder_;
        const zsibot::Role role = parse_role();
        const std::string ip = robot_ip_;
        const uint32_t send_port = static_cast<uint32_t>(send_port_);
        const uint32_t recv_port = static_cast<uint32_t>(recv_port_);

        try {
            std::thread([holder, role, ip, send_port, recv_port]() {
                try {
                    auto exec = std::make_unique<zsibot::ZsibotExecutor>(
                        role, ip, send_port, recv_port);
                    std::lock_guard<std::mutex> lock(holder->mutex);
                    if (holder->stop_requested) {
                        // 节点已退出：exec 在此线程内析构并 join SDK 线程。
                        return;
                    }
                    holder->executor = std::move(exec);
                    holder->init_in_progress = false;
                } catch (const std::exception & exception) {
                    std::lock_guard<std::mutex> lock(holder->mutex);
                    holder->init_error = exception.what();
                    holder->init_failed = true;
                    holder->init_in_progress = false;
                    holder->next_retry =
                        std::chrono::steady_clock::now() + kExecutorRetryDelay;
                }
            }).detach();
        } catch (const std::exception & exception) {
            std::lock_guard<std::mutex> lock(holder_->mutex);
            holder_->init_in_progress = false;
            holder_->init_error = exception.what();
            holder_->init_failed = true;
            holder_->next_retry = std::chrono::steady_clock::now() + kExecutorRetryDelay;
            RCLCPP_ERROR(get_logger(), "创建初始化线程失败: %s", exception.what());
        }
        return false;
    }

    void on_timer()
    {
        if (!ensure_executor()) {
            return;
        }

        zsibot::ZsibotExecutor * exec = nullptr;
        {
            std::lock_guard<std::mutex> lock(holder_->mutex);
            exec = holder_->executor.get();
        }
        if (exec == nullptr) {
            return;
        }

        bool connected = false;
        try {
            connected = exec->IsConnected();
        } catch (const std::exception & exception) {
            RCLCPP_ERROR_THROTTLE(
                get_logger(), *get_clock(), warn_period_ms_,
                "读取连接状态失败: %s", exception.what());
            return;
        }

        if (!connected) {
            if (was_connected_) {
                RCLCPP_WARN(get_logger(), "与机器狗的连接已断开");
                was_connected_ = false;
            }
            RCLCPP_WARN_THROTTLE(
                get_logger(), *get_clock(), warn_period_ms_,
                "未连接到机器狗 (目标 %s:%lld, 接收端口 %lld, 角色 %s)，等待数据...",
                robot_ip_.c_str(), static_cast<long long>(send_port_),
                static_cast<long long>(recv_port_), role_.c_str());
            return;
        }

        if (!was_connected_) {
            RCLCPP_INFO(get_logger(), "已连接到机器狗");
            was_connected_ = true;
        }

        try {
            const std::string block =
                read_state::format_snapshot(*exec, make_context());
            // 快照内容包含 '%'（电量百分比），必须作为参数传入而非格式串。
            RCLCPP_INFO(get_logger(), "\n%s", block.c_str());
        } catch (const zsibot::ZsibotException & exception) {
            RCLCPP_ERROR_THROTTLE(
                get_logger(), *get_clock(), warn_period_ms_,
                "读取机器狗状态失败: %s", exception.what());
        } catch (const std::exception & exception) {
            RCLCPP_ERROR_THROTTLE(
                get_logger(), *get_clock(), warn_period_ms_,
                "读取机器狗状态失败: %s", exception.what());
        }
    }

    std::shared_ptr<ExecutorHolder> holder_;
    rclcpp::TimerBase::SharedPtr timer_;

    std::string robot_ip_{"192.168.234.1"};
    int64_t send_port_{8081};
    int64_t recv_port_{8080};
    std::string role_{"remote"};
    double print_period_sec_{1.0};
    double warn_period_sec_{5.0};
    int64_t warn_period_ms_{5000};
    bool executor_ready_logged_{false};

    bool was_connected_{false};
};

}  // namespace

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);
    try {
        auto node = std::make_shared<ReadStateNode>();
        rclcpp::spin(node);
    } catch (const std::exception & exception) {
        RCLCPP_FATAL(
            rclcpp::get_logger("read_state"), "节点启动失败: %s", exception.what());
        rclcpp::shutdown();
        return 1;
    }
    rclcpp::shutdown();
    return 0;
}
