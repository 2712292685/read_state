# read_state — ROS2 机器狗状态打印节点

基于 **Zsibot Genisom L1 SDK**（`zsibot-sdk/genisom_L1_sdk`）的 ROS2 Humble 节点。节点周期性读取机器狗的全部状态，并以中文标签 + 单位打印到终端。

**仅打印，不发布任何 ROS 话题/服务/动作。**

## 功能

- 使用 SDK 的 `zsibot::ZsibotExecutor` 通过 UDP 与机器狗通信。
- 默认 1 Hz 打印完整状态快照：
  - 连接状态、目标 IP/端口、角色
  - 设备 SN、名称、型号、WiFi SSID、运控/任务版本
  - 电池电量、电压、电流、温度、错误码；机身温度
  - 速度等级、功能模式、控制模式、运动模式、运动类型（中文 + 枚举名）
  - 前后/左右/角速度/头部角度
  - IMU 四元数、RPY、加速度、角速度
  - 里程计位置、世界速度、机体速度
  - 全部 16 个电机温度
  - 4 条腿 × 4 个关节（abad/hip/knee/foot）的角度、角速度、力矩
  - 故障列表
- 未连接时只打印限流警告，不打印全零快照；连接后自动开始打印。
- 无机器人时节点也能正常启动，Ctrl-C 干净退出。

> **实现说明**：`ZsibotExecutor` 的构造函数会阻塞到收到机器狗的 model 信息为止（内部每 10 ms 轮询一次）。因此节点在**后台线程**中创建 executor，避免阻塞 ROS 初始化和 Ctrl-C。等待连接期间 SDK 自身会周期性向 stdout 打印 `model is empty, please wait..`，这是 SDK 的正常提示；节点同时会按 `warn_period_sec` 限流打印等待警告。

## 目录结构

```
read_state/                      # colcon 工作空间
├── README.md
├── .gitignore
└── src/
    └── read_state/              # ament_cmake 包
        ├── package.xml
        ├── CMakeLists.txt
        ├── include/read_state/
        │   └── state_formatter.hpp
        ├── src/
        │   ├── read_state_node.cpp
        │   └── state_formatter.cpp
        └── launch/
            └── read_state.launch.py
```

构建后会自动生成 `build/`、`install/`、`log/`（已在 `.gitignore` 中忽略）。

## 依赖

- ROS2 Humble（`/opt/ros/humble`）
- colcon、CMake ≥ 3.22、GCC ≥ 11（支持 C++20）
- Zsibot SDK：`../zsibot-sdk/genisom_L1_sdk`（相对本工作空间根目录），包含头文件和 `lib/x86_64|aarch64/libzsibot.a`

## 构建

```bash
source /opt/ros/humble/setup.bash
cd /home/snail/project/Development-course/read_state
colcon build --packages-select read_state --event-handlers console_direct+
source install/setup.bash
```

构建输出应包含 SDK 库和头文件路径：

```
-- read_state: zsibot_sdk lib = .../zsibot-sdk/genisom_L1_sdk/lib/x86_64/libzsibot.a
-- read_state: zsibot_sdk include = .../zsibot-sdk/genisom_L1_sdk/include
```

SDK 路径或架构不匹配时，可在构建时覆盖（修改后建议删除 `build/ install/ log/` 再构建）：

```bash
colcon build --packages-select read_state --cmake-args \
  -DZSIBOT_SDK_DIR=/绝对路径/genisom_L1_sdk \
  -DZSIBOT_ARCH=x86_64
```

## 运行

```bash
source /opt/ros/humble/setup.bash
source /home/snail/project/Development-course/read_state/install/setup.bash
ros2 run read_state read_state_node
```

使用 launch 文件（参数同名）：

```bash
ros2 launch read_state read_state.launch.py
ros2 launch read_state read_state.launch.py robot_ip:=192.168.234.1 role:=remote print_period_sec:=0.5
```

直接指定参数：

```bash
ros2 run read_state read_state_node --ros-args \
  -p robot_ip:=192.168.234.1 \
  -p print_period_sec:=1.0 \
  -p warn_period_sec:=5.0
```

## 参数

所有参数均为只读（启动时设置，运行中不可修改）。

| 参数 | 类型 | 默认值 | 说明 / 范围 |
|---|---|---|---|
| `robot_ip` | string | `192.168.234.1` | 机器狗 IP，非空且不含空白 |
| `send_port` | int | `8081` | UDP 发送端口，1~65535 |
| `recv_port` | int | `8080` | UDP 接收端口，1~65535，且不能等于发送端口 |
| `role` | string | `remote` | 心跳角色：`remote` 或 `sdk`（不区分大小写） |
| `print_period_sec` | double | `1.0` | 状态打印周期，0.1~60.0 秒 |
| `warn_period_sec` | double | `5.0` | 未连接警告限流周期，0.5~60.0 秒 |

参数非法时节点会立即报错退出（例如 `role:=invalid`、`send_port:=70000`）。

### 角色说明

- `remote`（默认）：与 SDK 默认值和 `example/remote_control.cpp` 一致，只发送心跳，适合纯状态监控。
- `sdk`：心跳角色为 SDK。若需要后续通过 SDK 控制机器狗，可使用该角色；仅读取状态时两者行为没有区别。

## 网络连接

机器狗默认地址为 `192.168.234.1`，UDP 发送端口 `8081`、接收端口 `8080`。运行节点的主机必须能访问 `192.168.234.0/24` 网段：

