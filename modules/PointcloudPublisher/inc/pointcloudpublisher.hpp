/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "module.hpp"
#include "roschannels/pointcloudchannel.hpp"
#include "roschannels/commandchannels.hpp"
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>



class PointcloudPublisher : public Module {

  public:
    PointcloudPublisher();
    ~PointcloudPublisher();

  private:
    static inline std::string const moduleName = "pointcloud_publisher";
    ROSChannels::Init::Sub initSub;
		ROSChannels::Pointcloud::Pub pointcloudPub;

    std::string pointcloudFile;
    sensor_msgs::msg::PointCloud2 msg;
    std::shared_ptr<pcl::PointCloud<pcl::PointXYZRGB>> pointcloud;

    // std::shared_ptr<rclcpp::Publisher<sensor_msgs::msg::PointCloud2>> publisher;
    std::shared_ptr<rclcpp::TimerBase> timer;

		void onInitMessage(const ROSChannels::Init::message_type::SharedPtr) override;
    void initialize();
    void getPointcloudFile();
    void loadPointCloud();
    void createPointcloudMessage();

};