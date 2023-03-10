#pragma once

#include "module.hpp"
#include <sensor_msgs/msg/point_cloud2.hpp>



class PointcloudPublisher : public Module {

  public:
    PointcloudPublisher();
    ~PointcloudPublisher();

  private:
    static inline std::string const moduleName = "pointcloud_publisher";
    std::string pointcloudFile;
    sensor_msgs::msg::PointCloud2 msg;

    std::shared_ptr<rclcpp::Publisher<sensor_msgs::msg::PointCloud2>> publisher;
    std::shared_ptr<rclcpp::TimerBase> timer;


    void initialize();
    void getPointcloudFile();
    void loadPointCloud();
    void createPointcloudMessage();
    void publishPointcloud();
};