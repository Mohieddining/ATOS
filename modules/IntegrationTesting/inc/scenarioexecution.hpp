/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once

#include "integrationtesting.hpp"
#include "atos_interfaces/srv/get_object_control_state.hpp"


class ScenarioExecution : public IntegrationTesting {

	public:
		ScenarioExecution();
		~ScenarioExecution();

	private:
		std::shared_ptr<rclcpp::Client<atos_interfaces::srv::GetObjectControlState>> getObjectControlStateClient;

		void runScenario();
};
