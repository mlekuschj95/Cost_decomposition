#ifndef _MACHINE_H_
#define _MACHINE_H_
#include <vector>
#include <iostream>
#include <tuple>
using namespace std;

class Machine {
protected:
	//Machine Id
	int machineId_;
	//Stage Id
	int stageId_;
	// Vector with idle times
	vector<pair<int, int>> idle_time_;
	// Earliest available time of the machine
	int earliest_available_;

public:

	//constructor
	Machine(int mId, int stId, vector<pair<int, int>>& idle_time);

	//Access Machine elements
	int mId() const { return machineId_; };
	int stId() const { return stageId_; };
	int earliest() const { return earliest_available_; };
	vector<pair<int, int>>::iterator idle_begin() { return idle_time_.begin(); }
	vector<pair<int, int>>::const_iterator idle_begin() const { return idle_time_.begin(); }
	vector<pair<int, int>>::iterator idle_end() { return idle_time_.end(); }
	vector<pair<int, int>>::const_iterator idle_end() const { return idle_time_.end(); }
	//Schedules the next operation on the machine and updates the earliest available time

};


ostream& operator << (ostream& os, const Machine& machine);
#endif // 

