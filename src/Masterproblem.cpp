#include "Masterproblem.h"

MASTER_Model::~MASTER_Model()
{
    env_.end();
}

MASTER_Model::MASTER_Model(
    const DRCRFFSP_Instance& instance,
    double cmax_epsilon,
    double time_limit_seconds)
{
    try {

        n_ = instance.n();
        s_ = instance.s();
        o_ = instance.o();
        w_ = instance.w();

        cout << "Operations = " << o_ << endl;
        cout << "Workers    = " << w_ << endl;
        cout << "Binary vars = " << o_ * w_ << endl;

        // IMPORTANT: create the Concert model
        model_ = IloModel(env_);

        for (auto it = instance.workers_begin();
             it != instance.workers_end();
             ++it) {

            worker_costs_.push_back(it->costs());
        }

        // ------------------------------------------------------------
        // Assignment variables
        // ------------------------------------------------------------

        x_ = IloArray<IloBoolVarArray>(env_, o_);

        for (int i = 0; i < o_; ++i) {

            x_[i] = IloBoolVarArray(env_, w_);

            for (int w = 0; w < w_; ++w) {

                string name =
                    to_string(i) + "_" + to_string(w);

                x_[i][w].setName(name.c_str());
            }
        }

        // ------------------------------------------------------------
        // Exactly one eligible worker per operation
        // ------------------------------------------------------------

        for (int i = 0; i < o_; ++i) {

            IloExpr assignment(env_);

            for (int w = 0; w < w_; ++w) {

                if (instance.is_worker_eligible(i, w)) {

                    assignment += x_[i][w];
                }
                else {

                    model_.add(x_[i][w] == 0);
                }
            }

            model_.add(assignment == 1);
            assignment.end();
        }

        // ------------------------------------------------------------
        // Assignment cost
        // ------------------------------------------------------------

        assignment_cost_ = IloExpr(env_);

        for (int i = 0; i < o_; ++i) {

            double p_i =
                instance.get_processing_time(i);

            for (int w = 0; w < w_; ++w) {

                if (instance.is_worker_eligible(i, w)) {

                    assignment_cost_
                        += worker_costs_[w]
                        * p_i
                        * x_[i][w];
                }
            }
        }
        // ------------------------------------------------------------
        // Epsilon-constraint necessary workload bound (only when active):
        //     sum_i p_i * x_[i][w] <= cmax_epsilon   for every worker w
        // NECESSARY, not sufficient -- see the constructor's doc comment
        // in Masterproblem.h. Does not touch the objective.
        // ------------------------------------------------------------

        if (cmax_epsilon > 0) {

            cout << "Epsilon-constraint mode: Cmax <= " << cmax_epsilon
                 << " (adding worker workload bounds to master)" << endl;

            for (int w = 0; w < w_; ++w) {

                IloExpr worker_load(env_);

                for (int i = 0; i < o_; ++i) {

                    if (instance.is_worker_eligible(i, w)) {

                        double p_i = instance.get_processing_time(i);
                        worker_load += p_i * x_[i][w];
                    }
                }

                model_.add(worker_load <= cmax_epsilon);
                worker_load.end();
            }
        }

        // ------------------------------------------------------------
        // General symmetry breaking for equivalent workers
        // ------------------------------------------------------------

        // Build groups of equivalent workers
        vector<vector<int>> symmetry_groups;

        for (int w = 0; w < w_; ++w) {

            bool inserted = false;

            for (auto& group : symmetry_groups) {

                int representative = group.front();

                bool equivalent = true;

                // Same cost
                if (worker_costs_[w] != worker_costs_[representative]) {
                    equivalent = false;
                }

                // Same eligibility pattern over all operations
                if (equivalent) {

                    for (int i = 0; i < o_; ++i) {

                        bool eligible_w =
                            instance.is_worker_eligible(i, w);

                        bool eligible_rep =
                            instance.is_worker_eligible(i, representative);

                        if (eligible_w != eligible_rep) {
                            equivalent = false;
                            break;
                        }
                    }
                }

                if (equivalent) {

                    group.push_back(w);
                    inserted = true;
                    break;
                }
            }

            if (!inserted) {
                symmetry_groups.push_back({w});
            }
        }

        // ------------------------------------------------------------
        // Lex-leader symmetry breaking for equivalent workers.
        //
        // Replaces the earlier load(w1) >= load(w2) constraint (which
        // only compared TOTAL assigned processing time, so two
        // assignments giving equivalent workers different specific
        // operations with the same total duration were indistinguishable
        // to it) with a full lexicographic ordering of the two workers'
        // assignment vectors, restricted to their shared eligible
        // operations E = {i_1 < ... < i_m} (all workers in a group share
        // the same eligibility pattern by construction, so E is the same
        // for every pair in the group).
        //
        // For consecutive pair (w1, w2), let a_t = x_[i_t][w1],
        // b_t = x_[i_t][w2]. We want a >=_lex b: at the first position
        // where they differ, w1 must be the one assigned (a_t=1,b_t=0).
        //
        // Auxiliary binaries per position t = 0,...,m-1:
        //   e_t : exact indicator of (a_t == b_t)
        //       e_t <= 1 - a_t + b_t
        //       e_t <= 1 + a_t - b_t
        //       e_t >= a_t + b_t - 1
        //       e_t >= 1 - a_t - b_t
        //   q_t : exact indicator of "equal on positions 0..t"
        //       q_0 := e_0 (since the implicit q_{-1} = 1, AND(1,e_0)=e_0)
        //       for t >= 1: q_t = AND(q_{t-1}, e_t):
        //           q_t <= q_{t-1}
        //           q_t <= e_t
        //           q_t >= q_{t-1} + e_t - 1
        //
        // Main lex constraint, for every t = 0,...,m-1 (with q_{-1} := 1):
        //       a_t >= b_t + q_{t-1} - 1
        // (vacuous once q_{t-1}=0, i.e. once the ordering is already
        // decided at an earlier position -- see conversation for a
        // worked numeric example and why a simpler "prefix-sum
        // dominance" shortcut is NOT sound.)
        // ------------------------------------------------------------

        for (const auto& group : symmetry_groups) {

            if (group.size() <= 1)
                continue;

            // Shared eligible-operation set for this group (same for
            // every worker in it, by the grouping criterion above).
            int representative = group.front();
            vector<int> eligible_ops;

            for (int i = 0; i < o_; ++i) {
                if (instance.is_worker_eligible(i, representative)) {
                    eligible_ops.push_back(i);
                }
            }

            int m = static_cast<int>(eligible_ops.size());

            if (m == 0) {
                continue;
            }

            for (size_t g = 0; g + 1 < group.size(); ++g) {

                int w1 = group[g];
                int w2 = group[g + 1];

                vector<IloBoolVar> q(m);

                for (int t = 0; t < m; ++t) {

                    int i = eligible_ops[t];

                    IloBoolVar& a_t = x_[i][w1];
                    IloBoolVar& b_t = x_[i][w2];

                    string e_name =
                        "e_" + to_string(w1) + "_" + to_string(w2)
                        + "_" + to_string(t);

                    IloBoolVar e_t(env_, e_name.c_str());

                    model_.add(e_t <= 1 - a_t + b_t);
                    model_.add(e_t <= 1 + a_t - b_t);
                    model_.add(e_t >= a_t + b_t - 1);
                    model_.add(e_t >= 1 - a_t - b_t);

                    if (t == 0) {

                        // q_{-1} = 1 (constant), so q_0 = AND(1, e_0) = e_0.
                        q[0] = e_t;

                        // Main constraint at t=0: a_0 >= b_0 + 1 - 1 = b_0.
                        model_.add(a_t >= b_t);
                    }
                    else {

                        string q_name =
                            "q_" + to_string(w1) + "_" + to_string(w2)
                            + "_" + to_string(t);

                        IloBoolVar q_t(env_, q_name.c_str());

                        model_.add(q_t <= q[t - 1]);
                        model_.add(q_t <= e_t);
                        model_.add(q_t >= q[t - 1] + e_t - 1);

                        q[t] = q_t;

                        // Main constraint: a_t >= b_t + q_{t-1} - 1.
                        model_.add(a_t >= b_t + q[t - 1] - 1);
                    }
                }
            }
        }
    

        // ------------------------------------------------------------
        // Objective
        // ------------------------------------------------------------

        model_.add(
            IloMinimize(env_, assignment_cost_)
        );

        // ------------------------------------------------------------
        // Create CPLEX solver
        // ------------------------------------------------------------

        cplex_ = IloCplex(model_);
        cplex_.setOut(env_.getNullStream());

        // The decomposition treats every solve_master() objective value as
        // a PROVEN exact optimum (a valid global WWS lower bound, and the
        // basis for candidate_lb certification). CPLEX's default relative
        // MIP gap (1e-4) would otherwise let it stop early and report a
        // solution within ~0.01% of optimal as "solved" -- for objective
        // values in the thousands, that is slack enough to be off by more
        // than 1, which is fatal for integer-level reasoning (a level can
        // appear to go up, or even down across re-solves, if two different
        // near-optimal-but-not-exact values are reported for cut sets of
        // different sizes). Requiring an exact solve removes that risk.
        cplex_.setParam(IloCplex::Param::MIP::Tolerances::MIPGap, 0.0);
        cplex_.setParam(IloCplex::Param::MIP::Tolerances::AbsMIPGap, 0.0);

        // Fixed thread count / seed for reproducibility and to avoid
        // oversubscribing a single-CPU cluster allocation (see
        // experiments/README.md) -- applies to every MASTER_Model.
        cplex_.setParam(IloCplex::Param::Threads, 1);
        cplex_.setParam(IloCplex::Param::RandomSeed, 1);

        if (time_limit_seconds > 0) {
            // Standalone LB_AP/LB_AP_EPS experiment use only -- see the
            // doc comment on this parameter in Masterproblem.h.
            cplex_.setParam(IloCplex::Param::TimeLimit, time_limit_seconds);
        }

        cout << "MASTER model successfully created." << endl;
    }
    catch (IloException& e) {

        cerr << "Error creating MASTER model: "
             << e.getMessage()
             << endl;
    }
}

  

