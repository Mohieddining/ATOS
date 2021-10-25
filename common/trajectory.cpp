#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include "regexpatterns.hpp"
#include "logging.h"
#include "trajectory.hpp"

const std::regex Trajectory::fileHeaderPattern("TRAJECTORY;(" + RegexPatterns::intPattern + ");("
		+ RegexPatterns::namePattern + ");" + RegexPatterns::versionPattern + ";("
		+ RegexPatterns::intPattern + ");");
const std::regex Trajectory::fileLinePattern("LINE;(" + RegexPatterns::floatPattern + ");("
		+ RegexPatterns::floatPattern + ");(" + RegexPatterns::floatPattern + ");("
		+ RegexPatterns::floatPattern + ")?;(" + RegexPatterns::floatPattern + ");("
		+ RegexPatterns::floatPattern + ")?;(" + RegexPatterns::floatPattern + ")?;("
		+ RegexPatterns::floatPattern + ")?;(" + RegexPatterns::floatPattern + ")?;("
		+ RegexPatterns::floatPattern + ");(" + RegexPatterns::intPattern + ");ENDLINE;");
const std::regex Trajectory::fileFooterPattern("ENDTRAJECTORY;");

Trajectory::Trajectory(const Trajectory& other) {
	this->id = other.id;
	this->name = other.name;
	this->version = other.version;
	this->points = std::vector<TrajectoryPoint>(other.points);
}

void Trajectory::initializeFromFile(const std::string &fileName) {

	using namespace std;
	char trajDirPath[PATH_MAX];
	string errMsg;
	smatch match;
	ifstream file;
	bool isHeaderParsedSuccessfully = false;
	unsigned long nPoints = 0;

	UtilGetTrajDirectoryPath(trajDirPath, sizeof (trajDirPath));
	string trajFilePath(trajDirPath);
	trajFilePath += fileName;

	file.open(trajFilePath);
	if (!file.is_open()) {
		throw ifstream::failure("Unable to open file <" + trajFilePath + ">");
	}

	string line;
	errMsg = "Encountered unexpected end of file while reading file <" + trajFilePath + ">";
	for (unsigned long lineCount = 0; getline(file, line); lineCount++) {
		if (lineCount == 0) {
			if (regex_search(line, match, this->fileHeaderPattern)) {
				this->id = stoi(match[1]);
				this->name = match[2];
				this->version = 0;
				nPoints = stoul(match[6]);
				this->points.reserve(nPoints);
				isHeaderParsedSuccessfully = true;
			}
			else {
				errMsg = "The header of trajectory file <" + trajFilePath + "> is badly formatted";
				break;
			}
		}
		else if (lineCount > 0 && !isHeaderParsedSuccessfully) {
			errMsg = "Attempt to parse trajectory file <" + trajFilePath + "> before encountering header";
			break;
		}
		else if (lineCount > nPoints + 1) {
			errMsg = "Trajectory line count of file <" + trajFilePath
					+ "> does not match specified line count";
			break;
		}
		else if (lineCount == nPoints + 1) {
			if (regex_search(line, match, fileFooterPattern)) {
				file.close();
				LogMessage(LOG_LEVEL_DEBUG, "Closed <%s>", trajFilePath.c_str());
				return;
			}
			else {
				errMsg = "Final line of trajectory file <" + trajFilePath + "> badly formatted";
				break;
			}
		}
		else {
			if (regex_search(line, match, fileLinePattern)) {
				TrajectoryPoint point;
				point.setTime(stod(match[1]));
				point.setXCoord(stod(match[2]));
				point.setYCoord(stod(match[3]));
				if (match[4].matched)
					point.setZCoord(stod(match[4]));
				point.setHeading(stod(match[5]));
				if (match[6].matched)
					point.setLongitudinalVelocity(stod(match[6]));
				if (match[7].matched)
					point.setLateralVelocity(stod(match[7]));
				if (match[8].matched)
					point.setLongitudinalAcceleration(stod(match[8]));
				if (match[9].matched)
					point.setLateralAcceleration(stod(match[9]));
				point.setCurvature(stod(match[10]));
				point.setMode(static_cast<Trajectory::TrajectoryPoint::ModeType>(stoi(match[11])));
				points.push_back(point);
			}
			else {
				errMsg = "Line " + to_string(lineCount) + " of trajectory file <"
						+ trajFilePath + "> badly formatted";
				break;
			}
		}
	}
	file.close();
	LogMessage(LOG_LEVEL_DEBUG, "Closed <%s>", trajFilePath.c_str());
	throw invalid_argument(errMsg);
}


