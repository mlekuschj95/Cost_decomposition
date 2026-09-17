#include <fstream>   // file stream
#include <sstream>   // string stream used for tokenising
#include <string>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <iostream>
#include "Operation.h"
#include "Machine.h"
#include "DRCRFFSP_Instance.h"
using namespace std;



DRCRFFSP_Instance::DRCRFFSP_Instance(string fname) {
    readInstance(fname);
    //cout << *this << endl;
};

void DRCRFFSP_Instance::readInstance(string fname) {
    string line;
    ifstream in_file(fname.c_str());
    int count_line = 0;
    int operationId = 1;
    filename_ = fname;
    if (!in_file.is_open()) {
        cerr << "can't open file: " << fname << endl;
        exit(2);
    }
    while (!in_file.eof()) {
        getline(in_file, line);
        stringstream tok(line);
        switch (count_line) {
        case 0:
            // first line = jobs, stages, machines, workers, reentrances, operations, sum processing time
            tok >> n_;
            tok >> s_;
            tok >> m_;
            tok >> w_;
            tok >> r_;
            tok >> o_;
            tok >> sumpr_;
            processing_times_per_stage_ = vector<float>(s_, 0);
            break;
        default:
            // Read in all Stage information 
            // One Line : 
                    //Number of m machines at stage, number of w workers at stage, m machine ids, w worker ids 
            if (count_line > n_ + s_ + w_) break;
            if (count_line <= s_) {
                int stageId = count_line;
                int number_machines;
                int number_workers;
                tok >> number_machines;
                tok >> number_workers;
                vector<Machine> machines;
                vector<int> workers(number_workers);
                // Vector of idle intervalls 
                vector<pair<int, int>> idle_times(n_ * (r_ + 1));
                idle_times[n_ * (r_ + 1) - 1] = make_pair(0, sumpr_);
                for (int i = 0; i < number_machines; i++)
                {
                    int machineId;
                    tok >> machineId;
                    Machine machine(machineId, stageId, idle_times);
                    machines.push_back(machine);

                }

                for (int i = 0; i < number_workers; i++)
                {
                    int worker_id;
                    tok >> worker_id;
                    workers[i] = worker_id;
                }
                Stage stage(stageId, number_machines, number_workers, machines, workers);
                stages_.push_back(stage);
            }
            // Read in all Job Information 
            // One Line : 
            //      Number of o Operations, o pairs(stage,duration) 
            else if (count_line <= s_ + n_)
            {
                int jobId = count_line - s_;
                int number_operations;
                int duration;
                int stage;
                int prev_stage = -1;
                int level = 0;
                tok >> number_operations;
                vector<int> not_scheduled(number_operations);
                vector<int> scheduled(number_operations);
                vector<Operation> operations;
                operations.reserve(number_operations);
                for (int i = 0; i < number_operations; i++) {
                    tok >> stage;
                    tok >> duration;
                    if (stage <= prev_stage) {
                        level += 1;
                    }
                    prev_stage = stage;
                    if (duration != 0) {
                        not_scheduled[number_operations - i - 1] = (operationId);
                        Operation operation(operationId, jobId, duration, stage, level);
                        operations.push_back(operation);
                        processing_times_per_stage_[stage - 1] += duration;
                        operationId += 1;
                    }
                    else {
                        i--;
                    }
                }
                Job job(jobId, number_operations, operations, not_scheduled, scheduled);
                jobs_.push_back(job);
            }
            // Read in all Worker Information
            // One Line:
            //      Worker id, Number of s stages he is skilled for, s stage id�s
            else {
                int workerid;
                int number_stages;
                tok >> workerid;
                tok >> number_stages;
                float costs_fl = 1 + (float)(number_stages - 1) / (float)(s_ - 1);
                //int costs = (int)round(costs_fl * 100);
                int costs = s_ - 1 + (number_stages - 1);
                vector<int> stages(number_stages);
                vector<pair<int, int>> idle_times(n_ * number_stages * (r_ + 1));
                idle_times[n_ * number_stages * (r_ + 1) - 1] = make_pair(0, sumpr_);
                for (int i = 0; i < number_stages; i++) {
                    int stage;
                    tok >> stage;
                    stages[i] = (stage);
                }
                Worker worker(workerid, costs, stages, idle_times);
                workers_.push_back(worker);

            }

            break;
        }
        count_line++;

    }

}
const Operation& DRCRFFSP_Instance::operation(int index) const
{
    int counter = 0;

    for (const auto& job : jobs_) {

        for (auto it = job.operations_begin();
             it != job.operations_end();
             ++it) {

            if (counter == index) {
                return *it;
            }

            ++counter;
        }
    }

    throw std::out_of_range("Invalid operation index");
}


float DRCRFFSP_Instance::get_processing_time(int index) const
{
    return operation(index).duration();
}


bool DRCRFFSP_Instance::is_worker_eligible(
    int operation_index,
    int worker_index) const
{
    if (worker_index < 0 ||
        worker_index >= static_cast<int>(workers_.size())) {

        return false;
    }

    const Operation& op = operation(operation_index);

    int required_stage = op.stageId();

    const Worker& worker = workers_[worker_index];

    for (auto it = worker.stage_begin();
         it != worker.stage_end();
         ++it) {

        if (*it == required_stage) {
            return true;
        }
    }

    return false;
}



ostream& operator<<(ostream& os, const DRCRFFSP_Instance& instance) {
    os << "DRCRFFS instance loaded from " << instance.filename();
    //<< " with " << instance.n() << " jobs " << "and " << instance.m() << " machines,  " << instance.s() << " Stages, "<< instance.w() << " Workers and " << instance.r() << " reentrance(s)";
    return os;
};
