#pragma once

#include <future>

#include "module.hpp"
#include "osi_handler.hpp"
#include "server.hpp"
#include "tcphandler.hpp"
#

class OSIAdapter : public Module
{
  public:
    int initialize();
    OSIAdapter();

  private:
    static inline std::string const moduleName = "osi_adapter";
    static inline const int TCPPort = 12345; // what should this be?

    void sendOSIData();
    std::vector<char> makeOSIMessage(const OsiHandler::LocalObjectGroundTruth_t osiData);
    const OsiHandler::LocalObjectGroundTruth_t makeTestOsiData();
    
    void onAbortMessage(const ROSChannels::Abort::message_type::SharedPtr) override;
    
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher;
    rclcpp::TimerBase::SharedPtr timer;
    TCPServer tcp;
    Socket connection;
};