```bash
ip -br addr
# 若网卡不在该网段，可临时添加地址（示例，按实际网卡名修改）：
sudo ip addr add 192.168.234.100/24 dev eth0
ping 192.168.234.1
```

WSL2 注意事项：

- 推荐 Windows 11 **mirrored 网络模式**（`.wslconfig` 中 `[wsl2]` 下设置 `networkingMode=mirrored`，然后 `wsl --shutdown`）。
- 默认 NAT 模式下，节点发出的 UDP 可以到达机器狗，但机器狗返回 8080 端口的 UDP 包通常无法进入 WSL2；`netsh interface portproxy` 只支持 TCP，不能解决。
- 备选：使用桥接/NAT 端口映射之外的原生 Ubuntu、在 Windows 主机上运行，或使用 mirrored 模式。
- 若使用 Windows 防火墙，请允许 UDP 8080。

## 输出示例

```
====================== 机器狗状态快照 ======================
[连接] 已连接 | 目标 192.168.234.1:8081 | 接收端口 8080 | 角色 remote (ROLE_REMOTE)
[设备] SN=... | 名称=... | 型号=小狗点足(MODEL_XG)
[设备] WiFi=... | 运控版本=... | 任务版本=...
[电池] 电量=86% | 电压=24.10 V | 电流=1.23 A | 温度=30.5 °C | 错误码=0
[温度] 机身温度=36.0 °C
[模式] 速度等级=正常速度(SL_NORMAL) | 功能模式=遥控模式(FM_REMOTE) | 控制模式=移动模式(CM_MOVE_MODE)
[模式] 运动模式=休息(MM_REST) | 运动类型=无(MT_NULL)
[速度] 前后=0.000 m/s | 左右=0.000 m/s | 角速度=0.000 rad/s | 头部角度=0.000 rad
[IMU] 四元数 w=1.0000 x=0.0000 y=0.0000 z=0.0000
[IMU] 姿态 roll=0.0000 pitch=0.0000 yaw=0.0000 rad
[IMU] 加速度 x=0.000 y=0.000 z=9.810 m/s² | 角速度 x=0.000 y=0.000 z=0.000 rad/s
[里程] 位置 x=0.000 y=0.000 z=0.000 m
[里程] 世界速度 x=0.000 y=0.000 z=0.000 m/s
[里程] 机体速度 x=0.000 y=0.000 z=0.000 m/s
[电机] 温度(°C): M0=30.0 M1=31.0 ... M7=30.0
[电机]           M8=30.0 M9=31.0 ... M15=30.0
[关节] 每腿顺序 abad/hip/knee/foot；单位 角=rad 速=rad/s 矩=N·m；腿序 LF/RF/LR/RR
[关节] 左前(LF) 角[ ... ] 速[ ... ] 矩[ ... ]
[关节] 右前(RF) 角[ ... ] 速[ ... ] 矩[ ... ]
[关节] 左后(LR) 角[ ... ] 速[ ... ] 矩[ ... ]
[关节] 右后(RR) 角[ ... ] 速[ ... ] 矩[ ... ]
[故障] 无故障
==========================================================
```

## 故障排查

| 现象 | 处理 |
|---|---|
| 持续打印 `未连接到机器狗` | 检查主机是否在 `192.168.234.0/24` 网段、`ping 192.168.234.1`、机器狗是否上电、防火墙是否拦截 UDP 8080；WSL2 请参考上面的网络说明 |
| 出现 `model is empty, please wait..` | SDK 正在等待机器狗的 model 信息，说明尚未建立有效通信；检查网络/机器狗电源。节点仍可正常响应 Ctrl-C |
| `创建 ZsibotExecutor 失败`（bind 错误） | UDP 8080 已被占用。用 `ss -lunp \| grep ':8080'` 查找；同一时间只能运行一个 SDK 消费者（不要与 SDK 示例程序同时运行）。节点每 5 秒自动重试 |
| 构建报 `未找到 Zsibot 静态库` | 检查 `ZSIBOT_SDK_DIR` / `ZSIBOT_ARCH`，或按上面的命令覆盖；切换后删除 `build/` |
| 链接时报架构不匹配 | 确认 `ZSIBOT_ARCH` 与本机一致（x86_64 / aarch64） |
| `ros2 run` 找不到包 | 先 `source install/setup.bash` |
| `ros2 launch` 找不到 launch 文件 | 重新构建；确认 `install/read_state/share/read_state/launch/read_state.launch.py` 存在 |
| 终端中文或 `m/s²` 乱码 | 使用 UTF-8 locale（如 `LANG=C.UTF-8`）的终端 |
| 连接前的数值为 0 | 正常现象：节点用 `IsConnected()` 门控，连接前不会打印快照 |
| 刚连接时个别字段为 0 | 正常现象：SDK 的各路数据包到达有先后，等待下一帧快照即可 |

## 与 SDK 示例对照

SDK 自带示例 `zsibot-sdk/genisom_L1_sdk/build/example/remote_control` 按 `i` 也会打印状态。注意：

- 示例与 `read_state` **不能同时运行**（UDP 8080 端口冲突）。
- 示例的关节打印把数组下标误当成关节类型；本项目按 SDK 文档顺序输出：`0=左前(LF), 1=右前(RF), 2=左后(LR), 3=右后(RR)`，每条腿包含 abad/hip/knee/foot。
- 本项目打印全部 16 个电机温度，示例只打印前 4 个。

## 许可证

MIT（与 Zsibot SDK 一致）。
