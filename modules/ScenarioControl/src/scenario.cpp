/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "atosTime.h"

#include <fstream>
#include <stdexcept>
#include <regex>

#include "scenario.h"

#include "isotrigger.h"
#include "externalaction.h"
#include "braketrigger.h"
#include "distancetrigger.h"

namespace ATOS {

	Scenario::Scenario(const std::string scenarioFilePath, 
						const std::string openDriveFilePath, 
						const std::string openScenarioFilePath,  
						rclcpp::Logger log) : Loggable(log)
	{
		initialize(scenarioFilePath, openDriveFilePath, openScenarioFilePath);
	}


	Scenario::~Scenario()
	{
		clear();
	}

	void Scenario::clear()
	{
		for (Trigger* tp : allTriggers)
			delete tp;

		for (Action* ap : allActions)
			delete ap;

		causalities->clear();
		allTriggers.clear();
		allActions.clear();
	}

	void Scenario::loadOpenDrive(const std::string openDriveFilePath)
	{
		xodr openDriveParser;
		openDriveParser.load(openDriveFilePath);
		openDriveParser.parse();
		openDriveObject = openDriveParser.m_OpenDRIVE;
	}
	void Scenario::loadOpenScenario(const std::string openScenarioFilePath)
	{
		xosc openScenarioParser;
		openScenarioParser.load(openScenarioFilePath);
		openScenarioParser.parse();
		openScenarioObject = openScenarioParser.m_OpenSCENARIO;
	}

	void Scenario::initialize(const std::string scenarioFilePath,std::string openDriveFilePath, std::string openScenarioFilePath)
	{
		std::ifstream file;
		std::string debugStr;
		causalities = std::make_shared<std::set<Causality>>();

		clear();
		RCLCPP_DEBUG(get_logger(), "Opening scenario file <%s>", scenarioFilePath.c_str());
		file.open(scenarioFilePath);

		if (file.is_open())
		{
			try {
				parseScenarioFile(file);
			} catch (std::invalid_argument) {
				file.close();
				throw;
			}
			file.close();
		}
		else {
			throw std::ifstream::failure("Unable to open file <" + scenarioFilePath + ">");
		}
		RCLCPP_INFO(get_logger(),"Successfully initialized scenario with %d unique triggers and %d unique actions", allTriggers.size(), allActions.size());

		try{
			loadOpenDrive(openDriveFilePath);
		}
		catch(const std::exception &e){
			RCLCPP_WARN(get_logger(),"Failed to parse OpenDrive: %s", e.what());
		}
		catch(...){
			RCLCPP_WARN(get_logger(),"Failed to parse OpenDrive");
		}
		try{
			loadOpenScenario(openScenarioFilePath);
		}
		catch(const std::exception &e){
			RCLCPP_WARN(get_logger(),"Failed to parse OpenSCENARIO: %s", e.what());
		}
		catch(...){
			RCLCPP_WARN(get_logger(),"Failed to parse OpenSCENARIO");
		}

		debugStr =  "\nTriggers:\n";
		for (Trigger* tp : allTriggers)
			debugStr += "\t" + tp->getObjectIPAsString() + "\t" + tp->getTypeAsString(tp->getTypeCode()) + "\n";

		debugStr += "Actions:\n";
		for (Action* ap : allActions)
			debugStr += "\t" + ap->getObjectIPAsString() + "\t" + ap->getTypeAsString(ap->getTypeCode()) + "\n";

		debugStr.pop_back();
		RCLCPP_DEBUG(get_logger(),debugStr.c_str());
	}

	void Scenario::reset() {
		for (Action* ap : allActions) {
			ap->reset();
		}
	}

	/*!
	* \brief Scenario::splitLine Splits a line at specified delimiter and stores the generated substrings in a vector (excluding the delimiters)
	* \param line Line to be split
	* \param delimiter Delimiter at which line is to be split
	* \param result Vector in which to store substrings generated by splitting operation
	*/
	void Scenario::splitLine(const std::string &line, const char delimiter, std::vector<std::string> &result)
	{
		std::istringstream strm(line);
		std::string part;
		while (getline(strm,part,delimiter)) result.push_back(part);
		return;
	}

