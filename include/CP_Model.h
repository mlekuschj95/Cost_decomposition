#ifndef _CP_MODEL_H
#define _CP_MODEL_H



#include <ilcp/cp.h>
#include "DRCRFFSP_Instance.h"

using namespace std;

// Status of a CP Optimizer feasibility test (e.g. the zero-idle check).
// UNKNOWN must never be treated as INFEASIBLE by callers: it means the
// solver did not reach a conclusion (e.g. time limit), not that CP proved
// no feasible schedule exists.
enum class FeasibilityStatus {
    FEASIBLE,
    INFEASIBLE,
    UNKNOWN
};

// Result of a fixed-assignment WWS threshold feasibility test (see
// CP_Model::solve_wws_below_threshold). wws_value is only meaningful when
// status == FEASIBLE, and is the WWS of SOME schedule found below the
// threshold -- not necessarily that assignment's minimum WWS.
// achieved_makespan is that same schedule's actual Cmax (meaningful only
// when status == FEASIBLE); in epsilon mode it may be < the epsilon bound
// that was enforced -- see Decomposition.h.
struct ThresholdFeasibilityResult {
    FeasibilityStatus status;
    double wws_value;
    double achieved_makespan;
};

// Result of CP_Model::solve_zero_idle_feasibility(). achieved_makespan is
// that schedule's actual Cmax, meaningful only when status == FEASIBLE.
struct ZeroIdleResult {
    FeasibilityStatus status;
    double achieved_makespan;
};

// Result of a best-effort (time-limited) WWS minimization for a fixed
// assignment (see CP_Model::solve_wws_best_effort). This is NEVER a
// lower-bound proof by itself: status == FEASIBLE with proven_optimal ==
// false means CP found SOME schedule with this WWS before the time limit
// expired, not necessarily that assignment's true minimum.
struct BestEffortWWSResult {
    FeasibilityStatus status;
    double wws_value;       // meaningful only if status == FEASIBLE
    bool proven_optimal;    // true only if CP status was Optimal (a genuine certified per-assignment minimum)
    double achieved_makespan; // meaningful only if status == FEASIBLE
};

// Optional extra information filled in by solve_obj() / solve_static_lex()
// for the computational-experiment CLI, IN ADDITION to their existing
// return values -- no new solve method/search pattern is introduced; this
// just exposes a bit more of what the SAME existing single cp_.solve() call
// already computes internally (status, bound), plus the wall time that
// call already measures locally. Passing info == nullptr (the default)
// leaves every existing call site's behavior completely unchanged.
struct CPSolveInfo {
    IloAlgorithm::Status status = IloAlgorithm::Unknown;

    // CP's best proven bound on the objective (IloCP::getObjBound()) --
    // meaningful only when status is Optimal or Feasible. -1 (NA) for
    // solve_static_lex(): CP Optimizer's static-lex multi-criterion
    // objective does not expose a verified single bound through this call.
    double bound = -1.0;

    // The OTHER quantity than the one solve_obj()'s own return value
    // reports: for i==1 (min Cmax) this is the WWS of the returned
    // schedule; for i==2 (min WWS) this is its Cmax. -1 if unavailable.
    // Not filled by solve_static_lex() (it already returns both).
    double secondary_value = -1.0;

    double solve_time_sec = -1.0;
};

class CP_Model {
protected:
    char var[100];
    // Number of Jobs 
    int n_;
    // number of stages
    int s_;
    // number of total machines
    int m_;
    // number of levels
    int r_;
    // Number of operations
    int o_;
    //set of workers
    int w_;
    // vector with worker costs
    vector<float> worker_costs_;



    //CP model 
    IloEnv env_;   // CP environment 
    IloCP cp_; // CP object 
    IloModel model_; // actual model object
    //Decision variables
    IloIntervalVarArray2 machines_;
    IloIntervalVarArray operations_;
    IloIntervalVarArray2 workers_;
    IloBoolVarArray working_;
    //IloIntervalVar ops_;
    //IloIntervalVar master_;
    //IloIntervalVar member_;
    //IloIntervalVarArray members_;
    //Objective
    IloCumulFunctionExprArray stages_;
    IloIntExprArray makespan_;
    IloExprArray costs_;
    IloIntExpr obj1_;
    IloExpr obj2_;
    IloObjective static_lex_;
    IloObjective objeps1_;
    IloObjective objeps2_;
    
public:
   

    // construct model out of DRCRFFSP instance
    CP_Model(const DRCRFFSP_Instance& instance);
    CP_Model(
    const DRCRFFSP_Instance& instance,
    const vector<int>& worker_assignment);

    // Releases the Concert/CP Optimizer environment and everything built
    // on it (model, variables, expressions, the CP engine itself).
    // IloEnv does NOT do this automatically on destruction -- env.end()
    // must be called explicitly, which the class previously never did.
    // Safe here because CP_Model is never copied or moved anywhere in
    // this codebase (every use is a single locally-scoped named
    // variable), so there is no risk of two objects sharing one env_ and
    // double-ending it.
    ~CP_Model();