CartesianPosition Trajectory::TrajectoryPoint::getISOPosition() const {
	CartesianPosition retval;
	retval.xCoord_m = this->getXCoord();
	retval.yCoord_m = this->getYCoord();
	try {
		retval.zCoord_m = this->getZCoord();
	} catch (std::out_of_range e) {
		LogMessage(LOG_LEVEL_WARNING, "Casting trajectory point to cartesian position: optional z value assumed to be 0");
		retval.zCoord_m = 0.0;
	}
	retval.heading_rad = this->getHeading();
	retval.isHeadingValid = true;
	retval.isPositionValid = true;
	return retval;
}

SpeedType Trajectory::TrajectoryPoint::getISOVelocity() const {
	SpeedType retval;
	try {
	   retval.longitudinal_m_s = getLongitudinalVelocity();
	   retval.isLongitudinalValid = true;
	} catch (std::out_of_range) {
	   retval.longitudinal_m_s = 0.0;
	   retval.isLongitudinalValid = false;
	}
	try {
	   retval.lateral_m_s = getLateralVelocity();
	   retval.isLateralValid = true;
	} catch (std::out_of_range) {
	   retval.lateral_m_s = 0.0;
	   retval.isLateralValid = false;
	}
	return retval;
}

AccelerationType Trajectory::TrajectoryPoint::getISOAcceleration() const {
	AccelerationType retval;
	try {
	   retval.longitudinal_m_s2 = getLongitudinalAcceleration();
	   retval.isLongitudinalValid = true;
	} catch (std::out_of_range) {
	   retval.longitudinal_m_s2 = 0.0;
	   retval.isLongitudinalValid = false;
	}
	try {
	   retval.lateral_m_s2 = getLateralAcceleration();
	   retval.isLateralValid = true;
	} catch (std::out_of_range) {
	   retval.lateral_m_s2 = 0.0;
	   retval.isLateralValid = false;
	}
	return retval;
}

/*!
 * \brief Trajectory::TrajectoryPoint::relativeTo
 * \param other
 * \return
 */
Trajectory::TrajectoryPoint Trajectory::TrajectoryPoint::relativeTo(
		const TrajectoryPoint &other) const {

	using namespace Eigen;
	TrajectoryPoint relative;

	relative.setTime(this->getTime());
	relative.setHeading(this->getHeading() - other.getHeading());

	AngleAxis R3(-other.getHeading(), Vector3d::UnitZ());
	Rotation2Dd R(-relative.getHeading());
	relative.setPosition(R3*(zeroNaNs(this->getPosition())
						 - zeroNaNs(other.getPosition())));

	auto thisVel = zeroNaNs(this->getVelocity());
	auto otherVel = zeroNaNs(other.getVelocity());
	relative.setVelocity(R.inverse()*(R*thisVel - otherVel));

	auto thisAcc = zeroNaNs(this->getAcceleration());
	auto otherAcc = zeroNaNs(other.getAcceleration());
	relative.setAcceleration(R.inverse()*(R*thisAcc - otherAcc));

	// K(t) = ||r'(t) x r''(t)|| / ||r'(t)||³
	auto rPrim = R*this->getVelocity();
	auto rBis = R*this->getAcceleration();
	Eigen::Vector3d rPrim3(rPrim[0], rPrim[1], 0.0);
	Eigen::Vector3d rBis3(rBis[0], rBis[1], 0.0);
	if (rPrim3.norm() > 0.001) {
		relative.setCurvature(rPrim3.cross(rBis3).norm()
							  / std::pow(rPrim3.norm(), 3));
	}
	else {
		relative.setCurvature(0.0);
	}

	relative.setMode(this->getMode());
	return relative;
}

