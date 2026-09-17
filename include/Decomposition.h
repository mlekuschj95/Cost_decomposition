#ifndef _DECOMPOSITION_H
#define _DECOMPOSITION_H

#include "DRCRFFSP_Instance.h"
#include "Masterproblem.h"
#include "CP_Model.h"
#include <chrono>

using namespace std;

// ------------------------------------------------------------------
// POOL INVARIANT (see Decomposition::run()):
//
//     MASTER: represents every assignment NOT YET excluded by a no-good cut.
//     POOL:   represents every assignment that HAS been excluded.
//
// For every pool entry we retain valid assignment-specific bounds:
//     assignment_lb <= WWS*(A) <= assignment_ub    (assignment_ub only once known)
//
// Certifying a candidate global lower bound L therefore requires BOTH:
//     (a) every remaining master assignment has productive cost >= L
//         (true automatically -- L is the master's own new optimum, and a
//         MIP optimum is a valid lower bound over its whole feasible
//         region), AND
//     (b) every EXCLUDED (pool) assignment satisfies assignment_lb >= L.
//
// This is why the pool must be checked IN FULL -- including entries
// excluded many levels ago -- and must persist for the entire run: an
// assignment excluded at level 198 is still part of the original
// optimization problem and must be accounted for when later certifying
// candidate levels 200, 205, 210, ...
//
// EPSILON-CONSTRAINT MODE (see run_for_epsilon()):
//
// The decomposition can additionally be run for the epsilon-constrained
// problem P(epsilon): min WWS s.t. Cmax <= epsilon. This is a genuinely
// DIFFERENT optimization problem for each epsilon, so:
//   - every CP query made during one epsilon run enforces Cmax <= epsilon
//     (zero-idle, threshold, exact-match, best-effort -- ALL of them, so
//     every scheduling proof within one run refers to the same feasible
//     region);
//   - the master gets an additional NECESSARY (not sufficient) workload
//     bound sum_i p_i x_iw <= epsilon per worker, baked in at
//     MASTER_Model construction time;
//   - global_lb_/global_ub_/has_incumbent_/pool_ and all no-good cuts are
//     reset between epsilon values -- an assignment excluded (or
//     analytically bounded) under epsilon_1 tells us NOTHING about its
//     status under a different epsilon_2, so nothing is carried over;
//   - a fixed assignment can be "zero-idle infeasible" (some schedule
//     exists under epsilon, just none with zero idle -- it remains a
//     valid pool candidate) or "completely infeasible under epsilon" (NO
//     schedule exists for it at all under epsilon -- it has no feasible
//     Q_epsilon(A) and is marked PoolEntry::epsilon_infeasible instead).
//     These are NOT the same statement and must never be conflated.
// ------------------------------------------------------------------

// A master assignment excluded from the master by a no-good cut, tracked
// for the remainder of the decomposition run.
//
// worker_assignment and master_value never change once the entry is
// created. add_no_good_cut() permanently excludes the exact assignment
// vector from the master's feasible region, so CPLEX can never return it
// again -- duplicate pool entries are therefore structurally impossible,
// not something that needs separate bookkeeping to prevent.
//
// assignment_lb only ever increases (each increase is a separately proven
// fact: either the analytical strengthened bound at creation time, or a
// later CP threshold-infeasibility proof). assignment_ub only ever
// decreases once known (a feasible schedule is a fact that never expires,
// though a later one may be better).
struct PoolEntry {
    vector<int> worker_assignment;
    double master_value;               // L(A), fixed forever

    double assignment_lb;              // best proven lower bound on WWS(A)

    bool has_ub;
    double assignment_ub;              // best known feasible WWS(A); NOT necessarily WWS(A)'s true minimum unless assignment_lb == assignment_ub

    bool zero_idle_proven_infeasible;  // false only for the (rare) case where the entry actually IS the exact WWS from a zero-idle-feasible result at a not-yet-certified level -- see run()'s handling of that case

    bool best_effort_attempted;        // avoids re-running the (deterministic) best-effort solve on the same entry every round once it has already been tried