    //Solving the CP_Model
    // time_limit_seconds > 0 overrides this object's IloCP::TimeLimit
    // before solving (e.g. for a short "seed an upper bound quickly" use
    // from Decomposition); <= 0 (the default) leaves whatever time limit
    // is already set (the integrated baseline constructor's own value)
    // unchanged, preserving existing call sites' behavior exactly.
    // info, when non-null, is filled with status/solve-time (see
    // CPSolveInfo); bound is left at its NA default -- see CPSolveInfo.
    tuple<float, float> solve_static_lex(
        double time_limit_seconds = -1.0,
        CPSolveInfo* info = nullptr
    );
    void epsilon_original(const DRCRFFSP_Instance& instance);
    void epsilon_original_otherway(const DRCRFFSP_Instance& instance);
    void epsilon_fake(const DRCRFFSP_Instance& instance);
    // Creates a schedule that can be visualized in Python
    void create_schedule_from_CP_bi_obj(const DRCRFFSP_Instance& instance);
    void create_schedule_from_CP(const DRCRFFSP_Instance& instance);
    void create_schedule_from_CP_epsilon(const DRCRFFSP_Instance& instance, int k, int cmax);
    float solve();

    // cmax_epsilon > 0 additionally adds a PERMANENT IloMax(makespan_) <=
    // cmax_epsilon constraint before the objective (the direct-CP
    // counterpart of the master's P(epsilon) workload bound) -- this is
    // the "CP-WWS-EPS" experiment method (min WWS s.t. Cmax<=epsilon) when
    // combined with i==2. <= 0 (the default) preserves existing behavior
    // exactly. info, when non-null, is filled with status/bound/the other
    // objective's value/solve time -- see CPSolveInfo.
    //
    // known_wws_lower_bound > 0, combined with i==2, additionally adds a
    // PERMANENT IloSum(costs_) >= known_wws_lower_bound constraint before
    // the objective (see the 2026-09-23 conversation -- the "CP-WWS-EPS-
    // SEEDED" experiment method). This is REDUNDANT given a mathematically
    // valid bound (the true model's own constraints already imply it, so
    // the feasible region and optimum are unchanged), but hands CP
    // Optimizer a much tighter starting bound than its own default
    // internal dual bound would otherwise derive through propagation
    // alone (measured ~60x weaker on Small instances) -- typically the
    // output of MASTER_Model::solve_master()'s assignment relaxation for
    // the same (instance, cmax_epsilon). <= 0 (the default) preserves
    // existing behavior exactly.
    float solve_obj(
        const DRCRFFSP_Instance& instance,
        int i = 1,
        double cmax_epsilon = -1.0,
        CPSolveInfo* info = nullptr,
        double known_wws_lower_bound = -1.0
    );
    pair<float, float> solve_one_after_another(const DRCRFFSP_Instance& instance);

    // Zero-idle feasibility check for the fixed-assignment decomposition
    // subproblem: temporarily adds, for every used worker w,
    //     IloSizeOf(span_w) == assigned_load[w]
    // solves as a pure feasibility problem, then removes the temporary
    // constraints again so model_ is left unchanged for later normal solves
    // (solve(), solve_obj(), solve_static_lex()).
    //
    // cmax_epsilon > 0 additionally (temporarily) constrains
    // IloMax(makespan_) <= cmax_epsilon for the duration of this call, so
    // the zero-idle question becomes "does a zero-idle schedule exist
    // that ALSO satisfies Cmax <= epsilon" -- the correct question for an
    // epsilon-constrained run (see Decomposition.h). <= 0 (the default)
    // means unconstrained makespan, preserving existing behavior exactly.
    // A FEASIBLE result here only proves WWS(A) = master_value FOR THIS
    // epsilon; it says nothing about whether A is schedulable at all
    // under a DIFFERENT (e.g. tighter) epsilon.
    ZeroIdleResult solve_zero_idle_feasibility(double cmax_epsilon = -1.0);

    // Pure fixed-assignment scheduling feasibility under Cmax <= epsilon --
    // no zero-idle equality, no WWS bound, no objective. Used to
    // distinguish the two different kinds of "infeasible" that can occur
    // once epsilon is active:
    //     (a) zero-idle infeasible: SOME schedule exists under epsilon,
    //         just none with zero idle -- the assignment remains a valid
    //         candidate for P(epsilon), just not analytically resolved.
    //     (b) completely infeasible under epsilon: NO schedule exists at
    //         all for this fixed assignment with Cmax <= epsilon -- the
    //         assignment has no feasible Q_epsilon(A) and may be excluded
    //         entirely for this epsilon (see PoolEntry::epsilon_infeasible
    //         in Decomposition.h).
    // Call this ONLY to disambiguate after (a) has already been
    // observed (i.e. after solve_zero_idle_feasibility(cmax_epsilon)
    // returned INFEASIBLE) -- it is a second, separate CP solve.
    FeasibilityStatus solve_feasibility_under_epsilon(double cmax_epsilon);

