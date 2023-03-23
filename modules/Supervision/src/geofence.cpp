/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include <fstream>

#include "geofence.hpp"
#include "util.h"
#include "util/fileutils.hpp"
#include "regexpatterns.hpp"

const std::regex Geofence::fileHeaderPattern("GEOFENCE;([a-zA-Z0-9]+);(" + RegexPatterns::intPattern
					+ ");(permitted|forbidden);(" + RegexPatterns::floatPattern + ");("
							   + RegexPatterns::floatPattern + ");");
const std::regex Geofence::fileLinePattern("LINE;(" + RegexPatterns::floatPattern + ");("
							 + RegexPatterns::floatPattern + ");ENDLINE;");
const std::regex Geofence::fileFooterPattern("ENDGEOFENCE;");

Geofence::Geofence(const Geofence& other) {
	this->name = other.name;
	this->maxHeight = other.maxHeight;
	this->minHeight = other.minHeight;
	this->isPermitted = other.isPermitted;
	this->polygonPoints = std::vector<CartesianPosition>(other.polygonPoints);
}

/*!
* \brief parseGeofenceFile Parse the given file into a Geofence object
* \param geofenceFile A string representing the filename relative to the geofence directory.
*/
void Geofence::initializeFromFile(const std::string &fileName) {

	using namespace std;
	ifstream file;
	smatch match;
	string line;
	string errMsg;

	bool isHeaderParsedSuccessfully = false;
	unsigned long nPoints = 0;

	auto geofenceDirPath = Util::getDirectoryPath(Util::GEOFENCE_DIR_NAME);
	std::string geofenceFilePath = geofenceDirPath.string();
	geofenceFilePath += fileName;

	file.open(geofenceFilePath);
	if (!file.is_open()) {
		throw ifstream::failure("Unable to open file <" + geofenceFilePath + ">");
	}

	errMsg = "Encountered unexpected end of file while reading file <" + geofenceFilePath + ">";

	for (unsigned long lineCount = 0; getline(file, line); lineCount++) {
		if (lineCount == 0) {
			if (regex_search(line, match, fileHeaderPattern)) {
				name = match[1];
				nPoints = stoul(match[2]);
				polygonPoints.reserve(nPoints);
				isPermitted = match[3].compare("permitted") == 0;
				minHeight = stod(match[4]);
				minHeight = stod(match[5]);
				isHeaderParsedSuccessfully = true;
			}
			else {
				errMsg = "The header of geofence file <" + geofenceFilePath + "> is badly formatted";
				break;
			}
		}
		else if (lineCount > 0 && !isHeaderParsedSuccessfully) {
			errMsg = "Attempt to parse geofence file <" + geofenceFilePath + "> before encountering header";
			break;
		}
		else if (lineCount > nPoints + 1) {
			errMsg = "Geofence line count of file <" + geofenceFilePath
					+ "> does not match specified line count";
			break;
		}
		else if (lineCount == nPoints + 1) {
			if (regex_search(line, match, fileFooterPattern)) {
				file.close();
				LogMessage(LOG_LEVEL_DEBUG, "Closed <%s>", geofenceFilePath.c_str());
				return;
			}
			else {
				errMsg = "Final line of geofence file <" + geofenceFilePath + "> badly formatted";
				break;
			}
		}
		else {
			if (regex_search(line, match, fileLinePattern)) {
				CartesianPosition pos;
				pos.xCoord_m = stod(match[1]);
				pos.yCoord_m = stod(match[2]);
				pos.zCoord_m = (maxHeight + minHeight) / 2.0;
				pos.isPositionValid = true;
				pos.heading_rad = 0;
				pos.isHeadingValid = false;

				LogMessage(LOG_LEVEL_DEBUG, "Point: (%.3f, %.3f, %.3f)",
						   pos.xCoord_m,
						   pos.yCoord_m,
						   pos.zCoord_m);
				polygonPoints.push_back(pos);
			}
			else {
				errMsg = "Line " + to_string(lineCount) + " of geofence file <"
						+ geofenceFilePath + "> badly formatted";
				break;
			}
		}
	}
	file.close();
	LogMessage(LOG_LEVEL_DEBUG, "Closed <%s>", geofenceFilePath.c_str());
	LogMessage(LOG_LEVEL_ERROR, errMsg.c_str());
	throw invalid_argument(errMsg);
}

bool Geofence::forbids(const CartesianPosition &position) const {
	return !permits(position);
}

bool Geofence::permits(const CartesianPosition &position) const {
	char isInPolygon = UtilIsPointInPolygon(position, polygonPoints.data(),
											static_cast<unsigned int>(polygonPoints.size()));
	if (isInPolygon < 0) {
		throw std::invalid_argument("No points in geofence");
	}

	if ((isPermitted && isInPolygon)
		|| (!isPermitted && !isInPolygon)) {
			return true;
	}
	return false;
}