    // Epsilon mode only (false in unconstrained runs): true iff CP proved
    // there is NO schedule at all for this fixed assignment with
    // Cmax <= epsilon (see CP_Model::solve_feasibility_under_epsilon()).
    // This is a STRONGER, DIFFERENT statement than
    // zero_idle_proven_infeasible: such an entry has no feasible
    // Q_epsilon(A) whatsoever and must be skipped by every bound
    // certification / exact-match / best-effort step for this epsilon --
    // assignment_lb/assignment_ub are not meaningful for it.
    bool epsilon_infeasible;
};

// Lightweight run statistics for the pre-matheuristic computational
// experiment (see the experiment CLI / experiments/README.md), useful for
// separating solution-finding difficulty from optimality-proof difficulty.
// Every counter is a plain increment at an existing call site -- nothing
// here changes any decomposition decision or branching logic.
struct DecompositionStats {
    int master_assignments_examined = 0;  // process_assignment() calls
    int master_levels_examined = 0;       // distinct master objective levels visited

    int zero_idle_checks = 0;
    int zero_idle_feasible = 0;
    int zero_idle_infeasible = 0;   // includes epsilon-mode case A and case B
    int zero_idle_unknown = 0;

    int threshold_calls = 0;                 // actual CP calls in certify_candidate_lb/run_exact_match_check
    int threshold_skipped_analytical = 0;    // skipped because assignment_lb already >= candidate_lb

    int exact_match_calls = 0;   // actual CP calls in run_exact_match_check
    int best_effort_calls = 0;   // actual CP calls in run_best_effort_ub_improvement

    int persistent_pool_size = 0;   // pool_.size() at the end of the run
};

// Final result of a full Decomposition::run() call (see also EpsilonResult
// for the richer result returned by run_for_epsilon()).
struct DecompositionResult {
    bool optimality_proven;
    double global_lb;
    bool has_incumbent;
    double global_ub;   // meaningful iff has_incumbent; == WWS* iff optimality_proven
    bool unresolved;    // true if the run stopped early on an UNKNOWN CP result, or
                         // because the total wall-clock time budget ran out (see
                         // time_limit_reached), without proving optimality

    // Actual Cmax of the incumbent/optimal schedule (the unconstrained
    // counterpart of EpsilonResult::achieved_makespan). -1 if has_incumbent
    // is false.
    double achieved_makespan;

    // true iff the run stopped specifically because the total wall-clock
    // budget (run()'s total_time_limit_seconds) was exhausted, as opposed
    // to a genuine CP UNKNOWN at full internal time. Distinguishing the two
    // lets the experiment CLI report FEASIBLE_TIME_LIMIT vs UNKNOWN
    // correctly (see section 19 of the experiment spec).
    bool time_limit_reached;

    DecompositionStats stats;
};

// Result of one independent Decomposition::run_for_epsilon() call, i.e. one
// complete exact solve of P(epsilon): min WWS s.t. Cmax <= epsilon.
struct EpsilonResult {
    double epsilon;

    bool optimality_proven;
    bool unresolved;         // stopped early on UNKNOWN, no proof either way

    double wws_lb;           // proven lower bound on WWS*(epsilon)
    bool has_incumbent;
    double wws_ub;           // meaningful iff has_incumbent; == WWS*(epsilon) iff optimality_proven

    // Actual Cmax of the incumbent/optimal schedule, which may be STRICTLY
    // LESS than epsilon even though the model only enforced Cmax <=
    // epsilon -- see Decomposition.h. -1 if has_incumbent is false.
    double achieved_makespan;

    double runtime_seconds;

    // Lightweight run statistics, useful for the paper's assignment-effect
    // vs scheduling/span-effect analysis (see the epsilon-constraint
    // conversation): pool_size is the number of assignments excluded from
    // the master over the course of this epsilon run (zero-idle-infeasible
    // plus epsilon-infeasible entries together).
    int pool_size;

    bool time_limit_reached;   // see DecompositionResult::time_limit_reached

    DecompositionStats stats;
};

// Orchestrates MASTER_Model (CPLEX) and CP_Model (CP Optimizer) for the WWS
// decomposition. Decomposition does not implement any CPLEX or CP Optimizer
// model itself -- it only calls into MASTER_Model / CP_Model and interprets
// their results.
class Decomposition {
private:

    const DRCRFFSP_Instance& instance_;

