#include "Operation.h"
#include <iterator>

Operation::Operation() {
	operationId_ = 0;
	jobId_ = 0;
	duration_ = 0;
	stageId_ = 0;
	level_ = 0;
	startingTime_ = 0;
	finishingTime_ = 0;
	earliest_starting_time_ = 0;
	machine_ = 0;
}

Operation::Operation(int id, int jobId, float duration, int stage, int level) {
	operationId_ = id;
	jobId_ = jobId;
	duration_ = duration;
	stageId_ = stage;
	level_ = level;
	startingTime_ = 0;
	finishingTime_ = 0;
	earliest_starting_time_ = 0;
	machine_ = 0;
}


void Operation::set_intervall(float start_time) {
	startingTime_ = start_time;
	finishingTime_ = start_time + duration_;
}

void Operation::set_earliest_starting_time(float predecessor_starting_time)
{
	earliest_starting_time_ = predecessor_starting_time;
}

void Operation::set_machine(int machineId) {
	machine_ = machineId;
}




ostream& operator<<(ostream& os, const Operation& operation) {
	os << " Operation " << operation.operationId() << " of Job " << operation.jobId() <<
		" with processing time " << operation.duration() << " can be processed on stage " << operation.stageId();
	return os;
}