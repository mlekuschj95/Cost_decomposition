#include "Job.h"

#include <iterator>
#include <climits>

#include <iterator>

Job::Job() {
	jobId_ = 0;
	number_operations_ = 0;
	finishTime_ = 0;
	operations_not_scheduled_ = {};
	operations_scheduled_ = {};
	
}
Job::Job(int id, int number_operations, const vector<Operation> &operations, const vector<int> &not_scheduled ,const vector<int>& scheduled) {
	jobId_ = id;
	number_operations_ = number_operations;
	operations_ = operations;
	operations_not_scheduled_ = not_scheduled;
	operations_scheduled_ = scheduled;
	finishTime_ = 0;

}


ostream& operator<<(ostream& os, const Job& job) {
	os << " For Job " << job.jobId() << " currently finishing at" << job.finishTime(); 
	copy(job.scheduled_begin(), job.scheduled_end(), ostream_iterator<int>(cout, " "));
	os << "are scheduled "; copy(job.not_scheduled_begin(), job.not_scheduled_end(), ostream_iterator<int>(cout, " "));
	os << "are not scheduled yet";
	return os;
}