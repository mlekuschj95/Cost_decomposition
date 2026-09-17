#include "Worker.h"
//#include "Windows.h"
#include <iterator>



Worker::Worker() {
	workerId_ = 0;
	costs_ = 0.0;
	stages_ = {};
	earliest_available_ = 0;
	start_time_ = 0;
	finish_time_ = 0;

	
}
Worker::Worker(int id, int costs, const vector<int> &stages, vector<pair<int, int>> &idle_time) {
	workerId_ = id;
	costs_ = costs;
	stages_ =  stages;
	earliest_available_ = 0;
	start_time_ = 0;
	finish_time_ = 0;
	idle_time_ = idle_time;
}

void Worker::schedule_worker(int start_time, int duration)
{ // vector idle_time originally conists of a only ones representing the availability of intervalls 

	for (auto it = idle_end() - 1; it != idle_begin(); it--) {
		if (get<0>(*it) <= start_time && get<1>(*it) >= start_time + duration) {
			if (get<0>(*it) == start_time) {
				get<0>(*it) = start_time + duration;
				if (earliest_available_ == start_time) {
					if (start_time + duration == get<1>(*it)) {
						earliest_available_ = get<0>(*(it - 1));
						idle_time_.erase(it);
						return;
					}
					else {
						earliest_available_ = start_time + duration;
						return;
					}
				}
				return;
			}
			else if (get<1>(*it) == start_time + duration) {
				get<1>(*it) = start_time;
				return;
			}
			else {
				int end = get<1>(*it);
				get<1>(*it) = start_time;
				if (get<1>(*(it - 1)) == 0) {
					get<0>(*(it - 1)) = start_time + duration;
					get<1>(*(it - 1)) = end;
				}
				else {
					pair<int,int> element = make_pair(start_time + duration, end);
					idle_time_.insert(it, element);
				}
				return;
			}
		}
	}
}

void Worker::remove_interval_right(int duration, int end) {
	for (auto it = idle_end() - 1; it != idle_begin(); it--) {
		if (get<0>(*it) == end) {
			if (it != idle_end() - 1) {
				if (get<1>(*(it + 1)) == end - duration) {
					get<0>(*it) = get<0>(*(it + 1));
					idle_time_.erase(it + 1);
				}
				else {
					get<0>(*it) = end - duration;
					if (earliest_available_ == end) {
						earliest_available_ = end - duration;
					}
				}
			}
			else {
				get<0>(*it) = end - duration;
				if (earliest_available_ == end) {
					earliest_available_ = end - duration;
				}
			}
			return;
		}
	}
}

void Worker::remove_interval_left(int duration, int start_time, int end) {
	for (auto it = idle_end() - 1; it != idle_begin(); it--) {
		if (get<1>(*it) == start_time) {
			if (get<0>(*(it - 1)) == start_time + duration) {
				get<1>(*it) = get<1>(*(it - 1));
				idle_time_.erase(it - 1);
			}
			else {
				get<1>(*it) = start_time + duration;
				if (get<0>(*(it - 1)) == start_time) {
					get<0>(*(it - 1)) = start_time + duration;
				}
			}
			return;
		}
		if (get<0>(*it) >= end) {
			if (get<0>(*it) == end) {
				if (it != idle_end() - 1) {
					if (get<1>(*(it - 1)) == end - duration) {
						get<0>(*it) = get<0>(*(it - 1));
						idle_time_.erase(it - 1);
					}
					else {
						get<0>(*it) = end - duration;
						if (earliest_available_ == end) {
							earliest_available_ = end - duration;
						}
					}
				}
				else {
					get<0>(*it) = end - duration;
					if (earliest_available_ == end) {
						earliest_available_ = end - duration;
					}
				}
				return;
			}
			else {
				pair<int, int> element = make_pair(start_time, start_time + duration);
				if (earliest_available_ == get<0>(*it)) {
					earliest_available_ = start_time;
				}
				idle_time_.insert(it + 1, element);

			}
			return;
		}
	}
}


ostream& operator<<(ostream& os, const Worker& worker) {
	os << " Worker "<< worker.Id() <<  " with costs " << worker.costs() << " skilled for stages ";
    copy(worker.stage_begin(), worker.stage_end(), ostream_iterator<int>(cout, " "));
    return os;
}