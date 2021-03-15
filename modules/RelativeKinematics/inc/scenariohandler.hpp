#pragma once

#include "state.hpp"
#include "testobject.hpp"
#include <map>

// Forward declarations
class ObjectControlState;
namespace RelativeKinematics {
	class Idle;
	class Initialized;
	class Connecting;
	class Ready;
	class Aborting;
	class Armed;
	class TestLive;
	class Disarming;
	class Done;
}

class ScenarioHandler {
	friend class ObjectControlState;
	friend class RelativeKinematics::Idle;
	friend class RelativeKinematics::Initialized;
	friend class RelativeKinematics::Connecting;
	friend class RelativeKinematics::Ready;
	friend class RelativeKinematics::Aborting;
	friend class RelativeKinematics::Armed;
	friend class RelativeKinematics::TestLive;
	friend class RelativeKinematics::Disarming;
	friend class RelativeKinematics::Done;
public:
	typedef enum {
		RELATIVE_KINEMATICS,
		ABSOLUTE_KINEMATICS
	} ControlMode;

	ScenarioHandler(ControlMode);
	~ScenarioHandler();

	void handleInitCommand();

	std::vector<uint32_t> getVehicleUnderTestIDs() const;
	std::vector<uint32_t> getVehicleIDs() const {
		std::vector<uint32_t> retval;
		for (auto it  = objects.begin(); it != objects.end(); ++it) {
			retval.push_back(it->first);
		}
		return retval;
	}
private:
	ObjectControlState* state;
	std::map<uint32_t,TestObject> objects;

	void loadScenario();
	void loadObjectFiles();
	void parseObjectFile(const fs::path& objectFile, TestObject& object);
	void transformScenarioRelativeTo(const uint32_t objectID);
};