    // Non-owning pointer, not a reference, specifically so run_for_epsilon()
    // can temporarily point this at a FRESH, epsilon-specific MASTER_Model
    // (which must be constructed with the epsilon workload bound baked in,
    // and must start with zero no-good cuts) and restore the original
    // afterward. The public constructor still takes a MASTER_Model& for
    // the ordinary (unconstrained) use, exactly as before.
    MASTER_Model* master_;

    // Numerical tolerance for comparing master/WWS objective values (doubles).
    double tolerance_;

    // ---- Run-scoped state, reset at the start of every run() ----
    vector<PoolEntry> pool_;
    double global_lb_;
    bool has_incumbent_;
    double global_ub_;
    double incumbent_achieved_makespan_;   // -1 if unknown/not tracked for the current incumbent

    // Epsilon-constraint mode: <= 0 means unconstrained (preserves all
    // prior behavior exactly). Set only via run_for_epsilon(); run()
    // itself does not take it as a parameter so existing call sites are
    // unaffected.
    double cmax_epsilon_;

    // Total wall-clock deadline for the CURRENT run() call (see
    // total_time_limit_seconds on run()). Every internal CP call
    // (zero-idle, threshold, exact-match, best-effort) is capped to
    // whatever remains of this budget via CP_Model::set_time_limit() /
    // the time_limit_seconds argument already accepted by
    // solve_wws_best_effort(), so N internal calls cannot each
    // independently consume a full 3600s -- see run()'s doc comment.
    // Master (CPLEX) solves are deliberately NEVER time-limited here:
    // doing so could make solve_master()/solve_master_pool() return a
    // non-exact incumbent, which would silently break every LB proof in
    // this class (see MASTER_Model.h) -- so instead only whether a NEW
    // master-level iteration is STARTED is gated on this deadline.
    std::chrono::steady_clock::time_point run_deadline_;

    bool time_limit_reached_;   // set once, when the deadline check first fires
    DecompositionStats stats_;

    double time_remaining_seconds() const;

public:

    Decomposition(
        const DRCRFFSP_Instance& instance,
        MASTER_Model& master,
        double tolerance = 1e-6
    );

    // Runs the full decomposition to completion: master-level enumeration,
    // zero-idle checks, persistent no-good-cut pool bookkeeping, candidate
    // LB certification against the FULL pool, exact-match checks, and
    // best-effort UB improvement -- looping across as many master levels as
    // needed until either global optimality is proven (global_lb ==
    // global_ub) or a CP call returns UNKNOWN and the run must stop rather
    // than guess. best_effort_time_limit_seconds bounds each individual
    // best-effort CP solve; it is never used to justify a lower bound.
    //
    // Before the master-enumeration loop starts, if seed_ub_time_limit_
    // seconds > 0, runs solve_static_lex() ONCE on the integrated
    // (non-decomposed) baseline model to seed an initial global_ub_
    // cheaply -- static_lex empirically finds real feasible schedules
    // (and sometimes proves them optimal) far more reliably than either
    // the decomposition's own best-effort step or a direct WWS
    // minimization, for instances where zero-idle enumeration alone
    // makes little or no progress in reasonable time. This is a HEURISTIC
    // upper bound only: static_lex is optimal-makespan-first, so its
    // result is a valid WWS upper bound but not a proven global WWS
    // optimum unless the run's own exact-match/certification logic later
    // confirms it.
    //
    // Defaults to 0 (disabled): solve_static_lex() keeps searching for a
    // proof of optimality for as long as its time budget allows,
    // regardless of how fast it already found a good solution -- so an
    // unconditional seed would turn even an instance the main loop
    // resolves in milliseconds (zero-idle feasible on the very first
    // assignment) into one that blocks for the whole seed budget. Pass a
    // positive value explicitly when the main loop is already expected
    // to struggle (e.g. a known worker>machine or heavily-symmetric hard
    // instance) and a heuristic UB in hand from the start is wanted.
    //
    // NOTE: this runs the UNCONSTRAINED WWS problem (cmax_epsilon_ stays
    // disabled). Use run_for_epsilon() for P(epsilon).
    // total_time_limit_seconds bounds the ENTIRE call's wall-clock time,
    // covering every internal CP call this run makes (zero-idle, threshold,
    // exact-match, best-effort) as well as master (CPLEX) solves -- NOT
    // 3600s freely re-granted to each individual internal CP call (see the
    // class-level comment on run_deadline_). Defaults to a very large value
    // so every pre-existing call site (which predates this parameter) keeps
    // its previous "run until done" behavior unchanged; the computational
    // experiment CLI passes 3600.0 explicitly.
    DecompositionResult run(
        double best_effort_time_limit_seconds = 10.0,
        double seed_ub_time_limit_seconds = 0.0,
        double total_time_limit_seconds = 1e9
    );

