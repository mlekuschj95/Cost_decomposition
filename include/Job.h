#ifndef _JOB_H_
#define _JOB_H_

#include <vector>
#include <iostream> 
#include "Operation.h"
using namespace std;

class Job {
protected:
	// Jobid 
	int jobId_;
	// Number of Operations of that Job
	int number_operations_;
	// Ordered Operations
	vector<Operation> operations_;
	// Vector containing all Operations that haven´t been scheduled yet
	vector<int> operations_not_scheduled_;
	// Vector containing all Operations that are already scheduled
	vector<int> operations_scheduled_;
	// Finishing time of that Job 
	float finishTime_;
public:
	//default constructor
	Job();

	//constructor
	Job(int id, int number_operations, const vector<Operation> &operations, const vector<int> &not_scheduled, const vector<int>& scheduled);
	
	// access Job elements
	int jobId() const { return jobId_; };
	float finishTime() const { return finishTime_; };
	int number_operations() const { return number_operations_; };
	vector<int> scheduled() const { return operations_scheduled_; };
	vector<Operation> op() { return operations_; };

	// access Operations 
	vector<int>::iterator not_scheduled_begin() { return operations_not_scheduled_.begin(); };
	vector<int>::const_iterator not_scheduled_begin() const { return operations_not_scheduled_.begin(); };
	vector<int>::iterator not_scheduled_end() { return operations_not_scheduled_.end(); };
	vector<int>::const_iterator not_scheduled_end() const { return operations_not_scheduled_.end(); };
	vector<int>::iterator scheduled_begin() { return operations_scheduled_.begin(); };
	vector<int>::const_iterator scheduled_begin() const { return operations_scheduled_.begin(); };
	vector<int>::iterator scheduled_end() { return operations_scheduled_.end(); };
	vector<int>::const_iterator scheduled_end() const { return operations_scheduled_.end(); };
	vector<Operation>::iterator operations_begin() { return operations_.begin(); };
	vector<Operation>::const_iterator operations_begin() const { return operations_.begin(); };
	vector<Operation>::iterator operations_end() { return operations_.end(); };
	vector<Operation>::const_iterator operations_end() const { return operations_.end(); };
	

};
ostream& operator << (ostream& os, const Job& job);

#endif