    // Fixed-assignment WWS threshold feasibility test: temporarily
    // constrains WWS = sum_w c_w * T_w (the same IloSum(costs_) expression
    // used for the WWS cost in the constructor) to be strictly below
    // `threshold`, within `epsilon` numerical tolerance (worker costs, and
    // therefore WWS, are not guaranteed to be integer-valued in general,
    // though they are for this problem's current data). Solves as a pure
    // feasibility problem -- it does NOT minimize WWS -- then removes the
    // temporary constraints again so model_ is left unchanged for later
    // normal solves (solve(), solve_obj(), solve_static_lex(),
    // solve_zero_idle_feasibility()), exactly like the zero-idle check.
    //
    // In addition to that global bound, this recomputes L(A) = sum_w c_w
    // * assigned_load[w] (an exact integer, from this CP_Model's own
    // data) and the remaining integer idle budget
    //     Delta = (largest integer WWS allowed by the global bound) - L(A)
    // If Delta < 0, WWS < threshold is already impossible from the
    // productive-cost value alone and INFEASIBLE is returned without any
    // CP call. Otherwise, for every USED worker w it adds a redundant but
    // propagation-helping bound on that worker's own span, derived from
    // max_idle_w = Delta / c_w (integer division):
    //     max_idle_w == 0  ->  IloSizeOf(span_w) == assigned_load[w]
    //     max_idle_w >  0  ->  IloSizeOf(span_w) <= assigned_load[w] + max_idle_w
    // These per-worker bounds are necessary conditions only (a worker
    // respecting its own bound does not by itself guarantee the total
    // weighted idle budget is respected -- the global WWS bound above is
    // still required and is never removed). They are specific to this
    // exact fixed assignment and this exact threshold call, and are
    // removed again before returning, so they never leak into another
    // call or another assignment's CP_Model.
    // cmax_epsilon > 0 additionally (temporarily) constrains
    // IloMax(makespan_) <= cmax_epsilon for this call, so the threshold
    // question becomes "does a schedule with WWS < threshold exist that
    // ALSO satisfies Cmax <= epsilon". <= 0 (the default) means
    // unconstrained makespan, preserving existing behavior exactly.
    ThresholdFeasibilityResult solve_wws_below_threshold(
        double threshold,
        double epsilon = 1e-6,
        double cmax_epsilon = -1.0
    );

    // Best-effort (time-limited) minimization of WWS = sum_w c_w * T_w for
    // this fixed assignment. Unlike solve_wws_below_threshold() /
    // solve_zero_idle_feasibility(), this PERMANENTLY adds a minimization
    // objective to model_ -- safe only because every caller constructs a
    // fresh, single-use CP_Model per assignment and never reuses it
    // afterward (see Decomposition::run_best_effort_ub_improvement()).
    // Sets a short IloCP::TimeLimit (time_limit_seconds) so a hard
    // instance cannot block the decomposition loop.
    //     status == FEASIBLE, proven_optimal == true  -> CP proved
    //         optimality within the time limit: a genuine certified
    //         minimum for this exact assignment.
    //     status == FEASIBLE, proven_optimal == false -> CP found a
    //         feasible incumbent before the time limit: a valid upper
    //         bound, NOT a proof of that assignment's minimum.
    //     status == UNKNOWN -> no solution found within the time limit.
    // This method must NEVER be used to justify raising a lower bound.
    // cmax_epsilon > 0 permanently constrains IloMax(makespan_) <=
    // cmax_epsilon (replacing the loose sum-of-processing-times
    // propagation aid this method already adds, since a real epsilon
    // bound is strictly tighter and equally valid for propagation). <= 0
    // (the default) preserves existing unconstrained behavior exactly.
    BestEffortWWSResult solve_wws_best_effort(
        double time_limit_seconds,
        double cmax_epsilon = -1.0
    );

    // Overrides this CP_Model's IloCP::TimeLimit after construction. Used
    // by Decomposition to cap each internal fixed-assignment subproblem
    // call to the REMAINING total decomposition wall-clock budget, so many
    // internal CP calls cannot each independently consume a full 3600s
    // (see Decomposition::run()'s time_limit_seconds parameter). Also used
    // by the experiment CLI to apply --time-limit uniformly.
    void set_time_limit(double seconds);

    // Reads back which worker was actually assigned to each operation on
    // the most recently solved model (Optimal or Feasible status) --
    // result[i] = the 1-based worker ID (matching Worker::Id()) chosen for
    // operation i, or -1 if operation i has no present mode (should not
    // happen for a feasible solution). Reads the SAME optional-interval
    // "mode" presence pattern the constructor builds (see CP_Model.cpp) --
    // a pure query, does not change the model. Ported out of the existing
    // (but fragile: hardcoded output path, an unexplained
    // filename().erase(0, 25)) create_schedule_from_CP() so both the
    // integrated model (worker chosen freely by CP, e.g. cp_lex) and the
    // fixed-assignment model (worker chosen by MASTER_Model beforehand)
    // can have their resulting assignment read back the same way for
    // comparison (see the 2026-09-23 conversation).
    vector<int> get_worker_assignment(const DRCRFFSP_Instance& instance) const;
};

#endif