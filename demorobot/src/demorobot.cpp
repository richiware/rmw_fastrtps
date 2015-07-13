#include <iostream>
#include <memory>

#include <rclcpp/rclcpp.hpp>

#include <std_interfaces/msg/string.hpp>

void messageCallback(const std_interfaces::msg::String::ConstSharedPtr &msg)
{
    std::cout << "I heard: [" << msg->data << "}" << std::endl;
}

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    auto node = rclcpp::Node::make_shared("demorobot");
    auto sub = node->create_subscription<std_interfaces::msg::String>("message", 7, messageCallback);

    rclcpp::spin(node);

    return 0;
}
