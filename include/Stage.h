#ifndef _STAGE_H_
#define _STAGE_H_

#include <vector>
#include <iostream> 
#include "Machine.h"
#include "Worker.h"


using namespace std;

class Stage {
protected:
	// The stage number
	int stageId_;
	// The number of parallel machines at the stage
	int number_machines_;
	// The number of workers who are able to work at that stage
	int number_workers_;
	// The machines
	vector<Machine> machines_;
	// The Worker Id´s
	vector<int> workers_;

public:
	// constructor
	Stage(int Id, int nmachines, int nworkers, vector<Machine>& machines, const vector<int>& workers);

	//default constructor
	Stage();

	// access stage elements 
	int stageId() const { return stageId_; };
	int number_machines() const { return number_machines_; };
	int number_workers() const { return number_workers_; };

	// access machine, workers at stage
	vector<Machine>::iterator machine_begin() { return machines_.begin(); }
	vector<Machine>::iterator machine_end() { return machines_.end(); }
	vector<Machine>::const_iterator machine_begin() const { return machines_.begin(); }
	vector<Machine>::const_iterator machine_end() const { return machines_.end(); }
	vector<int>::iterator worker_begin() { return workers_.begin(); }
	vector<int>::iterator worker_end() { return workers_.end(); }
	vector<int>::const_iterator worker_begin() const { return workers_.begin(); }
	vector<int>::const_iterator worker_end() const { return workers_.end(); }
};
ostream& operator << (ostream& os, const Stage& stage);

#endif

