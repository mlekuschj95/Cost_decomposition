#ifndef _OPERATION_H_
#define _OPERATION_H_

#include <vector>
#include <iostream> 



using namespace std;

class Operation {
protected:

	// Jobid
	int jobId_;
	// Operationid
	int operationId_;
	// Level of that operation
	int level_;
	// Duration
	float duration_;
	// StageId
	int stageId_;
	// MachineId
	int machine_;
	//Starting time of the Operation
	float startingTime_;
	//Finishing time of the Operation
	float finishingTime_;
	// Earliest starting time = finishing time of its predecessor
	float earliest_starting_time_;
	
public:
	//default constructor
	Operation();

	//constructor
	Operation(int id, int jobId, float duration, int stage , int level);

	// access Operation elements
	int operationId() const { return operationId_; };
	float duration() const { return duration_; };
	int jobId() const { return jobId_; };
	int stageId() const { return stageId_; };
	int machineid() const { return machine_; };
	float start()  { return startingTime_; };
	float finish()  { return finishingTime_; };
	float earliest() const { return earliest_starting_time_; };
	int level() const { return level_; };

	//setters
	void set_intervall(float start_time);
	void set_earliest_starting_time(float predecessor_starting_time);
	void set_machine(int machineId);	

};
ostream& operator << (ostream& os, const Operation& operation);

#endif

