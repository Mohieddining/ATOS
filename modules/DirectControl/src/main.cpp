#include <iostream>
#include <unistd.h>
#include <signal.h>

#include "logging.h"
#include "util.h"
#include "datadictionary.h"
#include "directcontrol.hpp"

#define MODULE_NAME "DirectControl"

static std::shared_ptr<DirectControl> dc;
static void signalHandler(int signo);
int initializeModule(const LOG_LEVEL logLevel);

int main() {
	rclcpp::init(0,nullptr);
	dc = std::make_shared<DirectControl>();
	const LOG_LEVEL logLevel = LOG_LEVEL_DEBUG;
	// Initialize
	if (initializeModule(logLevel) < 0) {
		util_error("Failed to initialize module");
	}
	dc->run();
	rclcpp::shutdown();
	return 0;
}

/*!
 * \brief initializeModule Initializes this module by creating log,
 *			connecting to the message queue bus, setting up signal handers etc.
 * \param logLevel Level of the module log to be used.
 * \return 0 on success, -1 otherwise
 */
int initializeModule(const LOG_LEVEL logLevel) {
	int retval = 0;
	struct timespec sleepTimePeriod, remTime;
	sleepTimePeriod.tv_sec = 0;
	sleepTimePeriod.tv_nsec = 1000000;
	int maxRetries = 10, retryNumber;

	// Initialize log
	LogInit(MODULE_NAME, logLevel);
	LogMessage(LOG_LEVEL_INFO, MODULE_NAME " task running with PID: %d", getpid());

	// Set up signal handlers
	/*
	LogMessage(LOG_LEVEL_DEBUG, "Initializing signal handler");
	if (signal(SIGINT, signalHandler) == SIG_ERR) {
		perror("signal");
		retval = -1;
		LogMessage(LOG_LEVEL_ERROR, "Unable to initialize signal handler");
	}
	*/

	// Initialize message bus connection
	/*
	LogMessage(LOG_LEVEL_DEBUG, "Initializing connection to message bus");
	for (retryNumber = 0; iCommInit() != 0 && retryNumber < maxRetries; ++retryNumber) {
		nanosleep(&sleepTimePeriod, &remTime);
	}
	if (retryNumber == maxRetries) {
		retval = -1;
		LogMessage(LOG_LEVEL_ERROR, "Unable to initialize connection to message bus");
	}
	*/
	if (DataDictionaryInitObjectData() != READ_OK) {
		retval = -1;
		LogMessage(LOG_LEVEL_ERROR, "Preexisting data dictionary not found");
	}
	return retval;
}


void signalHandler(int signo) {
	if (signo == SIGINT) {
		LogMessage(LOG_LEVEL_WARNING, "Caught keyboard interrupt");
		dc->exit();
	}
	else {
		LogMessage(LOG_LEVEL_ERROR, "Caught unhandled signal");
	}
}
