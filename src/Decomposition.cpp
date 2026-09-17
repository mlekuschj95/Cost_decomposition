#include "Decomposition.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

Decomposition::Decomposition(
    const DRCRFFSP_Instance& instance,
    MASTER_Model& master,
    double tolerance)
    : instance_(instance)
    , master_(&master)
    , tolerance_(tolerance)
    , global_lb_(0.0)
    , has_incumbent_(false)
    , global_ub_(std::numeric_limits<double>::infinity())
    , incumbent_achieved_makespan_(-1.0)
    , cmax_epsilon_(-1.0)
    , run_deadline_(chrono::steady_clock::now())
    , time_limit_reached_(false)
{
}

double Decomposition::time_remaining_seconds() const
{
    return chrono::duration<double>(run_deadline_ - chrono::steady_clock::now()).count();
}

double Decomposition::compute_strengthened_assignment_lb(
    const vector<int>& worker_assignment,
    double master_value,
    const DRCRFFSP_Instance& instance) const
{
    // int, not float/double: Worker::costs() is declared as int (see
    // Worker.h), so this loop cannot silently accept a fractional cost.
    bool any_used = false;
    int min_used_worker_cost = 0;

    for (int w : worker_assignment) {

        int c_w = (instance.workers_begin() + w)->costs();

        if (!any_used || c_w < min_used_worker_cost) {
            min_used_worker_cost = c_w;
            any_used = true;
        }
    }

    return master_value + static_cast<double>(min_used_worker_cost);
}

bool Decomposition::optimality_proven() const
{
    return has_incumbent_ && (global_lb_ >= global_ub_ - tolerance_);
}

void Decomposition::update_incumbent(double found_wws, double achieved_makespan)
{
    if (!has_incumbent_ || found_wws < global_ub_ - tolerance_) {

        global_ub_ = found_wws;
        has_incumbent_ = true;
        incumbent_achieved_makespan_ = achieved_makespan;

        cout << "\nGLOBAL BOUNDS" << endl;
        cout << "LB = " << global_lb_ << endl;
        cout << "UB = " << global_ub_ << endl;
        if (achieved_makespan >= 0.0) {
            cout << "Achieved Cmax of this schedule: " << achieved_makespan << endl;
        }
    }
}

bool Decomposition::certify_candidate_lb(double candidate_lb, bool& unknown_hit)
{
    unknown_hit = false;
    bool all_certified = true;

    int entry_index = 0;

    for (auto& entry : pool_) {

        ++entry_index;

        if (entry.epsilon_infeasible) {
            // No feasible Q_epsilon(A) exists at all for this entry; it
            // can never contribute a WWS value below (or at) anything,
            // so it can never invalidate a candidate LB. Trivially
            // "certified" -- skip.
            continue;
        }

        if (entry.assignment_lb >= candidate_lb - tolerance_) {
            // Already certified for this (or a higher) candidate --
            // resolved by the analytical LB alone, no CP call needed.
            ++stats_.threshold_skipped_analytical;
            continue;
        }

        cout << "\nPOOL ENTRY " << entry_index << endl;
        cout << "Master value:  " << entry.master_value << endl;
        cout << "Assignment LB: " << entry.assignment_lb << endl;
        cout << "Assignment UB: "
             << (entry.has_ub ? std::to_string(entry.assignment_ub) : string("none"))
             << endl;
        cout << "Candidate LB:  " << candidate_lb << endl;

        // Already known (from an earlier discovery) that this entry can
        // beat candidate_lb -- no need to ask CP again.
        if (entry.has_ub && entry.assignment_ub < candidate_lb - tolerance_) {

            cout << "Already-known UB (" << entry.assignment_ub
                 << ") is below the candidate LB -- cannot certify, no new "
                    "CP call needed." << endl;

            all_certified = false;
            continue;
        }

        cout << "LB insufficient. Running CP test: WWS < " << candidate_lb;
        if (cmax_epsilon_ > 0) {
            cout << "  (Cmax <= " << cmax_epsilon_ << ")";
        }
        cout << endl;

        CP_Model subproblem(instance_, entry.worker_assignment);
        subproblem.set_time_limit(time_remaining_seconds());
        ++stats_.threshold_calls;

        ThresholdFeasibilityResult test =
            subproblem.solve_wws_below_threshold(candidate_lb, 1e-6, cmax_epsilon_);

        if (test.status == FeasibilityStatus::UNKNOWN) {

            cout << "Result: UNKNOWN (e.g. time limit). Candidate LB "
                    "cannot be certified or rejected from this entry; "
                    "stopping." << endl;

            unknown_hit = true;
            return false;
        }

        if (test.status == FeasibilityStatus::INFEASIBLE) {

            cout << "Result: INFEASIBLE" << endl;
            cout << "Assignment LB updated: " << entry.assignment_lb
                 << " -> " << candidate_lb << endl;

            entry.assignment_lb = candidate_lb;
            continue;
        }

        // FEASIBLE: candidate_lb cannot be certified this round, but this
        // is a genuine incumbent -- keep testing the remaining entries too.
        cout << "Result: FEASIBLE, WWS = " << test.wws_value
             << ", Cmax = " << test.achieved_makespan << endl;

        if (!entry.has_ub || test.wws_value < entry.assignment_ub) {
            entry.has_ub = true;
            entry.assignment_ub = test.wws_value;
        }

        update_incumbent(test.wws_value, test.achieved_makespan);

        all_certified = false;

        if (optimality_proven()) {
            // Section 9: stop immediately once proven; the caller checks
            // optimality_proven() right after this call returns.
            return false;
        }
    }

    return all_certified;
}

