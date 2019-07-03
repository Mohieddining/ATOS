#include "trigger.h"


Trigger::Trigger(TriggerID_t triggerID)
{
    this->triggerID = triggerID;
}

Trigger::~Trigger()
{

}


/*!
 * \brief Trigger::checkTriggerParameter Checks if the queried parameter is within the list of accepted parameters
 * \param triggerParameter Queried parameter
 * \return According to ::TriggerReturnCode_t
 */
Trigger::TriggerReturnCode_t Trigger::checkTriggerParameter(TriggerParameter_t triggerParameter)
{
    std::set<TriggerParameter_t> acceptedParameters = getAcceptedParameters();
    for (const TriggerParameter_t param : acceptedParameters) {
        if (triggerParameter == param)
            return OK;
    }
    return NOT_OK;
}
