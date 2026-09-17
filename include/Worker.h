#ifndef _WORKER_H_
#define _WORKER_H_

#include <vector>
#include <iostream> 
#include <tuple>
using namespace std;

class Worker {
protected:
	//Worker id 
	int workerId_;
	// Worker costs
	int costs_;
	// vector with stageId´s he is skilled for 
	vector<int> stages_;
	// starting time for that worker
	int start_time_;
	// Happy hour 
	int finish_time_;
	// current earliest available time 
	int earliest_available_;
	// vector with idle intervalls 
	vector<pair<int,int>> idle_time_;

public:
	//default constructor
	Worker();

	//constructor
	Worker(int id, int costs, const vector<int> &stages, vector<pair<int, int>>&idle_time);

	// access Worker elements
	int Id() const { return workerId_; };
	int costs() const { return costs_; };
	int earliest() const { return earliest_available_; };
	int start() const { return start_time_; };
	int finish() const { return finish_time_; };
	int skills() const { return stages_.size(); };
	

	// access stages that this worker is skilled for
	vector<int>::iterator stage_begin() { return stages_.begin(); }
	vector<int>::const_iterator stage_begin() const { return stages_.begin(); }
	vector<int>::iterator stage_end() { return stages_.end(); }
	vector<int>::const_iterator stage_end() const { return stages_.end(); }
	vector<pair<int, int>>::iterator idle_begin() { return idle_time_.begin(); }
	vector<pair<int, int>>::const_iterator idle_begin() const { return idle_time_.begin(); }
	vector<pair<int, int>>::iterator idle_end() { return idle_time_.end(); }
	vector<pair<int, int>>::const_iterator idle_end() const { return idle_time_.end(); }

	// schedules an operation for this worker 
	void schedule_worker(int start_time, int duration);
	void remove_interval_right(int duration, int end);
	void remove_interval_left(int duration, int start, int end);
};
ostream& operator << (ostream& os, const Worker& worker);

#endif
