from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'robot_ip', default_value='192.168.234.1',
            description='机器狗 IP 地址'),
        DeclareLaunchArgument(
            'send_port', default_value='8081',
            description='UDP 发送端口'),
        DeclareLaunchArgument(
            'recv_port', default_value='8080',
            description='UDP 接收端口'),
        DeclareLaunchArgument(
            'role', default_value='remote',
            description='心跳角色：remote 或 sdk'),
        DeclareLaunchArgument(
            'print_period_sec', default_value='1.0',
            description='状态打印周期（秒）'),
        DeclareLaunchArgument(
            'warn_period_sec', default_value='5.0',
            description='未连接警告限流周期（秒）'),
        Node(
            package='read_state',
            executable='read_state_node',
            name='read_state',
            output='screen',
            emulate_tty=True,
            parameters=[{
                'robot_ip': LaunchConfiguration('robot_ip'),
                'send_port': ParameterValue(
                    LaunchConfiguration('send_port'), value_type=int),
                'recv_port': ParameterValue(
                    LaunchConfiguration('recv_port'), value_type=int),
                'role': LaunchConfiguration('role'),
                'print_period_sec': ParameterValue(
                    LaunchConfiguration('print_period_sec'), value_type=float),
                'warn_period_sec': ParameterValue(
                    LaunchConfiguration('warn_period_sec'), value_type=float),
            }],
        ),
    ])