Trajectory Trajectory::relativeTo(
		const Trajectory &other) const {
	using namespace Eigen;
	// TODO check that ranges are sorted by time
	if (this->version != other.version) {
		throw std::invalid_argument("Attempted to calculate relative trajectory "
									"for two trajectories with differing versions");
	}

	Trajectory relative;
	relative.id = this->id;
	relative.name = this->name + "_rel_" + other.name;
	relative.version = this->version;
	for (auto trajPt = this->points.begin(); trajPt != this->points.end(); ++trajPt) {
		auto nearestTrajPtInOther = getNearest(other.points.begin(), other.points.end(), trajPt->getTime());
		// TODO maybe a check on time difference here
		relative.points.push_back(trajPt->relativeTo(*nearestTrajPtInOther));
	}

	return relative;
}

Trajectory::const_iterator Trajectory::getNearest(
		const_iterator first,
		const_iterator last,
		const double &time) {
	// Assumption: input range is sorted by time
	// Get first element with larger time than requested
	const_iterator after = std::lower_bound(first, last, time, [](const TrajectoryPoint& trajPt, const double& t) {
		return trajPt.getTime() < t;
	});

	if (after == first) return first;
	if (after == last)  return last - 1;

	const_iterator before = after - 1;

	// Return the element nearest to the requested time
	return (after->getTime() - time) < (time - before->getTime()) ? after : before;
}

std::string Trajectory::TrajectoryPoint::getFormatString() const {
	return "x:[m], y:[m], z:[m], hdg:[rad CW from N], "
		   "vx:[m/s,longitudinal], vy:[m/s,lateral], "
		   "ax:[m/s2,longitudinal], ay:[m/s2,lateral], "
		   "c:[1/m], md:[]";
}

std::string Trajectory::TrajectoryPoint::toString() const {
	std::string retval = "";
	std::stringstream ss(retval);
	ss << "x:" << position[0] << ", "
	   << "y:" << position[1] << ", "
	   << "z:" << position[2] << ", "
	   << "hdg:" << heading << ", "
	   << "vx:" << velocity[0] << ", "
	   << "vy:" << velocity[1] << ", "
	   << "ax:" << acceleration[0] << ", "
	   << "ay:" << acceleration[1] << ", "
	   << "c:" << curvature << ", "
	   << "md:" << mode;
	return ss.str();
}

std::string Trajectory::toString() const {
	std::string retval = "";
	std::stringstream ss(retval);
	ss << "Trajectory:"
	   << "\n Name: " << this->name
	   << "\n ID: " << this->id
	   << "\n Version: " << this->version;
	for (const auto& point : points) {
		ss << "\n\t" << point.toString();
	}
	return ss.str();
}

Trajectory Trajectory::appendedWith(
		const Trajectory &other) {
	Trajectory newTrajectory = this->points.empty() ? Trajectory(other) : Trajectory(*this);
	newTrajectory.name = this->name + "_app_" + other.name;
	if (this->points.empty() || other.points.empty()) {
		return newTrajectory;
	}
	return Trajectory(); // TEMP
}

/*!
 * \brief Trajectory::TrajectoryPoint::rescaleToVelocity Returns a copy of the trajectory rescaled to match a certain constant speed.
 * \param vel_m_s Speed to which trajectory is to be reduced
 * \return Trajectory
 */
Trajectory Trajectory::rescaledToVelocity(
		const double vel_m_s) const {

	Trajectory newTrajectory = Trajectory(*this);
	newTrajectory.name = newTrajectory.name + "_rescaled";
	if (newTrajectory.points.empty()) {
		return newTrajectory;
	}
	Eigen::Vector2d maxVel_m_s = std::max_element(newTrajectory.points.begin(), newTrajectory.points.end(), [](const TrajectoryPoint& pt1, const TrajectoryPoint& pt2)
								{ return pt1.getVelocity().norm() < pt2.getVelocity().norm(); }).base()->getVelocity();
	if (vel_m_s > maxVel_m_s.norm()) {
		LogMessage(LOG_LEVEL_DEBUG, "Requested max velocity is larger than current max velocity");
		return newTrajectory;
	}
	double scaleFactor = vel_m_s / maxVel_m_s.norm();
	double startTime = newTrajectory.points.front().getTime();
	for (auto& point : newTrajectory.points) {
		point.setVelocity(point.getVelocity()*scaleFactor);
		point.setTime(startTime + (point.getTime()-startTime)/scaleFactor);
		point.setAcceleration(point.getAcceleration()*pow(scaleFactor, 2));
	}

	return newTrajectory;
}