vector<int> MASTER_Model::solve_assignment(
    const DRCRFFSP_Instance& instance)
{
    vector<int> worker_assignment(o_, -1);

    if (cplex_.solve()) {

        cout << "\n--- MASTER WORKER ASSIGNMENT ---\n";

        for (int i = 0; i < o_; ++i) {

            for (int w = 0; w < w_; ++w) {

                if (cplex_.getValue(x_[i][w]) > 0.5) {

                    worker_assignment[i] = w;

                    cout << "operation " << i
                         << " -> worker "
                         << (instance.workers_begin() + w)->Id()
                         << endl;

                    break;
                }
            }
        }
    }
    else {

        cout << "No feasible master solution found." << endl;
    }

    return worker_assignment;
}

double MASTER_Model::get_assignment_cost() const
{
    return cplex_.getObjValue();
}

MasterSolution MASTER_Model::solve_master()
{
    MasterSolution result;
    result.feasible = false;
    result.objective_value = -1.0;
    result.worker_assignment.assign(o_, -1);
    result.proven_optimal = false;

    if (cplex_.solve()) {

        result.feasible = true;
        result.objective_value = cplex_.getObjValue();
        result.proven_optimal = (cplex_.getStatus() == IloAlgorithm::Optimal);

        for (int i = 0; i < o_; ++i) {

            for (int w = 0; w < w_; ++w) {

                if (cplex_.getValue(x_[i][w]) > 0.5) {

                    result.worker_assignment[i] = w;
                    break;
                }
            }
        }
    }
    else {

        cout << "No feasible master solution found." << endl;
    }

    return result;
}

