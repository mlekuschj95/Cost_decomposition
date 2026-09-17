#include "Stage.h"
#include <iterator>
#include <vector>
#include <algorithm>
#include <numeric>

using namespace std;

Stage::Stage(int Id, int nmachines, int nworkers, vector<Machine>& machines, const vector<int>& workers) {
	stageId_ = Id;
	number_machines_ = nmachines;
	number_workers_ = nworkers;
	machines_ = machines;
	workers_ = workers;
};

Stage::Stage() {
	stageId_ = 0;
	number_machines_ = 0;
	number_workers_ = 0;
	machines_ = {};
	workers_ = {};
}



ostream& operator<<(ostream& os, const Stage& stage) {
	os << " Stage " << stage.stageId() << " with machines ";
	copy(stage.machine_begin(), stage.machine_end(), ostream_iterator<Machine>(cout, " "));
	os << "and " << stage.number_workers() << " Workers: "; copy(stage.worker_begin(), stage.worker_end(), ostream_iterator<int>(cout, " "));
	return os;
};