bool Decomposition::run_exact_match_check(double target_lb, bool& unknown_hit)
{
    unknown_hit = false;

    cout << "\nChecking whether any pool entry achieves WWS == "
         << target_lb << " exactly." << endl;

    const double epsilon = 1e-6;
    int entry_index = 0;

    for (auto& entry : pool_) {

        ++entry_index;

        if (entry.epsilon_infeasible) {
            continue;   // no feasible Q_epsilon(A) at all; cannot match
        }

        if (entry.assignment_lb > target_lb + tolerance_) {
            continue;   // cannot possibly equal target_lb
        }

        if (entry.has_ub && entry.assignment_ub <= target_lb + tolerance_) {

            // assignment_lb <= target_lb (checked above) and, once
            // global_lb_ == target_lb has been certified, assignment_lb >=
            // target_lb too for every pool entry -- so together with
            // assignment_ub <= target_lb this pins WWS(A) == target_lb
            // exactly. No CP call needed.
            cout << "\nPOOL ENTRY " << entry_index
                 << ": assignment_lb/assignment_ub already pin WWS == "
                 << target_lb << " -- optimum proven without a CP call."
                 << endl;

            update_incumbent(entry.assignment_ub);
            return true;
        }

        cout << "\nPOOL ENTRY " << entry_index
             << ": testing WWS <= " << target_lb;
        if (cmax_epsilon_ > 0) {
            cout << "  (Cmax <= " << cmax_epsilon_ << ")";
        }
        cout << endl;

        CP_Model subproblem(instance_, entry.worker_assignment);
        subproblem.set_time_limit(time_remaining_seconds());
        ++stats_.exact_match_calls;

        // solve_wws_below_threshold enforces WWS <= threshold - epsilon;
        // passing (target_lb + epsilon) turns that into WWS <= target_lb.
        ThresholdFeasibilityResult test =
            subproblem.solve_wws_below_threshold(
                target_lb + epsilon, epsilon, cmax_epsilon_
            );

        if (test.status == FeasibilityStatus::UNKNOWN) {

            cout << "Result: UNKNOWN. Exact-match check inconclusive for "
                    "this entry; stopping." << endl;
            unknown_hit = true;
            return false;
        }

        if (test.status == FeasibilityStatus::INFEASIBLE) {

            cout << "Result: INFEASIBLE (WWS > " << target_lb << ")." << endl;
            continue;
        }

        // FEASIBLE: WWS <= target_lb, and target_lb is already a proven
        // global LB, so this schedule's WWS must equal target_lb exactly.
        cout << "Result: FEASIBLE, WWS == " << target_lb
             << " exactly, Cmax = " << test.achieved_makespan << endl;

        if (!entry.has_ub || test.wws_value < entry.assignment_ub) {
            entry.has_ub = true;
            entry.assignment_ub = test.wws_value;
        }

        update_incumbent(target_lb, test.achieved_makespan);
        return true;
    }

    cout << "\nNo pool entry achieves WWS == " << target_lb << " exactly."
         << endl;

    return false;
}

