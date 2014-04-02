// Simulating Group Dynamics in Crowds of Pedestrians (SGDiCoP)
// Author: Sven Wehner <mail@svenwehner.de>
// Copyright (c) 2013 by Sven Wehner

// Includes
#include "queueingplanner.h"
// → SGDiCoP
#include "logging.h"
#include "element/agent.h"
#include "element/queueingwaypoint.h"
#include "element/waitingqueue.h"


QueueingWaypointPlanner::QueueingWaypointPlanner() {
	// initialize values
	agent = nullptr;
	waitingQueue = nullptr;
	currentWaypoint = nullptr;
	followedAgent = nullptr;
	status = QueueingWaypointPlanner::Unknown;
}

void QueueingWaypointPlanner::onFollowedAgentPositionChanged(double xIn, double yIn) {
	// sanity checks
	if(currentWaypoint == nullptr) {
		ERROR_LOG("Queued agent cannot update queueing position, because there's no waypoint set!");
		return;
	}

	Ped::Tvector followedPosition(xIn, yIn);
	addPrivateSpace(followedPosition);

	//HACK: don't update minor changes (prevent over-correcting)
	//TODO: integrate update importance to waypoint (force?)
	const double minUpdateDistance = 0.4;
	Ped::Tvector diff = followedPosition - currentWaypoint->getPosition();
	if(diff.length() < minUpdateDistance)
		return;

	currentWaypoint->setPosition(followedPosition);
}

void QueueingWaypointPlanner::onAgentMayPassQueue(int id) {
	// check who will leave queue
	if((agent != nullptr) && (id == agent->getId())) {
		// the agent may pass
		// → update waypoint
		status = QueueingWaypointPlanner::MayPass;
		
		// remove references to old queue
		disconnect(waitingQueue, SIGNAL(agentMayPass(int)),
			this, SLOT(onAgentMayPassQueue(int)));
		disconnect(waitingQueue, SIGNAL(queueEndPositionChanged(double,double)),
			this, SLOT(onQueueEndPositionChanged(double,double)));
	}
	else if((followedAgent != nullptr) && (id == followedAgent->getId())) {
		// followed agent leaves queue
		onFollowedAgentLeftQueue();
	}
}

void QueueingWaypointPlanner::onFollowedAgentLeftQueue() {
	// followed agent leaves queue
	// → remove all connections to old followed agent
	disconnect(followedAgent, SIGNAL(positionChanged(double,double)),
		this, SLOT(onFollowedAgentPositionChanged(double,double)));

	// → move to queue's front
	//HACK: actually we have to check our position and eventually bind to a new followed agent
	Ped::Tvector queueingPosition = waitingQueue->getPosition();
	currentWaypoint->setPosition(queueingPosition);
}

void QueueingWaypointPlanner::onQueueEndPositionChanged(double xIn, double yIn) {
	// there's nothing to do when the agent is already enqueued
	if(status != QueueingWaypointPlanner::Approaching)
		return;

	if(hasReachedQueueEnd()) {
		// change mode
		activateQueueingMode();
	}
	else {
		// don't update if the planner hasn't determined waypoint yet
		if(currentWaypoint == nullptr)
			return;

		// update destination
		Ped::Tvector newDestination(xIn, yIn);
		if(!waitingQueue->isEmpty())
			addPrivateSpace(newDestination);
		currentWaypoint->setPosition(newDestination);
	}
}

void QueueingWaypointPlanner::reset() {
	// disconnect signals
	if(followedAgent != nullptr) {
		disconnect(followedAgent, SIGNAL(positionChanged(double,double)),
			this, SLOT(onFollowedAgentPositionChanged(double,double)));
	}
	if(waitingQueue != nullptr) {
		disconnect(waitingQueue, SIGNAL(agentMayPass(int)),
			this, SLOT(onAgentMayPassQueue(int)));
		disconnect(waitingQueue, SIGNAL(queueEndPositionChanged(double,double)),
			this, SLOT(onQueueEndPositionChanged(double,double)));
	}

	// unset variables
	status = QueueingWaypointPlanner::Unknown;
	delete currentWaypoint;
	currentWaypoint = nullptr;
	followedAgent = nullptr;
}

Agent* QueueingWaypointPlanner::getAgent() const {
	return agent;
}

bool QueueingWaypointPlanner::setAgent(Agent* agentIn) {
	agent = agentIn;
	return true;
}

void QueueingWaypointPlanner::setDestination(Waypoint* waypointIn) {
	WaitingQueue* queue = dynamic_cast<WaitingQueue*>(waypointIn);

	// sanity checks
	if(queue == nullptr) {
		ERROR_LOG("Waypoint provided to QueueingWaypointPlanner isn't a waiting queue! (%1)",
			(waypointIn==nullptr)?"null":waypointIn->toString());
		return;
	}

	// apply new destination
	setWaitingQueue(queue);
}