	/*!
	* \brief Scenario::parseScenarioFileLine Converts a line from the trigger and action configuration file into a number of linked triggers
	* and actions, and modifies members accordingly. Lines which are empty or start with a pound sign are ignored.
	* \param inputLine Line to be decoded
	*/
	void Scenario::parseScenarioFileLine(const std::string &inputLine)
	{
		using namespace std;
		if (inputLine[0] == '#' || inputLine.length() == 0) return;
		string line = inputLine;

		// Remove whitespace
		string::iterator endPos = remove_if(line.begin(), line.end(), ::isspace);
		line.erase(endPos,line.end());

		// Split into parts
		vector<string> parts;
		constexpr char delimiter = ';';
		splitLine(line,delimiter,parts);

		// Match relevant field according to below patterns
		regex ipAddrPattern("([0-2]?[0-9]?[0-9]\\.){3}([0-2]?[0-9]?[0-9])"); // Match 3 "<000-299>." followed by "<000-299>"
		regex triggerActionPattern("(([a-zA-Z_])+\\[([a-zA-Z0-9\\.,\\-<=>_:()])+\\])+");
		in_addr triggerIP, actionIP;
		string errMsg;
		set<Action*> actions;
		set<Trigger*> triggers;

		// Expect a line to consist of trigger IP, triggers, action IP and actions in that order
		enum {TRIGGER_IP,TRIGGER,ACTION_IP,ACTION,DONE} parseState = TRIGGER_IP;

		for (const string &part : parts) {
			switch (parseState) {
			case TRIGGER_IP:
				if(!regex_match(part,ipAddrPattern)) {
					errMsg = "Specified trigger IP address field <" + part + "> is invalid";
					RCLCPP_ERROR(get_logger(), errMsg.c_str());
					throw invalid_argument(errMsg);
				}

				if(inet_pton(AF_INET, part.c_str(), &triggerIP) <= 0) {
					errMsg = "Error parsing IP string <" + part + ">";
					RCLCPP_ERROR(get_logger(), errMsg.c_str());
					throw invalid_argument(errMsg);
				}
				parseState = TRIGGER;
				break;
			case TRIGGER:
				if(!regex_match(part,triggerActionPattern)) {
					errMsg = "Trigger configuration field <" + part + "> is invalid";
					RCLCPP_ERROR(get_logger(), errMsg.c_str());
					throw invalid_argument(errMsg);
				}

				triggers = parseTriggerConfiguration(part);
				for (Trigger* tp : triggers) {
					tp->setObjectIP(triggerIP.s_addr);
					for (Trigger* knownTrigger : allTriggers)
					{
						if(tp->isSimilar(*knownTrigger))
						{
							triggers.erase(tp);
							triggers.insert(knownTrigger);
							tp = knownTrigger;
						}
					}
					addTrigger(tp);
				}
				parseState = ACTION_IP;
				break;
			case ACTION_IP:
				if(!regex_match(part,ipAddrPattern)) {
					errMsg = "Specified action IP address field <" + part + "> is invalid";
					RCLCPP_ERROR(get_logger(), errMsg.c_str());
					throw invalid_argument(errMsg);
				}

				if(inet_pton(AF_INET, part.c_str(), &actionIP) <= 0) {
					errMsg = "Error parsing IP string <" + part + ">";
					RCLCPP_ERROR(get_logger(), errMsg.c_str());
					throw invalid_argument(errMsg);
				}
				parseState = ACTION;
				break;
			case ACTION:
				if(!regex_match(part,triggerActionPattern)) {
					errMsg = "Action configuration field <" + part + "> is invalid";
					RCLCPP_ERROR(get_logger(), errMsg.c_str());
					throw invalid_argument(errMsg);
				}

				actions = parseActionConfiguration(part);
				for (Action* ap : actions) {
					ap->setObjectIP(actionIP.s_addr);
					addAction(ap);
				}
				parseState = DONE;
				break;
			case DONE:
				if (part.length() != 0) RCLCPP_WARN(get_logger(),"Ignored tail field of row in configuration file: <%s>",part.c_str());
				break;
			}
		}

		linkTriggersWithActions(triggers, actions);
		return;
	}