void Decomposition::run_best_effort_ub_improvement(double time_limit_seconds)
{
    cout << "\n--- BEST-EFFORT UB IMPROVEMENT ---" << endl;

    int entry_index = 0;

    for (auto& entry : pool_) {

        ++entry_index;

        if (entry.epsilon_infeasible) {
            continue;   // no feasible Q_epsilon(A) at all; nothing to optimize
        }

        if (entry.best_effort_attempted) {
            continue;
        }

        if (has_incumbent_ && entry.assignment_lb >= global_ub_ - tolerance_) {
            // Cannot possibly improve the incumbent.
            continue;
        }

        if (entry.has_ub &&
            entry.assignment_lb >= entry.assignment_ub - tolerance_) {
            // Already pinned exactly; nothing to improve.
            continue;
        }

        cout << "\nPOOL ENTRY " << entry_index
             << ": best-effort WWS solve (time limit "
             << time_limit_seconds << "s)";
        if (cmax_epsilon_ > 0) {
            cout << ", Cmax <= " << cmax_epsilon_;
        }
        cout << endl;

        entry.best_effort_attempted = true;
        ++stats_.best_effort_calls;

        CP_Model subproblem(instance_, entry.worker_assignment);

        double effective_limit = std::min(time_limit_seconds, time_remaining_seconds());

        BestEffortWWSResult effort =
            subproblem.solve_wws_best_effort(effective_limit, cmax_epsilon_);

        if (effort.status == FeasibilityStatus::FEASIBLE) {

            cout << "Found WWS = " << effort.wws_value
                 << ", Cmax = " << effort.achieved_makespan
                 << (effort.proven_optimal
                         ? " (proven optimal for this assignment)"
                         : " (best-effort incumbent, not proven optimal)")
                 << endl;

            if (!entry.has_ub || effort.wws_value < entry.assignment_ub) {
                entry.has_ub = true;
                entry.assignment_ub = effort.wws_value;
            }

            if (effort.proven_optimal) {
                // A proven-optimal per-assignment solve also certifies
                // this entry's lower bound: this value IS its true
                // minimum, so no schedule for A can ever be better.
                entry.assignment_lb =
                    std::max(entry.assignment_lb, effort.wws_value);
            }

            update_incumbent(effort.wws_value, effort.achieved_makespan);

            if (optimality_proven()) {
                return;
            }
        }
        else if (effort.status == FeasibilityStatus::INFEASIBLE &&
                 cmax_epsilon_ > 0) {

            // In epsilon mode this is a genuine proof: no schedule at all
            // exists for this fixed assignment with Cmax <= epsilon. This
            // is the same "completely infeasible under epsilon" fact as
            // PoolEntry::epsilon_infeasible -- mark it so, so it is
            // skipped everywhere from now on for this epsilon.
            cout << "INFEASIBLE under epsilon: no schedule exists for "
                    "this assignment with Cmax <= " << cmax_epsilon_
                 << ". Marking epsilon_infeasible." << endl;
            entry.epsilon_infeasible = true;
        }
        else {

            // UNKNOWN (no solution within the time limit), or the
            // defensive INFEASIBLE case outside epsilon mode: not a
            // proof of anything, just no improvement found this time.
            // NEVER used to raise a bound.
            cout << "No usable result (status "
                 << (effort.status == FeasibilityStatus::UNKNOWN
                         ? "UNKNOWN"
                         : "INFEASIBLE")
                 << ") within the time limit; leaving bounds unchanged."
                 << endl;
        }
    }
}