/*!
 * \brief Trajectory::TrajectoryPoint::reversed Returns a reversed copy of the trajectory
 */
Trajectory Trajectory::reversed() const {

	Trajectory newTrajectory = Trajectory(*this);

	newTrajectory.name = newTrajectory.name + "_reversed";

	std::reverse(newTrajectory.points.begin(), newTrajectory.points.end());

	for (auto & point : newTrajectory.points) {
		point.setHeading(point.getHeading()-M_PI);
		point.setCurvature(point.getCurvature()*-1);
		try {
			point.setLateralVelocity(point.getLateralVelocity()*-1);
		}
		catch (std::out_of_range) {
			LogMessage(LOG_LEVEL_DEBUG, "Ignoring uninitialized lateral velocity");
		}
		try {
			point.setLateralAcceleration(point.getLateralAcceleration()*-1);
		}
		catch (std::out_of_range) {
			LogMessage(LOG_LEVEL_DEBUG, "Ignoring uninitialized lateral acceleration");
		}
	}

	for (unsigned long i = 0 ; i < newTrajectory.points.size(); i++) {
		newTrajectory.points[i].setTime(this->points.back().getTime() - this->points[i].getTime());
	}
	return newTrajectory;
}

/*!
 * \brief Trajectory::saveToFile saves a .traj file of the trajectory to the traj directory.
 * \param fileName
 * \return
 */
void Trajectory::saveToFile(const std::string& fileName) const{
	using std::string, std::smatch, std::ofstream;
	char trajDirPath[PATH_MAX];

	UtilGetTrajDirectoryPath(trajDirPath, sizeof (trajDirPath));
	string trajFilePath(trajDirPath);
	trajFilePath += fileName;

	ofstream outputTraj;
	LogMessage(LOG_LEVEL_DEBUG, "Opening file %s", trajFilePath.c_str());
	try {
		outputTraj.open(trajFilePath);
		LogMessage(LOG_LEVEL_DEBUG, "Outputting trajectory to file");
		outputTraj << "TRAJECTORY;" << this->id <<";" << this->name << ";" << this->version << ";" << this->points.size() << ";" <<  "\n";
		for (const auto& point : points) {
			outputTraj << "LINE;"
			<< std::fixed << std::setprecision(2) << point.getTime() << ";"
			<< std::fixed << std::setprecision(6) << point.getXCoord() << ";"
			<< std::fixed << std::setprecision(6) << point.getYCoord() << ";"
			<< std::fixed << std::setprecision(6) << point.getZCoord() << ";"
			<< std::fixed << std::setprecision(6) << point.getHeading() << ";"
			<< std::fixed << std::setprecision(6) << point.getLongitudinalVelocity() << ";"
			<< ";" //point.getLateralVelocity() << ";"
			<<  std::fixed << std::setprecision(6) <<(point.getLongitudinalAcceleration()) << ";"
			<< ";" //point.getLateralAcceleration() << ";"
			<< std::fixed << std::setprecision(6) << point.getCurvature() << ";"
			<< std::fixed << std::setprecision(6) << point.getMode()
			<<";ENDLINE;" << "\n";
		}
		outputTraj << "ENDTRAJECTORY;" <<  "\n";
		outputTraj.close();
		LogMessage(LOG_LEVEL_DEBUG, "Closed file %s", trajFilePath.c_str());
	}
	catch (const ofstream::failure& e) {
		LogMessage(LOG_LEVEL_ERROR, "Failed when writing to file %s", trajFilePath.c_str());
	}
}