	/*!
	* \brief Scenario::parseTriggerConfiguration Parses a field from the trigger and action configuration file corresponding to trigger
	* configurations.
	* \param inputConfig The part of a configuration line which corresponds to trigger configurations
	* \return A set of trigger pointers according to what was found in the configuration
	*/
	std::set<Trigger*> Scenario::parseTriggerConfiguration(const std::string &inputConfig)
	{
		using namespace std;
		regex triggerPattern("([a-zA-Z_]+)\\[([^,^\\]]+)(?:,([^,^\\]]+))?(?:,([^,^\\]]+))?\\]");
		smatch match;
		string errMsg;
		vector<string> configs;
		Trigger::TriggerTypeCode_t triggerType;
		set<Trigger*> returnTriggers;
		Trigger* trigger;
		Trigger::TriggerID_t baseTriggerID = static_cast<Trigger::TriggerID_t>(allTriggers.size());

		// Split the configuration to allow for several in a row
		splitLine(inputConfig, ']', configs);
		for (string &config : configs)
			config.append("]");

		// Loop through the specified triggers and generate a trigger for each
		for(const string &config : configs)
		{
			if (!regex_search(config, match, triggerPattern))
			{
				errMsg = "The following is not a valid configuration: <" + config + ">";
				RCLCPP_ERROR(get_logger(), errMsg.c_str());
				throw invalid_argument(errMsg);
			}

			triggerType = Trigger::asTypeCode(match[1]);
			switch (triggerType) {
			// If the trigger type has ATOS monitoring implemented, use that
			case TRIGGER_BRAKE:
				trigger = new BrakeTrigger(get_logger(), baseTriggerID + static_cast<Trigger::TriggerID_t>(returnTriggers.size()));
				// TODO: possibly the OR between the ATOS trigger and possible TREO messages
				break;
			case TRIGGER_DISTANCE:
				trigger = new DistanceTrigger(get_logger(), baseTriggerID + static_cast<Trigger::TriggerID_t>(returnTriggers.size()));
				break;
			default:
				// Trigger with unimplemented ATOS monitoring: let object handle trigger reporting
				trigger = new ISOTrigger(get_logger(), baseTriggerID + static_cast<Trigger::TriggerID_t>(returnTriggers.size()), triggerType);
				break;
			}

			// Transfer specified parameters to relevant containers
			for (unsigned int i = 2; i < match.size(); ++i) {
				if (!match[i].str().empty()){
					if (trigger->appendParameter(match[i].str()) != Trigger::OK) {
						throw std::invalid_argument("Unable to interpret trigger parameter " + match[i].str());
					}
				}
			}

			// Parse all parameters into settings
			if (trigger->parseParameters() != Trigger::OK) {
				throw std::invalid_argument("Unable to interpret trigger configuration " + inputConfig);
			}

			returnTriggers.insert(trigger);
		}

		return returnTriggers;
	}

	/*!
	* \brief Scenario::parseActionConfiguration Parses a field from the trigger and action configuration file corresponding to action
	* configurations.
	* \param inputConfig The part of a configuration line which corresponds to action configurations
	* \return A set of action pointers according to what was found in the configuration
	*/
	std::set<Action*> Scenario::parseActionConfiguration(const std::string &inputConfig)
	{
		using namespace std;
		regex triggerPattern("([a-zA-Z_]+)\\[([^,^\\]]+)(?:,([^,^\\]]+))?(?:,([^,^\\]]+))?\\]");
		smatch match;
		string errMsg;
		vector<string> configs;
		Action::ActionTypeCode_t actionType;
		set<Action*> returnActions;
		Action* action;
		Action::ActionID_t baseActionID = static_cast<Action::ActionID_t>(get_logger(), allActions.size());

		// Split the configuration to allow for several in a row
		splitLine(inputConfig, ']', configs);
		for (string &config : configs)
			config.append("]");

		// Loop through specified actions and generate an action for each
		for(const string &config : configs)
		{
			if (!regex_search(config, match, triggerPattern))
			{
				errMsg = "The following is not a valid configuration: <" + config + ">";
				RCLCPP_ERROR(get_logger(), errMsg.c_str());
				throw invalid_argument(errMsg);
			}

			actionType = Action::asTypeCode(match[1]);
			switch (actionType) {
			// Handle any specialised action types
			case ACTION_INFRASTRUCTURE:
				action = new InfrastructureAction(get_logger(), baseActionID + static_cast<Action::ActionID_t>(returnActions.size()));
				break;
			case ACTION_TEST_SCENARIO_COMMAND:
				action = new TestScenarioCommandAction(get_logger(), baseActionID + static_cast<Action::ActionID_t>(returnActions.size()));
				break;
			default:
				// Regular action (only ACCM and EXAC)
				action = new ExternalAction(get_logger(), baseActionID + static_cast<Action::ActionID_t>(returnActions.size()), actionType);
				break;
			}

			// Transfer specified parameters to relevant containers
			for (unsigned int i = 2; i < match.size(); ++i) {
				if(!match[i].str().empty()) {
					if (action->appendParameter(match[i].str()) != Action::OK) {
						throw std::invalid_argument("Unable to interpret action parameter " + match[i].str());
					}
				}
			}
			// Parse all parameters into settings
			if (action->parseParameters() != Action::OK) {
				throw std::invalid_argument("Unable to interpret trigger configuration " + inputConfig);
			}

			returnActions.insert(action);
		}

		return returnActions;
	}

	/*!
	* \brief Scenario::parseScenarioFile Parses the trigger and action configuration file into relevant triggers and actions, and links between them.
	* \param file File stream for an open configuration file
	*/
	void Scenario::parseScenarioFile(std::ifstream &file)
	{
		RCLCPP_DEBUG(get_logger(), "Parsing scenario file");

		std::string line;
		while ( std::getline(file, line) ) parseScenarioFileLine(line);
		return;
	}

