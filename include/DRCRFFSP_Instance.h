#ifndef _DRC_INSTANCE_H_
#define _DRC_INSTANCE_H_

#include "Stage.h"
#include "Job.h"
#include "Worker.h"
#include <string>  
#include <vector>
#include <tuple>
using namespace std;
class DRCRFFSP_Instance {
protected:
	// a name for this instance
	string filename_;
	// number of jobs
	int n_;
	// number of stages
	int s_;
	// number of total machines
	int m_;
	//number of workers
	int w_;
	// number of reentrance
	int r_;
	// number of operations
	int o_;
	// Sum of processing times of all Operations (as upper bound)
	int sumpr_;
	// All stages
	vector<Stage> stages_;
	// All Jobs
	vector<Job> jobs_;
	// All workers;
	vector <Worker> workers_;
	// vector with processing times per stage
	vector<float> processing_times_per_stage_;
	void readInstance(string filename);

public:
	// a constructor that takes the name of the instance file as input
	DRCRFFSP_Instance(string filename);




	// Access Jobs, stages, Workers
	vector<Stage>::iterator stage_begin() { return stages_.begin(); }
	vector<Stage>::iterator stage_end() { return stages_.end(); }
	vector<Stage>::const_iterator stage_begin() const { return stages_.begin(); }
	vector<Stage>::const_iterator stage_end() const { return stages_.end(); }
	vector<Job>::iterator jobs_begin() { return jobs_.begin(); }
	vector<Job>::iterator jobs_end() { return jobs_.end(); }
	vector<Job>::const_iterator jobs_begin() const { return jobs_.begin(); }
	vector<Job>::const_iterator jobs_end() const { return jobs_.end(); }
	vector<Worker>::iterator workers_begin() { return workers_.begin(); }
	vector<Worker>::iterator workers_end() { return workers_.end(); }
	vector<Worker>::const_iterator workers_begin() const { return workers_.begin(); }
	vector<Worker>::const_iterator workers_end() const { return workers_.end(); }
	vector<Job> job() const { return jobs_; };
	vector<Stage> stage() const { return stages_; };
	vector<Worker> worker() const { return workers_; };

	//getters
	int n() const { return n_; };
	int s() const { return s_; };
	int m() const { return m_; };
	int w() const { return w_; };
	int r() const { return r_; };
	int o() const { return o_; };
	int sumpr() const { return sumpr_; };
	vector<float> processing_times_per_stage() const { return processing_times_per_stage_; };
	string filename() const { return filename_; };
		// Helper functions
	const Operation& operation(int index) const;
	float get_processing_time(int index) const;
	bool is_worker_eligible(int operation_index, int worker_index) const;


};

ostream& operator<<(ostream& os, const DRCRFFSP_Instance& instance);



#endif