void QueueingWaypointPlanner::setWaitingQueue(WaitingQueue* queueIn) {
	// clean up old waiting queue
	reset();

	// set up for new waiting queue
	waitingQueue = queueIn;
	if(waitingQueue != nullptr) {
		status = QueueingWaypointPlanner::Approaching;
		// connect signals
		connect(waitingQueue, SIGNAL(agentMayPass(int)),
			this, SLOT(onAgentMayPassQueue(int)));
		connect(waitingQueue, SIGNAL(queueEndPositionChanged(double,double)),
			this, SLOT(onQueueEndPositionChanged(double,double)));
	}
}

WaitingQueue* QueueingWaypointPlanner::getWaitingQueue() const {
	return waitingQueue;
}

bool QueueingWaypointPlanner::hasReachedQueueEnd() const {
	const double endPositionRadius = 2.0;

	// sanity checks
	if(waitingQueue == nullptr)
		return false;

	Ped::Tvector queueEnd = waitingQueue->getQueueEndPosition();
	Ped::Tvector diff = queueEnd - agent->getPosition();
	
	if(diff.length() <= endPositionRadius)
		return true;
	else
		return false;
}

void QueueingWaypointPlanner::activateApproachingMode() {
	DEBUG_LOG("Agent %1 enters Approaching Mode", agent->getId());

	// update mode
	status = QueueingWaypointPlanner::Approaching;

	// set new waypoint
	QString waypointName = createWaypointName();
	Ped::Tvector destination = waitingQueue->getQueueEndPosition();

	// reset waypoint (remove old one)
	delete currentWaypoint;
	currentWaypoint = new QueueingWaypoint(waypointName, destination);
}

void QueueingWaypointPlanner::activateQueueingMode() {
	DEBUG_LOG("Agent %1 enters Queueing Mode", agent->getId());

	// update mode
	status = QueueingWaypointPlanner::Queued;

	// set new waypoint
	QString waypointName = createWaypointName();
	Ped::Tvector queueingPosition;
	followedAgent = waitingQueue->enqueueAgent(agent);
	if(followedAgent != nullptr) {
		queueingPosition = followedAgent->getPosition();
		addPrivateSpace(queueingPosition);

		// keep updating the waypoint
		connect(followedAgent, SIGNAL(positionChanged(double,double)),
			this, SLOT(onFollowedAgentPositionChanged(double,double)));
	}
	else {
		queueingPosition = waitingQueue->getPosition();
	}

	// deactivate problematic forces
	agent->disableForce("Social");
	agent->disableForce("Random");
	agent->disableForce("GroupCoherence");
	agent->disableForce("GroupGaze");

	// reset waypoint (remove old one)
	delete currentWaypoint;
	currentWaypoint = new QueueingWaypoint(waypointName, queueingPosition);
}

void QueueingWaypointPlanner::addPrivateSpace(Ped::Tvector& queueEndIn) const {
	const double privateSpaceDistance = 0.7;
	Ped::Tvector queueOffset(Ped::Tvector::fromPolar(waitingQueue->getDirection(), privateSpaceDistance));
	queueEndIn -= queueOffset;
}

QString QueueingWaypointPlanner::createWaypointName() const {
	return QString("QueueHelper_A%1_Q%2").arg(agent->getId()).arg(waitingQueue->getName());
}

Waypoint* QueueingWaypointPlanner::getCurrentWaypoint() {
	if(hasCompletedWaypoint())
		currentWaypoint = getNextWaypoint();

	return currentWaypoint;
}

Waypoint* QueueingWaypointPlanner::getNextWaypoint() {
	// sanity checks
	if(agent == nullptr) {
		ERROR_LOG("Cannot determine queueing waypoint without agent!");
		return nullptr;
	}
	if(waitingQueue == nullptr) {
		WARN_LOG("Cannot determine queueing waypoint without waiting queues!");
		return nullptr;
	}

	// set mode
	if(hasReachedQueueEnd())
		activateQueueingMode();
	else
		activateApproachingMode();

	return currentWaypoint;
}

bool QueueingWaypointPlanner::hasCompletedWaypoint() const {
	if(currentWaypoint == nullptr)
		return true;

	// update waypoint, if necessary
	if(status == QueueingWaypointPlanner::Approaching) {
		if(hasReachedQueueEnd()) {
			return true;
		}
	}

	// check whether agent has reached waypoint
	return (status == QueueingWaypointPlanner::MayPass);
}

bool QueueingWaypointPlanner::hasCompletedDestination() const {
	if(waitingQueue == nullptr) {
		WARN_LOG("QueueingWaypointPlanner: No waiting queue set!");
		return true;
	}

	// check whether agent has reached waypoint
	return (status == QueueingWaypointPlanner::MayPass);
}

QString QueueingWaypointPlanner::name() const {
	return tr("QueueingWaypointPlanner");
}

QString QueueingWaypointPlanner::toString() const {
	return tr("%1 (%2)").arg(name()).arg((waitingQueue==nullptr)?"null":waitingQueue->toString());
}