void MASTER_Model::add_no_good_cut(const vector<int>& worker_assignment)
{
    if (worker_assignment.size() != static_cast<size_t>(o_)) {

        throw runtime_error(
            "add_no_good_cut: worker_assignment size does not match "
            "number of operations"
        );
    }

    IloExpr assignment_sum(env_);

    for (int i = 0; i < o_; ++i) {

        int w = worker_assignment[i];

        if (w < 0 || w >= w_) {

            assignment_sum.end();

            throw runtime_error(
                "add_no_good_cut: invalid worker index for operation "
                + to_string(i)
            );
        }

        assignment_sum += x_[i][w];
    }

    // sum_i x_[i][worker_assignment[i]] <= o_ - 1
    // Concert/CPLEX dynamically re-extract model_ before the next
    // cplex_.solve() call, so this constraint is picked up automatically.
    model_.add(assignment_sum <= o_ - 1);

    assignment_sum.end();
}

vector<MasterSolution> MASTER_Model::solve_master_pool(int max_solutions)
{
    vector<MasterSolution> solutions;

    // Pool::Intensity = 4: aim for an aggressively exhaustive enumeration
    // of qualifying solutions (still not a guarantee of finding all of
    // them -- see the header comment). Pool::AbsGap/RelGap = 0: only keep
    // solutions at the exact optimal objective, matching the exact
    // MIPGap = 0 already required for a single solve_master() call.
    cplex_.setParam(IloCplex::Param::MIP::Pool::Intensity, 4);
    cplex_.setParam(IloCplex::Param::MIP::Pool::AbsGap, 0.0);
    cplex_.setParam(IloCplex::Param::MIP::Pool::RelGap, 0.0);
    cplex_.setParam(IloCplex::Param::MIP::Pool::Capacity, max_solutions);
    cplex_.setParam(IloCplex::Param::MIP::Limits::Populate, max_solutions);

    if (!cplex_.populate()) {

        cout << "No feasible master solution found (populate)." << endl;
        return solutions;
    }

    IloInt pool_size = cplex_.getSolnPoolNsolns();

    for (IloInt s = 0; s < pool_size; ++s) {

        MasterSolution result;
        result.feasible = true;
        result.objective_value = cplex_.getObjValue(s);
        result.worker_assignment.assign(o_, -1);
        // Pool solutions are restricted to Pool::AbsGap = Pool::RelGap =
        // 0.0 (see this method's header comment), the same exactness
        // guarantee as a plain solve_master() with MIPGap = 0.
        result.proven_optimal = true;

        for (int i = 0; i < o_; ++i) {

            for (int w = 0; w < w_; ++w) {

                if (cplex_.getValue(x_[i][w], s) > 0.5) {

                    result.worker_assignment[i] = w;
                    break;
                }
            }
        }

        solutions.push_back(result);
    }

    return solutions;
}