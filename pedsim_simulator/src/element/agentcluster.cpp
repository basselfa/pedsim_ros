// Simulating Group Dynamics in Crowds of Pedestrians (SGDiCoP)
// Author: Sven Wehner <mail@svenwehner.de>
// Copyright (c) 2013 by Sven Wehner

// Includes
#include <pedsim_simulator/element/agentcluster.h>

// → SGDiCoP
#include <pedsim_simulator/rng.h>

#include "scene.h"
#include <pedsim_simulator/element/waitingqueue.h>

// #include "visual/agentclusterrepresentation.h"
// → Qt
#include <QSettings>


AgentCluster::AgentCluster(double xIn, double yIn, int countIn) {
	static int lastID = 0;
	
	// initialize values
	id = ++lastID;
	position = Ped::Tvector(xIn, yIn);
	count = countIn;
	distribution = QSizeF(0, 0);
	agentType = 0;
	shallCreateGroups = true;

	// graphical representation
	representation = new AgentClusterRepresentation(this);
};

AgentCluster::~AgentCluster() {
	// clean up
	delete representation;
}

QList<Agent*> AgentCluster::dissolve() {
	QList<Agent*> agents;

	std::uniform_real_distribution<double> randomX(-distribution.width()/2, distribution.width()/2);
	std::uniform_real_distribution<double> randomY(-distribution.height()/2, distribution.height()/2);

	// create and initialize agents
	for(int i = 0; i < count; ++i) {
		Agent* a = new Agent();
		
		double randomizedX = position.x;
		double randomizedY = position.y;
		// handle dx=0 or dy=0 cases
		if(distribution.width() != 0)
			randomizedX += randomX(RNG());
		if(distribution.height() != 0)
			randomizedY += randomY(RNG());
		a->setPosition(randomizedX, randomizedY);
		a->setType(agentType);

		// add waypoints to the agent
		foreach(Waypoint* waypoint, waypoints)
			a->addWaypoint(waypoint);
		
		// add agent to the scene
		SCENE.addAgent(a);
		
		// add agent to the return list
		agents.append(a);
	}
	
	return agents;
}

int AgentCluster::getId() const {
	return id;
}

int AgentCluster::getCount() const {
	return count;
}

void AgentCluster::setCount(int countIn) {
	count = countIn;
}

const QList<Waypoint*>& AgentCluster::getWaypoints() const {
	return waypoints;
}

void AgentCluster::addWaypoint(Waypoint* waypointIn) {
	// keep track of waypoint
	waypoints.append(waypointIn);
}

bool AgentCluster::removeWaypoint(Waypoint* waypointIn) {
	// don't keep track of waypoint anymore
	int removedWaypoints = waypoints.removeAll(waypointIn);
	
	// inform user about successful removal
	if(removedWaypoints > 0)
		return true;
	else
		return false;
}

void AgentCluster::addWaitingQueue(WaitingQueue* queueIn) {
	Waypoint* waypoint = dynamic_cast<Waypoint*>(queueIn);

	// keep track of waiting queues
	waypoints.append(waypoint);
}

bool AgentCluster::removeWaitingQueue(WaitingQueue* queueIn) {
	Waypoint* waypoint = dynamic_cast<Waypoint*>(queueIn);

	// don't keep track of waypoint anymore
	int removedWaitingQueues = waypoints.removeAll(waypoint);
	
	// inform user about successful removal
	return (removedWaitingQueues > 0);
}

Ped::Tvector AgentCluster::getPosition() const {
	return position;
}

void AgentCluster::setPosition(const Ped::Tvector& positionIn) {
	position = positionIn;

	// inform users
	emit positionChanged(position.x, position.y);
}

void AgentCluster::setPosition(double px, double py) {
	setPosition(Ped::Tvector(px, py));
}

void AgentCluster::setX(double xIn) {
	position.x = xIn;

	// inform users
	emit positionChanged(position.x, position.y);
}

void AgentCluster::setY(double yIn) {
	position.y = yIn;

	// inform users
	emit positionChanged(position.x, position.y);
}

int AgentCluster::getType() const {
	return agentType;
}

void AgentCluster::setType(int typeIn) {
	agentType = typeIn;

	// inform users
	emit typeChanged(agentType);
}

bool AgentCluster::getShallCreateGroups() const {
	//TODO: actually use this
	return shallCreateGroups;
}

void AgentCluster::setShallCreateGroups(bool shallCreateGroupsIn) {
	shallCreateGroups = shallCreateGroupsIn;
}

QSizeF AgentCluster::getDistribution() const {
	return distribution;
}

void AgentCluster::setDistribution(double xIn, double yIn) {
	distribution.setWidth(xIn);
	distribution.setHeight(yIn);
}

void AgentCluster::setDistributionWidth(double xIn) {
	distribution.setWidth(xIn);
}

void AgentCluster::setDistributionHeight(double yIn) {
	distribution.setHeight(yIn);
}

QPointF AgentCluster::getVisiblePosition() const {
	return QPointF(position.x, position.y);
}

void AgentCluster::setVisiblePosition(const QPointF& positionIn) {
	setPosition(positionIn.x(), positionIn.y());
}

QString AgentCluster::toString() const {
	return tr("AgentCluster (@%1,%2)").arg(position.x).arg(position.y);
}
