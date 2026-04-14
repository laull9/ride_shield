#pragma once

#ifdef RIDESHIELD_HAS_ROS2

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/string.hpp>

#include <memory>
#include <vector>

namespace RideShield::ros2 {

class FrontPerceptionNode final : public rclcpp::Node {
public:
    FrontPerceptionNode();

private:
    void on_image(const sensor_msgs::msg::Image::SharedPtr& message);

    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_subscription_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr objects_publisher_;
};

class DriverMonitorNode final : public rclcpp::Node {
public:
    DriverMonitorNode();

private:
    void on_driver_frame(const sensor_msgs::msg::Image::SharedPtr& message);

    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr driver_image_subscription_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr driver_state_publisher_;
};

class FusionDecisionNode final : public rclcpp::Node {
public:
    FusionDecisionNode();

private:
    void on_front_objects(const std_msgs::msg::String::SharedPtr& message);
    void on_driver_state(const std_msgs::msg::String::SharedPtr& message);
    void publish_decision();

    std::string latest_front_objects_;
    std::string latest_driver_state_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr front_objects_subscription_;
    rclcpp::Subscription<std_msgs::msg::String>::SharedPtr driver_state_subscription_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr risk_level_publisher_;
};

auto build_default_nodes() -> std::vector<rclcpp::Node::SharedPtr>;

}  // namespace RideShield::ros2

#endif