    // Solves P(epsilon): min WWS s.t. Cmax <= epsilon, as one complete,
    // independent exact decomposition run. Internally constructs a FRESH
    // MASTER_Model(instance_, epsilon) (so no no-good cuts or workload
    // bounds leak in from any other epsilon value or from the caller's
    // own master), temporarily uses it in place of the master passed to
    // the constructor, and threads cmax_epsilon through every CP query
    // made during this call (see the class-level comment above). The
    // original master passed to the constructor is restored before
    // returning, so this Decomposition object remains usable for an
    // ordinary run() or another run_for_epsilon() call afterward.
    //
    // epsilon must be > 0 (a valid makespan bound is always positive).
    EpsilonResult run_for_epsilon(
        double epsilon,
        double best_effort_time_limit_seconds = 10.0,
        double seed_ub_time_limit_seconds = 0.0,
        double total_time_limit_seconds = 1e9
    );

private:

    // Analytical (non-CP) lower bound on the true WWS of a single fixed
    // assignment whose zero-idle CP test has already been PROVEN
    // infeasible: master_value + min_{w used in A} c_w, valid because CP
    // Optimizer's interval variables (durations are cast to int in
    // CP_Model's fixed-assignment constructor) and Worker::costs() (a
    // plain int) make idle time and worker cost integer, so at least one
    // used worker must contribute >= 1 unit of idle-time penalty. This
    // reasoning is unaffected by an additional Cmax <= epsilon constraint
    // (it only concerns idle time GIVEN that a zero-idle schedule was
    // already shown impossible under whatever constraints -- including
    // epsilon, if active -- were in force for that proof).
    double compute_strengthened_assignment_lb(
        const vector<int>& worker_assignment,
        double master_value,
        const DRCRFFSP_Instance& instance
    ) const;

    // Section 9 (correctness spec): has_incumbent_ && global_lb_ >=
    // global_ub_ - tolerance_.
    bool optimality_proven() const;

    // Updates global_ub_/has_incumbent_ if found_wws is an improvement,
    // and prints the current global bounds when it is. achieved_makespan
    // (when >= 0) is recorded as the incumbent's actual Cmax for later
    // reporting -- see EpsilonResult::achieved_makespan.
    void update_incumbent(double found_wws, double achieved_makespan = -1.0);

    // Section 4/5/6: tests every OPEN pool entry (assignment_lb <
    // candidate_lb, and not epsilon_infeasible) against candidate_lb --
    // INCLUDING entries created at older master levels, not just ones
    // from the current round. Skips a CP call entirely when a pool
    // entry's own stored data already resolves the question
    // (assignment_lb already clears candidate_lb, or a previously-found
    // assignment_ub already disqualifies it). A single UNKNOWN result
    // aborts the whole pass (unknown_hit set true; nothing further is
    // certified this call) -- otherwise every entry is tested, even
    // after one is found to disqualify candidate_lb, so the best possible
    // incumbent is still gathered from the remaining entries. Returns
    // true only if every relevant entry ended up with assignment_lb >=
    // candidate_lb.
    bool certify_candidate_lb(double candidate_lb, bool& unknown_hit);

    // Section 7: after global_lb_ has just been raised to target_lb, tests
    // whether any pool entry with assignment_lb <= target_lb admits a
    // schedule with WWS <= target_lb. Since target_lb is already a proven
    // global lower bound, such a schedule's WWS must equal target_lb
    // exactly, proving global optimality. Skips entries that cannot match
    // (assignment_lb > target_lb) and resolves entries whose own
    // assignment_ub already pins them to target_lb without any CP call.
    bool run_exact_match_check(double target_lb, bool& unknown_hit);

    // Section 8: best-effort (time-limited, never a lower-bound proof) WWS
    // minimization on promising still-open pool entries (assignment_lb <
    // global_ub_, not yet attempted), to improve global_ub_ when no exact
    // match was found.
    void run_best_effort_ub_improvement(double time_limit_seconds);
};

#endif
