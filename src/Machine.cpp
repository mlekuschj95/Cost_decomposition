#include "Machine.h"



Machine::Machine(int mId, int stId, vector<pair<int, int>>& idle_time)
{
	machineId_ = mId;
	stageId_ = stId;
	earliest_available_ = 0;
	idle_time_ = idle_time;
}


ostream& operator<<(ostream& os, const Machine& machine) {
	os << "Machine " << machine.mId() << " on Stage " << machine.stId();
	return os;
};