	/*!
	* \brief Scenario::addTrigger Appends a trigger to the list of known triggers, unless another trigger of the same ID is already in place.
	* \param tp Trigger pointer to be added
	* \return Return code according to ::ScenarioReturnCode_t
	*/
	Scenario::ScenarioReturnCode_t Scenario::addTrigger(Trigger* tp)
	{
		for (Trigger* knownTrigger : allTriggers)
			if (knownTrigger->getID() == tp->getID()) return DUPLICATE_ELEMENT;

		RCLCPP_DEBUG(get_logger(), "Adding trigger with ID: %d", tp->getID());
		allTriggers.insert(tp);
		return OK;
	}

	std::shared_ptr<std::set<Causality>> Scenario::getCausalities() {
		return causalities;
	}

	/*!
	* \brief Scenario::addAction Appends an action to the list of known actions, unless another action of the same ID is already in place.
	* \param ap Action pointer to be added
	* \return Return code according to ::ScenarioReturnCode_t
	*/
	Scenario::ScenarioReturnCode_t Scenario::addAction(Action* ap)
	{
		for (Action* knownAction : allActions)
			if (knownAction->getID() == ap->getID()) return DUPLICATE_ELEMENT;

		allActions.insert(ap);
		return OK;
	}


	Scenario::ScenarioReturnCode_t Scenario::linkTriggersWithActions(std::set<Trigger *> ts, std::set<Action *> aps)
	{
		Causality c(Causality::AND);

		for (Trigger* tp : ts)
		{
			c.addTrigger(tp);
		}

		for (Action* ap : aps)
		{
			c.addAction(ap);
		}

		causalities->insert(c);
		return OK;
	}

	Scenario::ScenarioReturnCode_t Scenario::linkTriggerWithAction(Trigger *tp, Action *ap)
	{
		std::set<Trigger*> tps;
		std::set<Action*> aps;
		tps.insert(tp);
		aps.insert(ap);
		return linkTriggersWithActions(tps, aps);
	}

	Scenario::ScenarioReturnCode_t Scenario::linkTriggersWithAction(std::set<Trigger*> tps, Action* ap)
	{
		std::set<Action*> aps;
		aps.insert(ap);
		return linkTriggersWithActions(tps, aps);
	}

	Scenario::ScenarioReturnCode_t Scenario::linkTriggerWithActions(Trigger *tp, std::set<Action *> aps)
	{
		std::set<Trigger*> tps;
		tps.insert(tp);
		return linkTriggersWithActions(tps, aps);
	}

	void Scenario::executeTriggeredActions(ROSChannels::ExecuteAction::Pub& exacPub) const
	{
		for (const Causality& c : *causalities)
		{
			c.executeIfActive(exacPub);
		}
	}

	void Scenario::resetISOTriggers(void)
	{
		for (Trigger* tp : allTriggers)
		{
			if (dynamic_cast<ISOTrigger*>(tp) != nullptr)
			{
				// "untrigger" the trigger
				tp->update();
			}
		}
	}

	Scenario::ScenarioReturnCode_t Scenario::updateTrigger(const ObjectDataType &monr)
	{
		for (Trigger* tp : allTriggers)
		{
			if(tp->getObjectIP() == monr.ClientIP && dynamic_cast<ISOTrigger*>(tp) == nullptr)
			{
				Trigger::TriggerReturnCode_t result = Trigger::NO_TRIGGER_OCCURRED;
				switch (tp->getTypeCode())
				{
				case Trigger::TriggerTypeCode_t::TRIGGER_BRAKE:
					if (monr.MonrData.speed.isLongitudinalValid && monr.MonrData.isTimestampValid) {
						result = tp->update(monr.MonrData.speed.longitudinal_m_s, monr.MonrData.timestamp);
					}
					else {
						RCLCPP_WARN(get_logger(), "Could not update trigger type %s due to invalid monitor data values",
								tp->getTypeAsString(tp->getTypeCode()).c_str());
					}
					break;
				case Trigger::TriggerTypeCode_t::TRIGGER_DISTANCE:
					if (monr.MonrData.position.isPositionValid) {
						result = tp->update(monr);
					}
					else {
						RCLCPP_WARN(get_logger(), "Could not update trigger type %s due to invalid monitor data values",
								tp->getTypeAsString(tp->getTypeCode()).c_str());
					}
					break;
				default:
					RCLCPP_WARN(get_logger(), "Unhandled trigger type in update: %s",
							tp->getTypeAsString(tp->getTypeCode()).c_str());
				}
				if (result == Trigger::TRIGGER_OCCURRED) {
					std::string triggerType = Trigger::getTypeAsString(tp->getTypeCode());
					JournalRecordData(JOURNAL_RECORD_EVENT, "Trigger %s (ID %d) occurred - triggering data timestamped %u [¼ ms of week]",
									triggerType.c_str(), tp->getID(), TimeGetAsGPSqmsOfWeek(&monr.MonrData.timestamp));
				}
			}
		}
	}
}