DecompositionResult Decomposition::run(
    double best_effort_time_limit_seconds,
    double seed_ub_time_limit_seconds,
    double total_time_limit_seconds)
{
    pool_.clear();
    global_lb_ = 0.0;
    has_incumbent_ = false;
    global_ub_ = std::numeric_limits<double>::infinity();
    incumbent_achieved_makespan_ = -1.0;
    time_limit_reached_ = false;
    stats_ = DecompositionStats{};

    run_deadline_ = chrono::steady_clock::now()
        + chrono::duration_cast<chrono::steady_clock::duration>(
              chrono::duration<double>(total_time_limit_seconds));

    DecompositionResult result;

    auto finalize = [&](bool proven, bool unresolved) {
        result.optimality_proven = proven;
        result.unresolved = unresolved;
        result.global_lb = global_lb_;
        result.has_incumbent = has_incumbent_;
        result.global_ub = has_incumbent_ ? global_ub_ : -1.0;
        result.achieved_makespan = has_incumbent_ ? incumbent_achieved_makespan_ : -1.0;
        result.time_limit_reached = time_limit_reached_;
        stats_.persistent_pool_size = static_cast<int>(pool_.size());
        result.stats = stats_;
        return result;
    };

    auto announce_optimum = [&]() {
        cout << "\n========================================" << endl;
        cout << "GLOBAL OPTIMALITY PROVEN" << endl;
        cout << "WWS* = " << global_ub_ << endl;
        cout << "LB   = " << global_lb_ << endl;
        cout << "UB   = " << global_ub_ << endl;
        if (incumbent_achieved_makespan_ >= 0.0) {
            cout << "Achieved Cmax = " << incumbent_achieved_makespan_ << endl;
        }
        cout << "========================================" << endl;
    };

    // Processes exactly one assignment: zero-idle test, pool bookkeeping,
    // no-good cut. Returns a finalized result if the run should stop here
    // (either announce_optimum() was already called, or UNKNOWN was hit),
    // or std::nullopt if the caller should keep going. See the note on
    // PoolEntry::zero_idle_proven_infeasible in Decomposition.h and the
    // "correctness issue" writeup for why a zero-idle FEASIBLE result does
    // NOT unconditionally prove global optimality here.
    //
    // In epsilon mode (cmax_epsilon_ > 0), a zero-idle INFEASIBLE result
    // is disambiguated with a second CP call
    // (solve_feasibility_under_epsilon) to tell apart "zero-idle
    // infeasible but still a valid P(epsilon) candidate" from "no
    // schedule at all exists for A under epsilon" -- see PoolEntry::
    // epsilon_infeasible and the class-level comment in Decomposition.h.
    auto process_assignment =
        [&](const MasterSolution& assignment) -> std::optional<DecompositionResult> {

        ++stats_.master_assignments_examined;

        CP_Model subproblem(instance_, assignment.worker_assignment);
        subproblem.set_time_limit(time_remaining_seconds());

        ++stats_.zero_idle_checks;
        ZeroIdleResult zero_idle = subproblem.solve_zero_idle_feasibility(cmax_epsilon_);

        if (zero_idle.status == FeasibilityStatus::UNKNOWN) {

            ++stats_.zero_idle_unknown;
            cout << "\nZero-idle status UNKNOWN for this assignment "
                    "(e.g. time limit reached). Stopping; no no-good cut "
                    "added." << endl;
            return finalize(false, true);
        }

        PoolEntry entry;
        entry.worker_assignment = assignment.worker_assignment;
        entry.master_value = assignment.objective_value;
        entry.epsilon_infeasible = false;

        if (zero_idle.status == FeasibilityStatus::FEASIBLE) {

            ++stats_.zero_idle_feasible;
            cout << "\nZero-idle feasible: WWS(A) = "
                 << assignment.objective_value
                 << ", Cmax = " << zero_idle.achieved_makespan << endl;
            update_incumbent(assignment.objective_value, zero_idle.achieved_makespan);

            if (optimality_proven()) {
                announce_optimum();
                return finalize(true, false);
            }

            cout << "\nWWS(A) is known exactly, but the global LB has not "
                    "yet reached this level. Recording it as fully "
                    "resolved and continuing enumeration." << endl;

            entry.assignment_lb = assignment.objective_value;
            entry.has_ub = true;
            entry.assignment_ub = assignment.objective_value;
            entry.zero_idle_proven_infeasible = false;
            entry.best_effort_attempted = true;   // exact already; nothing to improve
        }
        else {

            // zero_idle.status == FeasibilityStatus::INFEASIBLE
            ++stats_.zero_idle_infeasible;
            //
            // Outside epsilon mode this can only mean "zero idle is
            // impossible for A" (case A). In epsilon mode we do not yet
            // know whether this is case A (some schedule exists under
            // epsilon, just none zero-idle) or case B (NO schedule exists
            // for A under epsilon at all) -- disambiguate with a second,
            // separate CP call.
            bool is_epsilon_infeasible = false;

            if (cmax_epsilon_ > 0) {

                cout << "\nZero-idle infeasible under epsilon. "
                        "Disambiguating: does ANY schedule exist for this "
                        "assignment with Cmax <= " << cmax_epsilon_
                     << "?" << endl;

                subproblem.set_time_limit(time_remaining_seconds());
                FeasibilityStatus feas =
                    subproblem.solve_feasibility_under_epsilon(cmax_epsilon_);

                if (feas == FeasibilityStatus::UNKNOWN) {

                    cout << "\nDisambiguation status UNKNOWN. Cannot "
                            "safely classify this assignment; stopping."
                         << endl;
                    return finalize(false, true);
                }

                is_epsilon_infeasible = (feas == FeasibilityStatus::INFEASIBLE);
            }

            if (is_epsilon_infeasible) {

                cout << "\nCompletely infeasible under epsilon: no "
                        "schedule exists for this assignment with "
                        "Cmax <= " << cmax_epsilon_
                     << ". Marking epsilon_infeasible and adding the "
                        "exact no-good cut." << endl;

                entry.assignment_lb = -1.0;   // not meaningful; see epsilon_infeasible
                entry.has_ub = false;
                entry.assignment_ub = -1.0;
                entry.zero_idle_proven_infeasible = false;
                entry.best_effort_attempted = true;   // nothing to optimize; no Q_epsilon(A)
                entry.epsilon_infeasible = true;
            }
            else {

                cout << "\nZero-idle proven infeasible. Adding no-good cut."
                     << endl;

                entry.assignment_lb = compute_strengthened_assignment_lb(
                    assignment.worker_assignment,
                    assignment.objective_value,
                    instance_
                );
                entry.has_ub = false;
                entry.assignment_ub = -1.0;
                entry.zero_idle_proven_infeasible = true;
                entry.best_effort_attempted = false;

                cout << "Assignment LB (master_value + min_used_worker_cost): "
                     << entry.assignment_lb << endl;
            }
        }

        pool_.push_back(entry);
        master_->add_no_good_cut(entry.worker_assignment);

        return std::nullopt;
    };

    cout << "\n==============================" << endl;
    cout << "DECOMPOSITION - WWS" << endl;
    if (cmax_epsilon_ > 0) {
        cout << "Epsilon-constraint mode: Cmax <= " << cmax_epsilon_ << endl;
    }
    cout << "==============================" << endl;

    // ------------------------------------------------------------
    // Discover assignments in BATCHES via CPLEX's solution pool
    // (MASTER_Model::solve_master_pool()) instead of one solve_master()
    // call per assignment: for a master level with many equal-cost
    // (symmetric) assignments, this can retrieve most or all of them from
    // a single search instead of re-proving the master's MIP to exact
    // optimality from scratch after every individual no-good cut, which
    // is what made large symmetric instances slow. Since the pool is not
    // guaranteed exhaustive, a plain solve_master() call ("the check") is
    // always made after cutting a whole batch, to confirm the level is
    // truly exhausted before certification proceeds; if it isn't (more
    // assignments remain at the same level), that one is processed too
    // and populate() is retried.
    // ------------------------------------------------------------

    vector<MasterSolution> batch = master_->solve_master_pool();

    if (batch.empty()) {
        cout << "\nMaster problem infeasible from the start." << endl;
        return finalize(false, false);
    }

    double current_level = batch.front().objective_value;
    ++stats_.master_levels_examined;

    // Base case: the unrestricted master optimum is unconditionally a
    // valid global WWS lower bound.
    global_lb_ = current_level;

    // ------------------------------------------------------------
    // HEURISTIC UB seeding: run solve_static_lex() once on the
    // integrated (non-decomposed) baseline model, time-capped, before
    // starting master-level enumeration. Empirically this finds a real
    // feasible schedule (sometimes a proven one) far more reliably than
    // this decomposition's own best-effort step, on instances where
    // zero-idle enumeration alone makes little progress in reasonable
    // time. The result is a valid WWS UPPER BOUND (a real, complete
    // schedule), but NOT a proven global WWS optimum: static_lex
    // minimizes WWS only among makespan-optimal schedules, so a
    // different (worse-makespan) schedule could still have lower WWS.
    // Only this run's own exact-match/certification logic below can
    // upgrade it to a proven optimum.
    //
    // NOTE: this seeding step ignores epsilon (solve_static_lex() has no
    // Cmax constraint) -- it is only used for the unconstrained run().
    // run_for_epsilon() does not use it (seed_ub_time_limit_seconds is
    // simply not a meaningful heuristic once Cmax is constrained: a
    // makespan-first search is not obviously helpful when makespan is
    // already capped, and a seeded schedule with Cmax > epsilon would be
    // outright invalid for P(epsilon)).
    // ------------------------------------------------------------

    if (seed_ub_time_limit_seconds > 0 && cmax_epsilon_ <= 0) {

        cout << "\n--- SEEDING INITIAL UB (solve_static_lex, integrated "
                "baseline, heuristic) ---" << endl;

        CP_Model seed_model(instance_);
        auto lex_result = seed_model.solve_static_lex(seed_ub_time_limit_seconds);
        double seed_wws = get<1>(lex_result);
        double seed_makespan = get<0>(lex_result);

        if (seed_wws >= 0.0) {

            cout << "Seed UB found: WWS = " << seed_wws
                 << " (lex-constrained upper bound -- optimal only among "
                    "makespan-optimal schedules, NOT necessarily the "
                    "global WWS optimum)." << endl;

            update_incumbent(seed_wws, seed_makespan);
        }
        else {

            cout << "No usable seed UB found within the time limit."
                 << endl;
        }

        if (optimality_proven()) {
            announce_optimum();
            return finalize(true, false);
        }
    }

    int batch_round = 0;

    while (true) {

        if (time_remaining_seconds() <= 0.0) {

            cout << "\nTOTAL DECOMPOSITION TIME BUDGET EXHAUSTED ("
                 << total_time_limit_seconds << "s). Stopping before "
                    "starting a new master-level iteration." << endl;

            time_limit_reached_ = true;
            return finalize(false, true);
        }

        ++batch_round;

        cout << "\nMaster level: " << current_level << endl;
        cout << "Pool-discovered batch size: " << batch.size() << endl;

        int assignment_index = 0;

        for (const auto& assignment : batch) {

            ++assignment_index;
            cout << "\nBatch " << batch_round << ", assignment #"
                 << assignment_index << endl;

            auto outcome = process_assignment(assignment);
            if (outcome.has_value()) {
                return *outcome;
            }
        }

        // "The check": a plain solve_master() call, which already
        // includes every no-good cut just added for the whole batch above
        // (and everything before it) -- this is what actually tells us
        // whether populate() found every optimal assignment at this level.
        MasterSolution check = master_->solve_master();

        if (!check.feasible) {

            // --------------------------------------------------------
            // The ENTIRE assignment space has now been excluded (only
            // realistic for very small instances). Nothing remains in
            // the master, so every remaining candidate lives in pool_:
            // WWS* = min over pool_ of true WWS(A) >= min over pool_ of
            // assignment_lb, which is a valid global LB here with NO CP
            // call needed. Epsilon-infeasible entries are excluded from
            // this min -- they have no feasible Q_epsilon(A) at all, so
            // they cannot be the minimizer.
            // --------------------------------------------------------

            cout << "\nMaster infeasible: every assignment has now been "
                    "excluded by no-good cuts. Resolving from the "
                    "persistent pool alone." << endl;

            bool any_relevant = false;
            double pool_min_lb = 0.0;
            for (const auto& e : pool_) {
                if (e.epsilon_infeasible) continue;
                if (!any_relevant || e.assignment_lb < pool_min_lb) {
                    pool_min_lb = e.assignment_lb;
                    any_relevant = true;
                }
            }
            if (any_relevant) {
                global_lb_ = std::max(global_lb_, pool_min_lb);
            }

            cout << "\nGLOBAL BOUNDS" << endl;
            cout << "LB = " << global_lb_ << endl;
            if (has_incumbent_) {
                cout << "UB = " << global_ub_ << endl;
            }

            if (optimality_proven()) {
                announce_optimum();
                return finalize(true, false);
            }

            run_best_effort_ub_improvement(best_effort_time_limit_seconds);

            if (optimality_proven()) {
                announce_optimum();
                return finalize(true, false);
            }

            cout << "\nMaster fully exhausted; gap remains. LB = "
                 << global_lb_;
            if (has_incumbent_) {
                cout << ", UB = " << global_ub_;
            } else {
                cout << ", UB unknown";
            }
            cout << endl;

            return finalize(false, false);
        }

        if (std::abs(check.objective_value - current_level) <= tolerance_) {

            // The pool missed at least one assignment still at this exact
            // level: process it too, then retry populate() (it may now
            // find the rest, or there may be none left).
            cout << "\nPopulate() missed at least one assignment at this "
                    "level; processing it and retrying populate()." << endl;

            auto outcome = process_assignment(check);
            if (outcome.has_value()) {
                return *outcome;
            }

            batch = master_->solve_master_pool();

            if (batch.empty()) {

                cout << "\nMaster infeasible: every assignment has now "
                        "been excluded by no-good cuts. Resolving from "
                        "the persistent pool alone." << endl;

                bool any_relevant = false;
                double pool_min_lb = 0.0;
                for (const auto& e : pool_) {
                    if (e.epsilon_infeasible) continue;
                    if (!any_relevant || e.assignment_lb < pool_min_lb) {
                        pool_min_lb = e.assignment_lb;
                        any_relevant = true;
                    }
                }
                if (any_relevant) {
                    global_lb_ = std::max(global_lb_, pool_min_lb);
                }

                if (optimality_proven()) {
                    announce_optimum();
                    return finalize(true, false);
                }

                run_best_effort_ub_improvement(best_effort_time_limit_seconds);

                if (optimality_proven()) {
                    announce_optimum();
                    return finalize(true, false);
                }

                return finalize(false, false);
            }

            current_level = batch.front().objective_value;
            ++stats_.master_levels_examined;
            continue;
        }

        // --------------------------------------------------------
        // Level exhausted; candidate_lb is the master's next level.
        // --------------------------------------------------------

        double candidate_lb = check.objective_value;

        cout << "\n========================================" << endl;
        cout << "MASTER LEVEL EXHAUSTED" << endl;
        cout << "Exhausted level:    " << current_level << endl;
        cout << "Candidate LB:       " << candidate_lb << endl;
        cout << "Pool size:          " << pool_.size() << endl;
        cout << "Current global LB:  " << global_lb_ << endl;
        cout << "Current global UB:  "
             << (has_incumbent_ ? std::to_string(global_ub_) : string("none"))
             << endl;
        cout << "========================================" << endl;

        bool unknown_hit = false;
        bool all_certified = certify_candidate_lb(candidate_lb, unknown_hit);

        if (unknown_hit) {

            cout << "\nCandidate LB " << candidate_lb
                 << " could not be certified: a threshold test returned "
                    "UNKNOWN. Stopping." << endl;
            return finalize(false, true);
        }

        if (optimality_proven()) {
            announce_optimum();
            return finalize(true, false);
        }

        if (all_certified) {

            cout << "\nALL POOL ENTRIES CERTIFIED >= " << candidate_lb << endl;
            cout << "GLOBAL LB: " << global_lb_ << " -> " << candidate_lb << endl;

            global_lb_ = candidate_lb;

            if (optimality_proven()) {
                announce_optimum();
                return finalize(true, false);
            }

            bool exact_unknown = false;
            bool matched = run_exact_match_check(global_lb_, exact_unknown);

            if (exact_unknown) {

                cout << "\nExact-match check inconclusive (UNKNOWN). "
                        "Stopping with LB certified but optimum not proven."
                     << endl;
                return finalize(false, true);
            }

            if (matched || optimality_proven()) {
                announce_optimum();
                return finalize(true, false);
            }

            run_best_effort_ub_improvement(best_effort_time_limit_seconds);

            if (optimality_proven()) {
                announce_optimum();
                return finalize(true, false);
            }
        }
        else {

            cout << "\nCandidate LB " << candidate_lb
                 << " NOT certified this round (a pool entry remains "
                    "below it). Continuing to the next master level "
                    "regardless." << endl;
        }

        // Move on to the new level's batch. 'check' itself is still an
        // uncut assignment at candidate_lb, so it will naturally reappear
        // in this next populate() call along with any siblings.
        current_level = candidate_lb;
        ++stats_.master_levels_examined;
        batch = master_->solve_master_pool();

        if (batch.empty()) {

            // 'check' turned out to be the very last assignment in the
            // whole problem; resolve from the pool alone (same edge case
            // as above, now including 'check' -- but it hasn't been
            // pushed to the pool yet since it was never processed).
            cout << "\nMaster infeasible: 'check' was the last remaining "
                    "assignment and has not yet been processed; "
                    "processing it now." << endl;

            auto outcome = process_assignment(check);
            if (outcome.has_value()) {
                return *outcome;
            }

            bool any_relevant = false;
            double pool_min_lb = 0.0;
            for (const auto& e : pool_) {
                if (e.epsilon_infeasible) continue;
                if (!any_relevant || e.assignment_lb < pool_min_lb) {
                    pool_min_lb = e.assignment_lb;
                    any_relevant = true;
                }
            }
            if (any_relevant) {
                global_lb_ = std::max(global_lb_, pool_min_lb);
            }

            if (optimality_proven()) {
                announce_optimum();
                return finalize(true, false);
            }

            run_best_effort_ub_improvement(best_effort_time_limit_seconds);

            if (optimality_proven()) {
                announce_optimum();
                return finalize(true, false);
            }

            return finalize(false, false);
        }
    }
}

