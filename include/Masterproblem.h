#ifndef _MASTER_MODEL_H
#define _MASTER_MODEL_H

#include <ilcplex/ilocplex.h>
#include "DRCRFFSP_Instance.h"

using namespace std;

// Result of one MASTER_Model solve: worker assignment together with its
// productive-cost objective value, so callers get both from a single
// CPLEX solve instead of solving twice.
struct MasterSolution {
    bool feasible;
    double objective_value;
    vector<int> worker_assignment;

    // True iff CPLEX's status for this solve was Optimal (a genuine proven
    // exact optimum), as opposed to Feasible (e.g. a time-limited,
    // non-exact incumbent -- only possible when the MASTER_Model was
    // constructed with a time_limit_seconds; every master Decomposition
    // constructs itself is unbounded and MIPGap=0, so this is always true
    // there). Used by the standalone LB_AP / LB_AP_EPS experiment method.
    bool proven_optimal;
};

class MASTER_Model {
protected:

    // Instance dimensions
    int n_;   // number of jobs
    int s_;   // number of stages
    int o_;   // number of operations
    int w_;   // number of workers

    // Worker costs
    vector<float> worker_costs_;

    // CPLEX model
    IloEnv env_;
    IloCplex cplex_;
    IloModel model_;

    // Assignment variables:
    // x_[i][w] = 1 if operation i is assigned to worker w
    IloArray<IloBoolVarArray> x_;

    // Productive-cost expression / lower bound on WWS
    IloExpr assignment_cost_;

public:

    // cmax_epsilon > 0 additionally adds, for every worker w, the
    // NECESSARY (not sufficient) epsilon-constraint workload bound
    //     sum_i p_i * x_[i][w] <= cmax_epsilon
    // valid because any schedule with Cmax <= epsilon must have every
    // worker's span T_w <= epsilon, and P_w <= T_w always (a worker
    // cannot process two operations at once). This only removes
    // assignments that cannot possibly fit inside epsilon regardless of
    // scheduling -- it does NOT prove an assignment IS schedulable within
    // epsilon (precedence/machine/stage interactions are still ignored
    // here, exactly as for the unconstrained master). The master
    // objective itself is unchanged; z_M remains a valid lower bound on
    // WWS*(epsilon) for the same reason it is one in the unconstrained
    // case (see Decomposition.h). <= 0 (the default) means no epsilon
    // constraint, preserving existing behavior exactly.
    // time_limit_seconds > 0 sets a CPLEX time limit (used ONLY by the
    // standalone LB_AP / LB_AP_EPS experiment CLI methods, which report
    // optimality_proven from CPLEX's own status rather than assuming exact
    // optimality). <= 0 (the default) leaves CPLEX's solve unbounded, which
    // Decomposition relies on for every master it constructs (solve_master()
    // /solve_master_pool() must always be a genuine, unbounded exact MIP
    // optimum -- see Decomposition.h) -- so Decomposition never passes this.
    // Threads and RandomSeed are always pinned (1 each) for reproducibility
    // and to avoid oversubscribing a single-CPU cluster allocation (see
    // experiments/README.md); this applies to every MASTER_Model regardless
    // of caller.
    MASTER_Model(
        const DRCRFFSP_Instance& instance,
        double cmax_epsilon = -1.0,
        double time_limit_seconds = -1.0
    );

    // Releases the Concert/CPLEX environment. IloEnv does not do this
    // automatically on destruction (env.end() must be called explicitly),
    // which this class previously never did. Safe here because
    // MASTER_Model is never copied or moved anywhere in this codebase.
    ~MASTER_Model();

    vector<int> solve_assignment(const DRCRFFSP_Instance& instance);

    // Objective value (assignment_cost_) of the last solved master problem
    double get_assignment_cost() const;

    // Solves the master and returns both the worker assignment and the
    // objective value from the same CPLEX solve (used by Decomposition).
    MasterSolution solve_master();

    // Adds  sum_i x_[i][worker_assignment[i]] <= o_ - 1,
    // excluding exactly this complete assignment from the master's
    // feasible region. This is used ONLY to enumerate master assignments
    // during decomposition; it is not a valid global WWS lower-bound cut,
    // since the excluded assignment may still be optimal in the original
    // (non-restricted) problem.
    void add_no_good_cut(const vector<int>& worker_assignment);

    // Uses CPLEX's solution pool (populate()) to retrieve MULTIPLE
    // exactly-optimal assignments from a single search, instead of one
    // solve_master() + add_no_good_cut() per assignment. The pool is
    // restricted to solutions AT the exact optimal objective (Pool::AbsGap
    // = Pool::RelGap = 0), consistent with the exact MIPGap = 0 already
    // set in the constructor -- every returned MasterSolution shares the
    // same objective_value.
    //
    // NOT guaranteed to return every optimal assignment: CPLEX's pool
    // search can stop before exhausting them even with a high Pool::
    // Intensity. Callers must still confirm exhaustion with a plain
    // solve_master() call after cutting everything returned here (see
    // Decomposition::run()) -- if that still returns a feasible solution
    // at the SAME objective value, more assignments remain and this
    // method should be called again.
    //
    // Returns an empty vector if the master is infeasible.
    vector<MasterSolution> solve_master_pool(int max_solutions = 500);
};

#endif