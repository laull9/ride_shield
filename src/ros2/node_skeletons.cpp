#include "RideShield/ros2/node_skeletons.h"

#ifdef RIDESHIELD_HAS_ROS2

namespace RideShield::ros2 {

FrontPerceptionNode::FrontPerceptionNode()
    : rclcpp::Node("front_perception_node") {
    image_subscription_ = create_subscription<sensor_msgs::msg::Image>(
        "/camera/front/image_raw",
        rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::Image::SharedPtr message) { on_image(message); }
    );
    objects_publisher_ = create_publisher<std_msgs::msg::String>("/perception/front/objects", 10);
}

void FrontPerceptionNode::on_image(const sensor_msgs::msg::Image::SharedPtr& message) {
    std_msgs::msg::String objects_message;
    objects_message.data = "front_frame_received:" + std::to_string(message->width) + "x" + std::to_string(message->height);
    objects_publisher_->publish(objects_message);
}

DriverMonitorNode::DriverMonitorNode()
    : rclcpp::Node("driver_monitor_node") {
    driver_image_subscription_ = create_subscription<sensor_msgs::msg::Image>(
        "/camera/driver/image_raw",
        rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::Image::SharedPtr message) { on_driver_frame(message); }
    );
    driver_state_publisher_ = create_publisher<std_msgs::msg::String>("/driver/state", 10);
}

void DriverMonitorNode::on_driver_frame(const sensor_msgs::msg::Image::SharedPtr& message) {
    std_msgs::msg::String driver_state;
    driver_state.data = "driver_frame_received:" + std::to_string(message->width) + "x" + std::to_string(message->height);
    driver_state_publisher_->publish(driver_state);
}

FusionDecisionNode::FusionDecisionNode()
    : rclcpp::Node("fusion_decision_node") {
    front_objects_subscription_ = create_subscription<std_msgs::msg::String>(
        "/perception/front/objects",
        10,
        [this](const std_msgs::msg::String::SharedPtr message) { on_front_objects(message); }
    );
    driver_state_subscription_ = create_subscription<std_msgs::msg::String>(
        "/driver/state",
        10,
        [this](const std_msgs::msg::String::SharedPtr message) { on_driver_state(message); }
    );
    risk_level_publisher_ = create_publisher<std_msgs::msg::String>("/decision/risk_level", 10);
}

void FusionDecisionNode::on_front_objects(const std_msgs::msg::String::SharedPtr& message) {
    latest_front_objects_ = message->data;
    publish_decision();
}

void FusionDecisionNode::on_driver_state(const std_msgs::msg::String::SharedPtr& message) {
    latest_driver_state_ = message->data;
    publish_decision();
}

void FusionDecisionNode::publish_decision() {
    std_msgs::msg::String risk_message;
    risk_message.data = "front=" + latest_front_objects_ + ";driver=" + latest_driver_state_;
    risk_level_publisher_->publish(risk_message);
}

auto build_default_nodes() -> std::vector<rclcpp::Node::SharedPtr> {
    std::vector<rclcpp::Node::SharedPtr> nodes;
    nodes.emplace_back(std::make_shared<FrontPerceptionNode>());
    nodes.emplace_back(std::make_shared<DriverMonitorNode>());
    nodes.emplace_back(std::make_shared<FusionDecisionNode>());
    return nodes;
}

}  // namespace RideShield::ros2

#endif