EpsilonResult Decomposition::run_for_epsilon(
    double epsilon,
    double best_effort_time_limit_seconds,
    double seed_ub_time_limit_seconds,
    double total_time_limit_seconds)
{
    cout << "\n==========================================" << endl;
    cout << "EPSILON-CONSTRAINT RUN" << endl;
    cout << "epsilon = " << epsilon << endl;
    cout << "Objective: min WWS" << endl;
    cout << "Constraint: Cmax <= " << epsilon << endl;
    cout << "==========================================" << endl;

    // Fresh, epsilon-specific master: constructed with the epsilon
    // workload bound baked in, and starting with ZERO no-good cuts, so
    // nothing from any other epsilon run (or from the caller's own
    // master) can leak into this one -- see the class-level comment in
    // Decomposition.h and section 19 of the epsilon-constraint spec.
    MASTER_Model fresh_master(instance_, epsilon);

    MASTER_Model* saved_master = master_;
    master_ = &fresh_master;
    cmax_epsilon_ = epsilon;

    auto start_time = chrono::steady_clock::now();
    DecompositionResult core = run(
        best_effort_time_limit_seconds,
        seed_ub_time_limit_seconds,
        total_time_limit_seconds
    );
    auto finish_time = chrono::steady_clock::now();

    // Restore the original master/epsilon state so this Decomposition
    // object remains usable afterward (e.g. for an ordinary run(), or
    // another run_for_epsilon() call with a different epsilon).
    master_ = saved_master;
    cmax_epsilon_ = -1.0;

    EpsilonResult result;
    result.epsilon = epsilon;
    result.optimality_proven = core.optimality_proven;
    result.unresolved = core.unresolved;
    result.wws_lb = core.global_lb;
    result.has_incumbent = core.has_incumbent;
    result.wws_ub = core.has_incumbent ? core.global_ub : -1.0;
    result.achieved_makespan = core.has_incumbent ? incumbent_achieved_makespan_ : -1.0;
    result.runtime_seconds = chrono::duration<double>(finish_time - start_time).count();
    result.pool_size = static_cast<int>(pool_.size());
    result.time_limit_reached = core.time_limit_reached;
    result.stats = core.stats;

    cout << "\n==========================================" << endl;
    cout << "EPSILON RESULT" << endl;
    cout << "epsilon:            " << result.epsilon << endl;
    cout << "status:             "
         << (result.optimality_proven ? "OPTIMAL"
             : result.unresolved ? "UNRESOLVED"
             : "GAP REMAINS") << endl;
    cout << "WWS LB:             " << result.wws_lb << endl;
    cout << "WWS UB:             "
         << (result.has_incumbent ? std::to_string(result.wws_ub) : string("none")) << endl;
    if (result.optimality_proven) {
        cout << "optimal WWS:        " << result.wws_ub << endl;
    }
    cout << "achieved Cmax:      "
         << (result.has_incumbent ? std::to_string(result.achieved_makespan) : string("n/a")) << endl;
    cout << "pool size:          " << result.pool_size << endl;
    cout << "runtime:            " << result.runtime_seconds << " s" << endl;
    cout << "==========================================" << endl;

    return result;
}
