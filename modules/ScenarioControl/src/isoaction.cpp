#include "isoaction.h"
#include "logging.h"
#include "util.h"

Action::ActionReturnCode_t ISOAction::execute(void)
{
    EXACData data;

    if (remainingAllowedRuns == 0)
        return NO_REMAINING_RUNS;
    else {
        data.actionID = actionID;
        data.delayTime_qms = actionDelayTime_qms;
        data.ip = actionObjectIP;

        LogMessage(LOG_LEVEL_INFO, "Sending execute action message over message bus (action ID %u)", actionID);
        if(iCommSendEXAC(data) == -1)
            return NOT_OK;

        remainingAllowedRuns--;
        return OK;
    }
}
