#include "CP_Model.h"
#include <numeric>
#include <iostream>
#include <numeric>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>

CP_Model::~CP_Model()
{
    env_.end();
}

CP_Model::CP_Model(const DRCRFFSP_Instance& instance) {

	n_ = instance.n();
	m_ = instance.m();
	s_ = instance.s();
	r_ = instance.r() + 1;
	o_ = instance.o();
	w_ = instance.w();
	for (auto it = instance.workers_begin(); it < instance.workers_end(); it++) {
		worker_costs_.push_back(it->costs());
	}

	try {
		model_ = IloModel(env_);
		cp_ = IloCP (model_);
		//machines_ = IloIntervalVarArray2(env_, m_);
		workers_ = IloIntervalVarArray2(env_, w_);
		makespan_ = IloIntExprArray(env_);
		stages_ = IloCumulFunctionExprArray (env_, s_);
		operations_ = IloIntervalVarArray(env_, o_);
		working_ = IloBoolVarArray(env_,w_);
		costs_ = IloExprArray(env_);
		obj1_ = IloIntExpr(env_);
		obj2_ = IloExpr(env_);
		//for (int m = 0; m < m_; m++)
			//machines_[m] = IloIntervalVarArray(env_);
		for (int w = 0; w < w_; w++)
			workers_[w] = IloIntervalVarArray(env_);
		for (int s = 0; s < s_; s++)
			stages_[s] = IloCumulFunctionExpr(env_);
		int operations_counter = 0;
		for (int n = 0; n < n_; n++) {
			int number_operations_of_job = (instance.jobs_begin() + n)->number_operations();
			IloIntervalVar prec;
			for (int o = 0; o < number_operations_of_job; o++) {
				int stageId = ((instance.jobs_begin() + n)->operations_begin() + o)->stageId();
				int duration = ((instance.jobs_begin() + n)->operations_begin() + o)->duration();
				int number_workers = (instance.stage_begin() + stageId - 1)->number_workers();
				IloIntervalVar ops(env_, duration);
				IloIntervalVarArray modes(env_);
				operations_[operations_counter] = ops;
				ops.setName(to_string(operations_counter).c_str());
				for (int w = 0; w < number_workers; w++) {
					int workerId = *((instance.stage_begin() + stageId - 1)->worker_begin() + w);
					IloIntervalVar mode(env_, duration);
					string name = to_string(operations_counter) + "_" + to_string(workerId);
					const char* c = name.c_str();
					mode.setName(c);
					mode.setOptional();
					modes.add(mode);
					workers_[workerId - 1].add(mode);
				}
				model_.add(IloAlternative(env_, ops, modes));
				operations_counter += 1;
				stages_[stageId - 1] += IloPulse(ops, 1);
				if (0 != prec.getImpl())
					model_.add(IloEndBeforeStart(env_, prec, ops));
				prec = ops;
			}
			makespan_.add(IloEndOf(prec));
		}
		for (int w = 0; w < w_; w++) {
			model_.add(IloNoOverlap(env_, workers_[w]));
		}


		for (int s = 0; s < s_; s++) {
			int number_machines = (instance.stage_begin() + s)->number_machines();
			model_.add(stages_[s] <= number_machines);
		}

		//At least one operation starts at time 0
		IloExpr sum(env_);
		for (int o = 0; o < o_; o++) {
			sum += (IloStartOf(operations_[o]) == 0);   
		}
		model_.add(sum >= 1);

		//Objective Function 1 Makespan
		//objeps1_ = IloMinimize(env_, IloMax(makespan_));
		//model_.add(objeps1_);

		// No-idle constraint: the sum of the working time on the machines of 
		//each machine must be exactly the processing time on that machine (case with no machine flexibility)
		/*for (int m = 0; m < m_; m++) {
			IloExpr expr(env_);
			IloIntExprArray machine_begin(env_);
			IloIntExprArray machine_end(env_);
			for (int mode = 0; mode < machines_[m].getSize(); mode++) {
				machine_begin.add(IloStartOf(machines_[m][mode], instance.sumpr()));
				machine_end.add(IloEndOf(machines_[m][mode]));
			}
			expr += (IloMax(machine_end) - IloMin(machine_begin));
			model_.add(expr <= *(instance.processing_times_per_stage().begin() + m));
		}*/
		
		//Objective Function 1 Makespan
		//objective_ = IloMinimize(env_, IloMax(makespan_));
		//Calculating second objective of worker costs
	
		/*for (int w = 0; w < w_; w++) {
			IloExpr expr(env_);
			IloIntExprArray worker_begin(env_);
			IloIntExprArray worker_end(env_);
			for (int mode = 0; mode < workers_[w].getSize(); mode++) {
				//Default is H, if Mode is not present 
				worker_begin.add(IloStartOf(workers_[w][mode],instance.sumpr()));
				//Default is 0, if Mode is not present 
				worker_end.add(IloEndOf(workers_[w][mode]));
				expr += IloPresenceOf(env_, workers_[w][mode]);
			} 
			//model_.add(working_[w] <= expr);
			model_.add(working_[w] * instance.sumpr() >= expr);
			costs_.add(working_[w] * 1000);
			//costs_.add((IloMax(worker_end) - IloMin(worker_begin)) * worker_costs_[w] * working_[w]);
		}*/

		//With span constraint
		for (int w = 0; w < w_; ++w) {
			// 0/1 variable: 1 if worker w is used

			// Optional span for this worker
			IloIntervalVar span(env_);
			span.setOptional();

			// Collect this worker's (optional) mode intervals
			IloIntervalVarArray worker_modes(env_);
			for (int mode = 0; mode < workers_[w].getSize(); ++mode) {
				// Ensure mode intervals are optional
				worker_modes.add(workers_[w][mode]);
			}

			// Span over present modes
			model_.add(IloSpan(env_, span, worker_modes));

			// Presence linking: worker used if span present
			//model_.add(IloPresenceOf(env_, span) == working_[w]);

			// Capacity/linking (only matters when worker is used)
			//model_.add(IloSizeOf(span) <= working_[w] * instance.sumpr());

			// Costs: variable cost + fixed activation cost
			costs_.add(IloSizeOf(span) * worker_costs_[w]);  // variable/length cost
			//costs_.add(working_[w] * 1000);                  // fixed cost per used worker
		}



		//second_objective_ = IloMinimize(env_, IloSum(costs_));
		/*IloNumExprArray objArray(env_);
		obj1_ = IloMax(makespan_);
		obj2_ = IloSum(costs_);
		objArray.add(obj1_);
		objArray.add(obj2_);
		
		IloMultiCriterionExpr biExpr = IloStaticLex(env_, objArray);
		static_lex_ = IloMinimize(env_,biExpr);
		model_.add(static_lex_);*/

		cp_ = IloCP(model_);
		cp_.setParameter(IloCP::TimeLimit, 300);

		// single-threaded solving
		cp_.setParameter(IloCP::Workers, 1);
		//cp_.setOut(env_.getNullStream());
		//cp_.setParameter(IloCP::LogVerbosity, IloCP::Terse);
		//cp_.setParameter(IloCP::LogPeriod, 10000);
		//Set time limit to 1 hour
		//cp_.setParameter(IloCP::HeurFreq, -1);
		//cp_.setParameter(IloCP::TimeLimit,600);
		//Set emphasis on finding a feasible rather than an optimal solution
		//cp_.setParameter(IloCP::Emphasis::MIP,1);
		//cp_.setParam(IloCplex::Param::MIP::Display,2);
		//cp_.setParam(IloCplex::Param::MIP::Interval, 2);
		
	}
	catch (IloException& e) {
		cerr << "Error creating model: " << e.getMessage() << endl;
		e.end();
	}

}

CP_Model::CP_Model(
    const DRCRFFSP_Instance& instance,
    const vector<int>& worker_assignment)
{
    try {

        // ------------------------------------------------------------
        // 1. Instance data
        // ------------------------------------------------------------

        n_ = instance.n();
        m_ = instance.m();
        s_ = instance.s();
        r_ = instance.r() + 1;
        o_ = instance.o();
        w_ = instance.w();

        for (auto it = instance.workers_begin();
             it != instance.workers_end();
             ++it) {

            worker_costs_.push_back(it->costs());
        }


        // ------------------------------------------------------------
        // 2. Check assignment vector
        // ------------------------------------------------------------

        if (worker_assignment.size() != static_cast<size_t>(o_)) {

            throw runtime_error(
                "worker_assignment size does not match number of operations"
            );
        }


        // ------------------------------------------------------------
        // 3. Initialize Concert objects
        // ------------------------------------------------------------

        model_ = IloModel(env_);

        workers_ = IloIntervalVarArray2(env_, w_);
        makespan_ = IloIntExprArray(env_);
        stages_ = IloCumulFunctionExprArray(env_, s_);
        operations_ = IloIntervalVarArray(env_, o_);

        working_ = IloBoolVarArray(env_, w_);

        costs_ = IloExprArray(env_);

        obj1_ = IloIntExpr(env_);
        obj2_ = IloExpr(env_);


        // Worker interval arrays
        for (int w = 0; w < w_; ++w) {

            workers_[w] = IloIntervalVarArray(env_);
        }

        // Stage cumulative functions
        for (int s = 0; s < s_; ++s) {

            stages_[s] = IloCumulFunctionExpr(env_);
        }


        // ------------------------------------------------------------
        // 4. Create operations
        //
        // IMPORTANT:
        // Worker assignment is fixed by worker_assignment.
        // CP Optimizer is NOT allowed to choose another worker.
        // ------------------------------------------------------------

        int operations_counter = 0;

        for (int n = 0; n < n_; ++n) {

            int number_operations_of_job =
                (instance.jobs_begin() + n)->number_operations();

            IloIntervalVar prec;

            for (int o = 0;
                 o < number_operations_of_job;
                 ++o) {

                const Operation& current_operation =
                    *((instance.jobs_begin() + n)->operations_begin() + o);

                int stageId =
                    current_operation.stageId();

                int duration =
                    static_cast<int>(current_operation.duration());


                // ----------------------------------------------------
                // Master assignment for this global operation
                // ----------------------------------------------------

                int assigned_worker =
                    worker_assignment[operations_counter];


                // Validate worker index
                if (assigned_worker < 0 ||
                    assigned_worker >= w_) {

                    throw runtime_error(
                        "Invalid worker assignment for operation "
                        + to_string(operations_counter)
                    );
                }


                // Validate eligibility
                if (!instance.is_worker_eligible(
                        operations_counter,
                        assigned_worker)) {

                    throw runtime_error(
                        "Ineligible worker assignment for operation "
                        + to_string(operations_counter)
                        + " -> worker "
                        + to_string(assigned_worker)
                    );
                }


                // ----------------------------------------------------
                // Create mandatory operation interval
                // ----------------------------------------------------

                IloIntervalVar ops(env_, duration);

                operations_[operations_counter] = ops;

                string operation_name =
                    to_string(operations_counter);

                ops.setName(operation_name.c_str());


                // ----------------------------------------------------
                // FIX worker assignment
                //
                // The same operation interval is directly inserted
                // into the assigned worker's NoOverlap structure.
                // ----------------------------------------------------

                workers_[assigned_worker].add(ops);


                // ----------------------------------------------------
                // Stage capacity
                // ----------------------------------------------------

                stages_[stageId - 1]
                    += IloPulse(ops, 1);


                // ----------------------------------------------------
                // Job precedence
                // ----------------------------------------------------

                if (0 != prec.getImpl()) {

                    model_.add(
                        IloEndBeforeStart(
                            env_,
                            prec,
                            ops
                        )
                    );
                }

                prec = ops;


                ++operations_counter;
            }


            // Completion of last operation of this job
            makespan_.add(
                IloEndOf(prec)
            );
        }


        // ------------------------------------------------------------
        // 5. Verify all operations were created
        // ------------------------------------------------------------

        if (operations_counter != o_) {

            throw runtime_error(
                "Number of constructed operations does not match instance.o()"
            );
        }


        // ------------------------------------------------------------
        // 6. Worker capacity
        //
        // Each worker can process at most one operation at a time.
        // ------------------------------------------------------------

        for (int w = 0; w < w_; ++w) {

            if (workers_[w].getSize() > 0) {

                model_.add(
                    IloNoOverlap(
                        env_,
                        workers_[w]
                    )
                );
            }
        }


        // ------------------------------------------------------------
        // 7. Stage / machine capacity
        // ------------------------------------------------------------

        for (int s = 0; s < s_; ++s) {

            int number_machines =
                (instance.stage_begin() + s)->number_machines();

            model_.add(
                stages_[s] <= number_machines
            );
        }


        // ------------------------------------------------------------
        // 8. Symmetry / normalization constraint
        //
        // At least one operation starts at time 0.
        // Same as integrated model.
        // ------------------------------------------------------------

        IloExpr sum(env_);

        for (int o = 0; o < o_; ++o) {

            sum +=
                (IloStartOf(operations_[o]) == 0);
        }

        model_.add(sum >= 1);

        sum.end();


        // ------------------------------------------------------------
        // 9. Weighted Worker Span
        //
        // T_w = end of last assigned operation
        //       - start of first assigned operation
        //
        // WWS = sum_w c_w * T_w
        // ------------------------------------------------------------

        for (int w = 0; w < w_; ++w) {

            // A worker may receive no operation from the master.
            if (workers_[w].getSize() == 0) {

                continue;
            }


            // workers_[w] holds only mandatory intervals in this
            // fixed-assignment model, so the span they define is always
            // present too; leaving it mandatory (not .setOptional())
            // avoids a spurious CP Optimizer warning without changing
            // the computed span size.
            IloIntervalVar span(env_);

            IloIntervalVarArray worker_operations(env_);

            for (int mode = 0;
                 mode < workers_[w].getSize();
                 ++mode) {

                worker_operations.add(
                    workers_[w][mode]
                );
            }


            model_.add(
                IloSpan(
                    env_,
                    span,
                    worker_operations
                )
            );


            costs_.add(
                IloSizeOf(span)
                * worker_costs_[w]
            );
        }


        // ------------------------------------------------------------
        // 10. Create CP Optimizer
        // ------------------------------------------------------------

        cp_ = IloCP(model_);


        // ------------------------------------------------------------
        // 11. Solver parameters
        // ------------------------------------------------------------

        cp_.setParameter(
            IloCP::TimeLimit,
            3600
        );

        cp_.setParameter(
            IloCP::Workers,
            1
        );

        // Optional:
        // cp_.setOut(env_.getNullStream());


        cout << "\nCP decomposition subproblem created successfully."
             << endl;
    }

    catch (IloException& e) {

        cerr << "Error creating CP subproblem: "
             << e.getMessage()
             << endl;

        throw;
    }

    catch (const exception& e) {

        cerr << "Error creating CP subproblem: "
             << e.what()
             << endl;

        throw;
    }
}

ZeroIdleResult CP_Model::solve_zero_idle_feasibility(double cmax_epsilon)
{
    // ------------------------------------------------------------
    // Temporarily add, for every USED worker w:
    //     IloSizeOf(span_w) == assigned_load[w]
    //
    // assigned_load[w] is derived directly from the fixed-duration
    // interval variables already stored in workers_[w] (no separate
    // bookkeeping needed: these intervals were created with a fixed
    // size, so getSizeMin() returns their duration).
    //
    // The span itself is built exactly like the WWS span in the
    // constructor (IloSpan over the same worker_operations), but as
    // a fresh local interval variable / constraint so nothing here
    // is stored as a permanent member of the model.
    // ------------------------------------------------------------

    struct WorkerSpanInfo {
        int worker_id;
        IloInt assigned_load;
        IloIntervalVar span;
    };

    vector<IloConstraint> added_constraints;
    vector<WorkerSpanInfo> worker_spans;

    try {

        cout << "\n--- ZERO-IDLE FEASIBILITY CHECK ---" << endl;

        if (cmax_epsilon > 0) {

            cout << "epsilon = " << cmax_epsilon
                 << "  (enforcing Cmax <= epsilon)" << endl;

            IloConstraint cmax_constraint = (IloMax(makespan_) <= cmax_epsilon);
            model_.add(cmax_constraint);
            added_constraints.push_back(cmax_constraint);
        }

        for (int w = 0; w < w_; ++w) {

            if (workers_[w].getSize() == 0) {
                // Unused worker: no zero-idle constraint.
                continue;
            }

            IloInt assigned_load = 0;

            IloIntervalVarArray worker_operations(env_);

            for (int mode = 0; mode < workers_[w].getSize(); ++mode) {

                worker_operations.add(workers_[w][mode]);
                assigned_load += workers_[w][mode].getSizeMin();
            }

            // workers_[w] holds only mandatory intervals here too, so
            // the span is always present; leaving it mandatory avoids
            // the same spurious CP Optimizer warning.
            IloIntervalVar span(env_);

            IloConstraint span_constraint =
                IloSpan(env_, span, worker_operations);

            IloConstraint zero_idle_constraint =
                (IloSizeOf(span) == assigned_load);

            model_.add(span_constraint);
            model_.add(zero_idle_constraint);

            added_constraints.push_back(span_constraint);
            added_constraints.push_back(zero_idle_constraint);

            worker_spans.push_back({ w, assigned_load, span });
        }

        // Pure feasibility solve: no makespan/WWS objective is added here
        // (Cmax <= epsilon above, when present, is a CONSTRAINT, not an
        // objective -- this never minimizes Cmax).
        cp_.solve();

        IloAlgorithm::Status status = cp_.getStatus();

        ZeroIdleResult result;
        result.status = FeasibilityStatus::UNKNOWN;
        result.achieved_makespan = -1.0;

        if (status == IloAlgorithm::Infeasible) {

            cout << "Status: INFEASIBLE" << endl;
            result.status = FeasibilityStatus::INFEASIBLE;
        }
        else if (status == IloAlgorithm::Feasible ||
                 status == IloAlgorithm::Optimal) {

            cout << "Status: FEASIBLE" << endl;
            result.status = FeasibilityStatus::FEASIBLE;
            result.achieved_makespan = cp_.getValue(IloMax(makespan_));

            cout << "Achieved Cmax: " << result.achieved_makespan << endl;

            for (const auto& info : worker_spans) {

                IloInt span_size = cp_.getSize(info.span);

                cout << "worker " << info.worker_id
                     << ": load = " << info.assigned_load
                     << ", span = " << span_size
                     << endl;
            }
        }
        else {

            // Time limit reached without a solution, or otherwise
            // inconclusive: this is NOT a proof of infeasibility.
            cout << "Status: UNKNOWN (solver status = " << status
                 << "; not proven infeasible, e.g. time limit reached)"
                 << endl;
            result.status = FeasibilityStatus::UNKNOWN;
        }

        // ------------------------------------------------------------
        // Remove the temporary zero-idle (and, in epsilon mode, Cmax)
        // constraints so the ordinary decomposition subproblem (solve(),
        // solve_obj(), solve_static_lex()) is unaffected by this check.
        // ------------------------------------------------------------
        for (auto& c : added_constraints) {
            model_.remove(c);
        }

        return result;
    }
    catch (IloException& e) {

        cerr << "Error in zero-idle feasibility check: "
             << e.getMessage()
             << endl;

        for (auto& c : added_constraints) {
            model_.remove(c);
        }

        ZeroIdleResult result;
        result.status = FeasibilityStatus::UNKNOWN;
        result.achieved_makespan = -1.0;
        return result;
    }
}

FeasibilityStatus CP_Model::solve_feasibility_under_epsilon(double cmax_epsilon)
{
    // Pure fixed-assignment scheduling feasibility: precedence, no-overlap,
    // stage capacity -- everything already baked into model_ by the
    // fixed-assignment constructor -- plus ONLY Cmax <= epsilon. No
    // zero-idle equality, no WWS bound, no objective. See CP_Model.h for
    // why/when to call this (disambiguating zero-idle-infeasible from
    // completely-infeasible-under-epsilon).

    vector<IloConstraint> added_constraints;

    try {

        cout << "\n--- FEASIBILITY UNDER EPSILON (no zero-idle) ---" << endl;
        cout << "epsilon = " << cmax_epsilon << endl;

        IloConstraint cmax_constraint = (IloMax(makespan_) <= cmax_epsilon);
        model_.add(cmax_constraint);
        added_constraints.push_back(cmax_constraint);

        cp_.solve();

        IloAlgorithm::Status status = cp_.getStatus();

        FeasibilityStatus result = FeasibilityStatus::UNKNOWN;

        if (status == IloAlgorithm::Infeasible) {

            cout << "Status: INFEASIBLE (no schedule exists for this "
                    "assignment under this epsilon at all)" << endl;
            result = FeasibilityStatus::INFEASIBLE;
        }
        else if (status == IloAlgorithm::Feasible ||
                 status == IloAlgorithm::Optimal) {

            cout << "Status: FEASIBLE (a schedule exists under this "
                    "epsilon, just not a zero-idle one)" << endl;
            result = FeasibilityStatus::FEASIBLE;
        }
        else {

            cout << "Status: UNKNOWN (solver status = " << status
                 << "; not proven infeasible, e.g. time limit reached)"
                 << endl;
            result = FeasibilityStatus::UNKNOWN;
        }

        for (auto& c : added_constraints) {
            model_.remove(c);
        }

        return result;
    }
    catch (IloException& e) {

        cerr << "Error in feasibility-under-epsilon check: "
             << e.getMessage()
             << endl;

        for (auto& c : added_constraints) {
            model_.remove(c);
        }

        return FeasibilityStatus::UNKNOWN;
    }
}

ThresholdFeasibilityResult CP_Model::solve_wws_below_threshold(
    double threshold,
    double epsilon,
    double cmax_epsilon)
{
    // ------------------------------------------------------------
    // Temporarily add:
    //     WWS <= threshold - epsilon                          (global, unchanged)
    //     for every used worker w: a span/idle bound derived
    //     from the remaining integer idle-time budget Delta    (new, additive)
    //
    // WWS is exactly the same expression already used for the WWS cost
    // in the constructor: IloSum(costs_), where costs_[w] ==
    // IloSizeOf(span_w) * worker_costs_[w] for every used worker w.
    // costs_ is a permanent model member (built once in the constructor),
    // so it is only READ here, not rebuilt.
    //
    // "<=" with an epsilon tolerance implements the strict "<" from the
    // math spec: worker_costs_ is a vector<float>, so WWS need not be
    // integer-valued in general and a bare "<" against floating results
    // would be numerically fragile. This global constraint is kept
    // exactly as before; the worker-specific bounds below are added IN
    // ADDITION to it, purely to help CP propagation -- they do not by
    // themselves enforce the total idle budget (see Delta derivation).
    // ------------------------------------------------------------

    ThresholdFeasibilityResult result;
    result.status = FeasibilityStatus::UNKNOWN;
    result.wws_value = -1.0;
    result.achieved_makespan = -1.0;

    // ------------------------------------------------------------
    // Recompute L(A) = sum_w c_w * assigned_load[w] as an exact integer,
    // directly from this CP_Model's own data (workers_, worker_costs_) --
    // the identical formula the master uses for this exact fixed
    // assignment, so this needs no value passed in from the caller.
    // ------------------------------------------------------------

    struct UsedWorkerInfo {
        int worker_id;
        int cost;
        int load;
    };

    vector<UsedWorkerInfo> used_workers;
    long long master_value_int = 0;

    for (int w = 0; w < w_; ++w) {

        if (workers_[w].getSize() == 0) {
            continue;
        }

        int load = 0;
        for (int mode = 0; mode < workers_[w].getSize(); ++mode) {
            load += static_cast<int>(workers_[w][mode].getSizeMin());
        }

        int cost = static_cast<int>(worker_costs_[w]);

        used_workers.push_back({ w, cost, load });
        master_value_int += static_cast<long long>(cost) * load;
    }

    // ------------------------------------------------------------
    // Delta = (largest integer WWS allowed by the global constraint
    // above) - L(A). Derived from the SAME threshold/epsilon pair used
    // for the global constraint (floor with a small guard against
    // floating noise), so this stays consistent however the caller
    // phrases the bound: threshold == 200 with the default epsilon means
    // "WWS < 200" (Delta = 199 - L(A)); threshold == 200 + epsilon means
    // "WWS <= 200" (Delta = 200 - L(A)). Both reduce to the math spec's
    // "Delta = threshold - 1 - L(A)" when threshold is itself an integer.
    // ------------------------------------------------------------

    const double float_guard = 1e-9;

    long long max_allowed_wws = static_cast<long long>(
        std::floor(threshold - epsilon + float_guard)
    );

    long long delta = max_allowed_wws - master_value_int;

    cout << "\n--- PHASE 2 IDLE BUDGET ---" << endl;
    cout << "Master value: " << master_value_int << endl;
    cout << "Threshold:    " << threshold << endl;
    cout << "Idle budget:  " << delta << endl;

    if (delta < 0) {

        // WWS(A) < threshold is already impossible from the
        // productive-cost value alone -- no CP call needed.
        cout << "Delta < 0: WWS < threshold is impossible from the "
                "productive-cost value alone." << endl;

        result.status = FeasibilityStatus::INFEASIBLE;
        return result;
    }

    vector<IloConstraint> added_constraints;

    try {

        IloExpr wws = IloSum(costs_);

        IloConstraint threshold_constraint = (wws <= threshold - epsilon);
        model_.add(threshold_constraint);
        added_constraints.push_back(threshold_constraint);

        if (cmax_epsilon > 0) {

            cout << "epsilon = " << cmax_epsilon
                 << "  (enforcing Cmax <= epsilon)" << endl;

            IloConstraint cmax_constraint = (IloMax(makespan_) <= cmax_epsilon);
            model_.add(cmax_constraint);
            added_constraints.push_back(cmax_constraint);
        }

        // ------------------------------------------------------------
        // Worker-specific span/idle bounds (additive, not a replacement
        // for the global constraint above -- see IMPORTANT SPECIAL CASE
        // in CP_Model.h). Only used workers get a bound; an unused worker
        // has P_w = 0 and needs none. Each span here is a fresh local
        // interval variable over that worker's own (mandatory) operation
        // set, exactly like the equivalent construction in
        // solve_zero_idle_feasibility() -- so it is guaranteed to equal
        // that worker's true span in any solution, without touching the
        // constructor's own permanent WWS span.
        // ------------------------------------------------------------

        for (const auto& info : used_workers) {

            // cost > 0 is guaranteed: Worker::costs() is a positive int
            // (see Worker.h), so this integer division is always safe.
            long long max_idle = delta / info.cost;

            IloIntervalVarArray worker_operations(env_);

            for (int mode = 0;
                 mode < workers_[info.worker_id].getSize();
                 ++mode) {

                worker_operations.add(workers_[info.worker_id][mode]);
            }

            IloIntervalVar span(env_);

            IloConstraint span_constraint =
                IloSpan(env_, span, worker_operations);
            model_.add(span_constraint);
            added_constraints.push_back(span_constraint);

            IloConstraint idle_bound_constraint;

            cout << "\nWorker " << info.worker_id << ":" << endl;
            cout << "    cost:      " << info.cost << endl;
            cout << "    load:      " << info.load << endl;
            cout << "    max idle:  " << max_idle << endl;

            if (max_idle <= 0) {

                // Delta < cost(w): worker w must have zero idle. Prefer
                // the stronger equality over "<= load + 0" to help
                // propagation, exactly as for the zero-idle check.
                idle_bound_constraint = (IloSizeOf(span) == info.load);

                cout << "    span ==    " << info.load << endl;
            }
            else {

                idle_bound_constraint =
                    (IloSizeOf(span) <= info.load + max_idle);

                cout << "    span <=    " << (info.load + max_idle) << endl;
            }

            model_.add(idle_bound_constraint);
            added_constraints.push_back(idle_bound_constraint);
        }

        cout << "\nGlobal constraint:" << endl;
        cout << "    WWS <= " << max_allowed_wws << endl;

        // Pure feasibility solve: WWS is only bounded, never minimized.
        cp_.solve();

        IloAlgorithm::Status status = cp_.getStatus();

        if (status == IloAlgorithm::Infeasible) {

            cout << "Status: INFEASIBLE" << endl;
            result.status = FeasibilityStatus::INFEASIBLE;
        }
        else if (status == IloAlgorithm::Feasible ||
                 status == IloAlgorithm::Optimal) {

            double wws_value = cp_.getValue(wws);
            double achieved_makespan = cp_.getValue(IloMax(makespan_));

            cout << "Status: FEASIBLE" << endl;
            cout << "Feasible WWS: " << wws_value << endl;
            cout << "Achieved Cmax: " << achieved_makespan << endl;

            result.status = FeasibilityStatus::FEASIBLE;
            result.wws_value = wws_value;
            result.achieved_makespan = achieved_makespan;
        }
        else {

            // Time limit reached without a solution, or otherwise
            // inconclusive: this is NOT a proof that WWS >= threshold.
            cout << "Status: UNKNOWN (solver status = " << status
                 << "; not proven infeasible, e.g. time limit reached)"
                 << endl;
            result.status = FeasibilityStatus::UNKNOWN;
        }

        wws.end();

        // ------------------------------------------------------------
        // Remove every temporary constraint added above (global bound +
        // all worker-specific spans/bounds) so the ordinary decomposition
        // subproblem (solve(), solve_obj(), solve_static_lex(),
        // solve_zero_idle_feasibility()) and any later call to this same
        // method with a different threshold are unaffected -- nothing
        // from this call is ever left behind in model_.
        // ------------------------------------------------------------
        for (auto& c : added_constraints) {
            model_.remove(c);
        }

        return result;
    }
    catch (IloException& e) {

        cerr << "Error in WWS threshold feasibility check: "
             << e.getMessage()
             << endl;

        for (auto& c : added_constraints) {
            model_.remove(c);
        }

        return result;
    }
}

BestEffortWWSResult CP_Model::solve_wws_best_effort(
    double time_limit_seconds,
    double cmax_epsilon)
{
    // Permanently adds a real minimization objective to model_ and lowers
    // the CP time limit -- both safe only because this object is a
    // fresh, single-use CP_Model created just for this one solve (see the
    // doc comment in CP_Model.h and Decomposition::run_best_effort_ub_
    // improvement()). No temporary-constraint add/remove pattern is
    // needed here, unlike solve_zero_idle_feasibility()/
    // solve_wws_below_threshold(), since the object is never reused
    // afterward for anything else.

    BestEffortWWSResult result;
    result.status = FeasibilityStatus::UNKNOWN;
    result.wws_value = -1.0;
    result.proven_optimal = false;
    result.achieved_makespan = -1.0;

    try {

        // Give CP a concrete, well-propagated bound to search against.
        // A bare WWS-sum objective gets much weaker propagation than a
        // makespan objective (CP Optimizer's scheduling propagation --
        // edge-finding, energy reasoning -- is built around makespan-like
        // quantities), so without this the solve's own reported lower
        // bound can stay completely stuck for the whole time limit even
        // on a tiny fixed-assignment model. In epsilon mode, the real
        // epsilon bound IS this constraint (tighter and equally valid for
        // propagation); otherwise fall back to the loose, always-
        // satisfiable sum-of-processing-times bound as before.
        if (cmax_epsilon > 0) {

            cout << "epsilon = " << cmax_epsilon
                 << "  (enforcing Cmax <= epsilon)" << endl;

            model_.add(IloMax(makespan_) <= cmax_epsilon);
        }
        else {

            int sum_processing_time = 0;

            for (int o = 0; o < o_; ++o) {
                sum_processing_time += static_cast<int>(operations_[o].getSizeMin());
            }

            model_.add(IloMax(makespan_) <= sum_processing_time);
        }

        model_.add(IloMinimize(env_, IloSum(costs_)));

        cp_.setParameter(IloCP::TimeLimit, time_limit_seconds);

        cp_.solve();

        IloAlgorithm::Status status = cp_.getStatus();

        if (status == IloAlgorithm::Optimal) {

            result.status = FeasibilityStatus::FEASIBLE;
            result.wws_value = cp_.getObjValue();
            result.proven_optimal = true;
            result.achieved_makespan = cp_.getValue(IloMax(makespan_));
        }
        else if (status == IloAlgorithm::Feasible) {

            result.status = FeasibilityStatus::FEASIBLE;
            result.wws_value = cp_.getObjValue();
            result.proven_optimal = false;
            result.achieved_makespan = cp_.getValue(IloMax(makespan_));
        }
        else if (status == IloAlgorithm::Infeasible) {

            // Without epsilon: should not happen for a valid fixed
            // assignment (some schedule always exists -- e.g. run
            // everything sequentially), handled defensively rather than
            // assumed away. WITH epsilon (cmax_epsilon > 0), this is a
            // real, meaningful outcome: it means NO schedule exists for
            // this fixed assignment with Cmax <= epsilon at all -- the
            // same "completely infeasible under epsilon" case as
            // CP_Model::solve_feasibility_under_epsilon(); callers in
            // epsilon mode should treat this as a genuine proof, not a
            // defensive/unexpected branch.
            result.status = FeasibilityStatus::INFEASIBLE;
        }
        else {

            // Time limit reached without any incumbent.
            result.status = FeasibilityStatus::UNKNOWN;
        }

        return result;
    }
    catch (IloException& e) {

        cerr << "Error in best-effort WWS solve: "
             << e.getMessage()
             << endl;

        return result;
    }
}

tuple<float,float> CP_Model::solve_static_lex(double time_limit_seconds, CPSolveInfo* info) {
	if (time_limit_seconds > 0) {
		cp_.setParameter(IloCP::TimeLimit, time_limit_seconds);
	}
	IloNumExprArray objArray(env_);
	obj1_ = IloMax(makespan_);
	obj2_ = IloSum(costs_);
	objArray.add(obj1_);
	objArray.add(obj2_);

	IloMultiCriterionExpr biExpr = IloStaticLex(env_, objArray);
	static_lex_ = IloMinimize(env_, biExpr);
	model_.add(static_lex_);
	float opt_1 = -1.0;
	float opt_2 = -1.0;
	try {
		auto start = cp_.getTime();
		cp_.solve();
		auto elapsed = cp_.getTime() - start;
		if (info) {
			info->status = cp_.getStatus();
			info->solve_time_sec = elapsed;
		}
		cout << cp_.getStatus() << endl;
		if (cp_.getStatus() == IloAlgorithm::Feasible) {
			opt_1 = cp_.getValue(obj1_);
			opt_2 = cp_.getValue(obj2_);
			cout << "Feasible Solution with makesspan: " << opt_1 << " and costs: " << opt_2 << endl;
		}
		else if  (cp_.getStatus() == IloAlgorithm::Optimal) {
			opt_1 = cp_.getValue(obj1_);
			opt_2 = cp_.getValue(obj2_);
			cout << "Optimal Solution with makesspan: " << opt_1 << " and costs: " << opt_2 << endl;
		}
		else {
			cout << "No feasible solution found" << endl;
		}

		if (cp_.getStatus() == IloAlgorithm::Feasible ||
			cp_.getStatus() == IloAlgorithm::Optimal) {

			cout << "\n--- WORKER USAGE ---" << endl;

			for (int w = 0; w < w_; ++w) {

				bool used = false;

				for (int mode = 0; mode < workers_[w].getSize(); ++mode) {

					if (cp_.isPresent(workers_[w][mode])) {
						used = true;
						break;
					}
				}

				cout << "worker " << w << ": "
					 << (used ? "used" : "NOT used") << endl;
			}
		}
	}
	catch (IloException& e) {
		cerr << "Error solving model: " << e.getMessage() << endl;
		e.end();
	}
	//Export model to lp file
	//cp_.exportModel("DRCRFFSP.lp");
	return make_tuple(opt_1,opt_2);
}

float CP_Model::solve_obj(const DRCRFFSP_Instance& instance,int i, double cmax_epsilon, CPSolveInfo* info) {
	if (cmax_epsilon > 0) {
		// Permanent Cmax <= cmax_epsilon constraint -- the "CP-WWS-EPS"
		// experiment method (min WWS s.t. Cmax<=epsilon) when combined
		// with i==2. Safe as a permanent addition: like the objective
		// itself, this method is meant to be called once on a freshly
		// constructed CP_Model, exactly like every existing call site
		// already does.
		model_.add(IloMax(makespan_) <= cmax_epsilon);
	}
	objeps1_ = IloMinimize(env_, IloMax(makespan_));
	objeps2_ = IloMinimize(env_, IloSum(costs_));
	if (i == 1){
		model_.add(objeps1_);
		//IloExpr expr_1 = objeps1_.getExpr();
		//IloRange cons_f1;
		/////////////////////////////////////////////////////////
		//cons_f1 = (expr_1 <= instance.sumpr());
		//cons_f1 = (expr_1 >= 380);
		//model_.add(cons_f1);
		//IloExpr expr_1 = objeps2_.getExpr();
		//IloRange cons_f1;
		//cons_f1 = (expr_1 <= 380);
		//model_.add(cons_f1);
	}
	if (i == 2) {
		model_.add(objeps2_);
		IloExpr expr_1 = objeps1_.getExpr();
		IloRange cons_f1;
		/////////////////////////////////////////////////////////
		cons_f1 = (expr_1 <= instance.sumpr());
		model_.add(cons_f1);
		////////////////////////////////////////////////////////
	}

	
	float opt = -1;
	try {
		auto start = cp_.getTime();
		cp_.solve();
		auto elapsed = cp_.getTime() - start;
		//solving_time.push_back(elapsed);
		if (info) {
			info->status = cp_.getStatus();
			info->solve_time_sec = elapsed;
		}
		if (cp_.getStatus() == IloAlgorithm::Optimal) {
			opt = cp_.getObjValue();
			cout << "Optimal Solution found in \t" << elapsed << " seconds" << endl;
		}
		else if (cp_.getStatus() == IloAlgorithm::Feasible) {
			opt = cp_.getObjValue();
			cout << "Feasible Solution found in \t" << elapsed << " seconds" << endl;
		}
		else {
			cout << "No feasible solution found" << endl;
		}
		 float makespan_value =
        cp_.getValue(IloMax(makespan_));

    	float wws_value =
        cp_.getValue(IloSum(costs_));

    	cout << "Makespan = " << makespan_value << endl;
    	cout << "WWS      = " << wws_value << endl;

		if (info) {
			info->bound = cp_.getObjBound();
			info->secondary_value = (i == 1) ? wws_value : makespan_value;
		}
	}
	catch (IloException& e) {
		cerr << "Error solving model: " << e.getMessage() << endl;
		e.end();
	}
	return opt;
}

float CP_Model::solve() {
	float opt = -1;
	float bound = -1;
	try {
		auto start = cp_.getTime();
		cp_.solve();
		auto elapsed = cp_.getTime() - start;
		//solving_time.push_back(elapsed);
		if (cp_.getStatus() == IloAlgorithm::Optimal) {
			opt = cp_.getObjValue();
			bound = cp_.getObjBound();
			cout << "Optimal Solution found in \t" << elapsed << " seconds" << ", Objective value: " << opt << ", Best Bound: " << bound <<  endl;
		}
		else if (cp_.getStatus() == IloAlgorithm::Feasible) {
			opt = cp_.getObjValue();
			bound = cp_.getObjBound();
			cout << "Feasible Solution found in \t" << elapsed << " seconds" << ", Objective value: " << opt << ", Best Bound: " << bound << endl;
		}
		else {
			cout << "No feasible solution found" << endl;
		}
	}
	catch (IloException& e) {
		cerr << "Error solving model: " << e.getMessage() << endl;
		e.end();
	}
	return opt;
}

pair<float, float> CP_Model::solve_one_after_another(const DRCRFFSP_Instance& instance) {
	//first solving the makespan objective, then giving this solution as a starting solution for optimizing the costs. 
	//However we do not set a bound on the makespan 
	objeps1_ = IloMinimize(env_, IloMax(makespan_));
	objeps2_ = IloMinimize(env_, IloSum(costs_));
	float z1, z2;
	model_.add(objeps1_);
	pair<float, float> opt_pair;
	try {
		auto start = cp_.getTime();
		cp_.setParameter(IloCP::TimeLimit, 100);
		cp_.solve();
		auto elapsed = cp_.getTime() - start;
		//solving_time.push_back(elapsed);
		if (cp_.getStatus() == IloAlgorithm::Optimal) {
			z1 = cp_.getObjValue();
			cout << "Optimal Solution found for cmax in \t" << elapsed << " seconds" << endl;
		}
		else if (cp_.getStatus() == IloAlgorithm::Feasible) {
			z1 = cp_.getObjValue();
			cout << "Feasible Solution found for cmax in \t" << elapsed << " seconds" << endl;
		}
		else {
			cout << "No feasible solution found for cmax" << endl;
		}
	}
	catch (IloException& e) {
		cerr << "Error solving model 1: " << e.getMessage() << endl;
		e.end();
	}

	IloSolution sol_1(env_);
	/*for (int w = 0; w < w_; w++) {
		sol_1.setValue(working_[w], cp_.getValue(working_[w]));
	}/*

	/*for (int m = 0; m < m_; m++) {
		for (int mode = 0; mode < machines_[m].getSize(); mode++) {
			if (cp_.isPresent(machines_[m][mode]) == true) {
				sol_1.setPresent(machines_[m][mode]);
				sol_1.setStart(machines_[m][mode], cp_.getStart(machines_[m][mode]));
			}
		}
	}*/

	for(int o = 0; o < o_; o++){
		sol_1.setStart(operations_[o], cp_.getStart(operations_[o]));
	}


	for (int w = 0; w < w_; w++) {
		for (int mode = 0; mode < workers_[w].getSize(); mode++) {
			if (cp_.isPresent(workers_[w][mode]) == true) {
				sol_1.setPresent(workers_[w][mode]);
				sol_1.setStart(workers_[w][mode], cp_.getStart(workers_[w][mode]));
			}
		}
	}

	cp_.setStartingPoint(sol_1);
	//changing the Objecitve from makespan to costs 
	model_.remove(objeps1_);
	model_.add(objeps2_);
	IloRange cons_f1;
	IloExpr expr_1 = objeps1_.getExpr();
	/////////////////////////////////////////////////////////
	cons_f1 = (expr_1 <= instance.sumpr());
	model_.add(cons_f1);

	try {
		cp_.end();
		cp_ = IloCP(model_);

		cp_.setParameter(IloCP::Workers, 1);
		cp_.setParameter(IloCP::TimeLimit, 100);
		auto start = cp_.getTime();
		cp_.solve();
		auto elapsed = cp_.getTime() - start;
		//solving_time.push_back(elapsed);
		if (cp_.getStatus() == IloAlgorithm::Optimal) {
			z2 = cp_.getObjValue();
			cout << "Optimal Solution found for costs in \t" << elapsed << " seconds" << endl;
			//this->create_schedule_from_CP(instance);
		}
		else if (cp_.getStatus() == IloAlgorithm::Feasible) {
			z2 = cp_.getObjValue();
			cout << "Feasible Solution found for costs in \t" << elapsed << " seconds" << endl;
			//this->create_schedule_from_CP(instance);
		}
		else {
			cout << "No feasible solution found for costs" << endl;
		}
	}
	catch (IloException& e) {
		cerr << "Error solving model 2: " << e.getMessage() << endl;
		e.end();
	}
	opt_pair.first = z1;
	opt_pair.second = z2;
	cout << "Makespan: " << z1 << " Costs: " << z2 << endl;
	return opt_pair;

}

void CP_Model::epsilon_original(const DRCRFFSP_Instance& instance) {

	objeps1_ = IloMinimize(env_, IloMax(makespan_));
	objeps2_ = IloMinimize(env_,IloSum(costs_));
	
	vector< pair<float, float> > opt_sol;
	pair<float, float> opt_pair;

	double epsilon = IloInfinity;
	float omega = 1;

	float z1, z2;
	//int f1, f2;

	int count = 0;

	IloExpr expr_1 = objeps1_.getExpr();
	IloExpr expr_2 = objeps2_.getExpr();

	IloRange cons_f1;
	IloRange cons_f2;

	cons_f2 = (expr_2 <= epsilon);
	model_.add(cons_f2);

	int k = 0;
	while (true) {
	//while (k <= 10){
		//Add and solve only with makespan objective
		cp_.clearStartingPoint();
		model_.add(objeps1_);
		k++;
		if (k == 3) {
			cp_.setParameter(IloCP::TimeLimit, 3600);
		}
		z1 = solve();
		create_schedule_from_CP_epsilon(instance, k,z1);
		if (cp_.getStatus() != IloAlgorithm::Feasible && cp_.getStatus() != IloAlgorithm::Optimal) {
			break;
		}
		IloSolution sol_1(env_);
		//for (int w = 0; w < w_; w++) {
			//sol_1.setValue(working_[w], cp_.getValue(working_[w]));
		//}
		
		/*for (int m = 0; m < m_; m++) {
			for (int mode = 0; mode < machines_[m].getSize(); mode++) {
				if (cp_.isPresent(machines_[m][mode]) == true) {
					sol_1.setPresent(machines_[m][mode]);
					sol_1.setStart(machines_[m][mode], cp_.getStart(machines_[m][mode]));
				}
			}
		}*/
		for (int w = 0; w < w_; w++) {
			for (int mode = 0; mode < workers_[w].getSize(); mode++) {
				if (cp_.isPresent(workers_[w][mode]) == true) {
					sol_1.setPresent(workers_[w][mode]);
					sol_1.setStart(workers_[w][mode], cp_.getStart(workers_[w][mode]));
				}
			}
		}

		cp_.setStartingPoint(sol_1);
		//changing the Objecitve from makespan to costs 
		model_.remove(objeps1_);
		model_.add(objeps2_);

		//bounding the makespan
		IloRange cons_f1 = (expr_1 <= z1);
		model_.add(cons_f1);

		// Solving only with the cost objective
		z2 = solve();
		k++;
		create_schedule_from_CP_epsilon(instance, k,z1);
		if (cp_.getStatus() != IloAlgorithm::Feasible && cp_.getStatus() != IloAlgorithm::Optimal) {
			break;
		}

		/*IloSolution sol_2(env_);
		for (int w = 0; w < w_; w++) {
			sol_2.setValue(working_[w], cp_.getValue(working_[w]));
		}
		
		for (int m = 0; m < m_; m++) {
			for (int mode = 0; mode < machines_[m].getSize(); mode++) {
				if (cp_.isPresent(machines_[m][mode]) == true) {
					sol_2.setPresent(machines_[m][mode]);
					sol_2.setStart(machines_[m][mode], cp_.getStart(machines_[m][mode]));
				}
			}
		}
		for (int w = 0; w < w_; w++) {
			for (int mode = 0; mode < workers_[w].getSize(); mode++) {
				if (cp_.isPresent(workers_[w][mode]) == true) {
					sol_2.setPresent(workers_[w][mode]);
					sol_2.setStart(workers_[w][mode], cp_.getStart(workers_[w][mode]));
				}
			}
		}

		cp_.setStartingPoint(sol_2);*/
		//collect the objective values for makespan and costs
		opt_pair.first = z1;
		opt_pair.second = z2;

		opt_sol.push_back(opt_pair);

		//remove bound on makespan objective
		model_.remove(cons_f1);

		//set epsilon to the objective value - 1 (trying to find better costs, while sacrificing makespan)
		//epsilon = z1 + omega;
		epsilon = z2 - omega;
		cout << epsilon << endl;

		//Removing the Objective for costs
		model_.remove(objeps2_);

		//update upper bound constraint on f2
		cons_f2.setUB(epsilon);//
		//cons_f2 = (expr_1 >= epsilon);
		//cons_f2.setLB(epsilon);
		//model_.add(cons_f2);

	}

	// print the pareto frontier
	cout << "f1: cmax" << "\tf2: cost" << endl;
	for (int i = 0; i < opt_sol.size(); ++i)
	{
		cout << opt_sol[i].first << "\t\t" << opt_sol[i].second << endl;
	}

}

void CP_Model::epsilon_original_otherway(const DRCRFFSP_Instance& instance) {
	//Solve first costs, but with warm start for makespan, then for makespan with bound on costs, then bound makespan - 1 (warm-start again) => then costs again
	//Because before solved for static lex makespan costs, we can use this solution as a warm start
	int makespan_min;
	int costs_max;
	objeps1_ = IloMinimize(env_, IloMax(makespan_));
	objeps2_ = IloMinimize(env_, IloSum(costs_));

	vector< pair<int, int> > opt_sol;
	pair<int, int> opt_pair;
	int epsilon = instance.sumpr();
	int omega = 1;

	int z1, z2;
	//int f1, f2;

	IloExpr expr_1 = objeps1_.getExpr();
	IloExpr expr_2 = objeps2_.getExpr();
	IloRange cons_f1;
	IloRange cons_f2;

	cp_.setOut(env_.getNullStream());
	int k = 100000000;
	model_.add(objeps1_);
	//minimal makespan
	makespan_min = solve();
	model_.remove(objeps1_);
	model_.add(objeps2_);
	//bounding the makespan
	cons_f1 = (expr_1 <= makespan_min);
	model_.add(cons_f1);
	// First solution for costs (maximal)
	costs_max = solve();
	create_schedule_from_CP_epsilon(instance, k,makespan_min);
	cout << "Last Solution: " << makespan_min << " " << costs_max << endl;
	model_.remove(cons_f1);
	model_.remove(objeps2_);
	IloSolution sol_1(env_);
	for (int w = 0; w < w_; w++) {
		sol_1.setValue(working_[w], cp_.getValue(working_[w]));
	}
	for (int o = 0; o < o_; o++) {
		sol_1.setStart(operations_[o], cp_.getStart(operations_[o]));
	}

	for (int w = 0; w < w_; w++) {
		for (int mode = 0; mode < workers_[w].getSize(); mode++) {
			if (cp_.isPresent(workers_[w][mode]) == true) {
				sol_1.setPresent(workers_[w][mode]);
			}
		}
	}
	
    //"Static lex" for costs
	//////////////////////////////////////
	k = 0;
	cons_f1 = (expr_1 <= epsilon);
	//warm start for costs
	cp_.setStartingPoint(sol_1);
	model_.add(objeps2_);
	model_.add(cons_f1);
	//minimal costs
	//cp_.setParameter(IloCP::TimeLimit, 100);
	z2 = solve();
	model_.remove(objeps2_);
	model_.add(objeps1_);
	//bounding the costs
	cons_f2 = (expr_2 <= z2);
	model_.add(cons_f2);
	// First solution for makespan (maximal)
	z1 = solve();
	create_schedule_from_CP_epsilon(instance, k, z1);
	opt_pair.first = z1;
	opt_pair.second = z2;
	opt_sol.push_back(opt_pair);
	cout << "Start Solution: " << z1 << " " << z2 << endl;
	model_.remove(cons_f2);
	////////////////////////////////////////////////
	k++;
	model_.remove(objeps1_);
	model_.add(objeps2_);
	epsilon = z1 - omega;
	//Bound on makespan
	cons_f1.setUB(epsilon);
	cout << z1 - makespan_min << " possible solutions" << endl;
	while (epsilon > makespan_min) {
		cp_.setParameter(IloCP::TimeLimit, 3600);
		
		//next solution for costs
		z2 = solve();
	
		if (cp_.getStatus() != IloAlgorithm::Feasible && cp_.getStatus() != IloAlgorithm::Optimal) {
			break;
		}

		IloSolution sol_2(env_);
		for (int w = 0; w < w_; w++) {
			sol_2.setValue(working_[w], cp_.getValue(working_[w]));
		}
		IloInt makespan = 0;
		for (int o = 0; o < o_; o++) {
			sol_2.setStart(operations_[o], cp_.getStart(operations_[o]));
			IloInt end = cp_.getEnd(operations_[o]);
			makespan = IloMax(makespan, end);
		}
		z1 = (int)makespan;

		for (int w = 0; w < w_; w++) {
			for (int mode = 0; mode < workers_[w].getSize(); mode++) {
				if (cp_.isPresent(workers_[w][mode]) == true) {
					sol_2.setPresent(workers_[w][mode]);
				}
			}
		}
		create_schedule_from_CP_epsilon(instance, k,makespan);
		k++;
		//Warm start for next solution (infeasible)
		cp_.clearStartingPoint();
		cp_.setStartingPoint(sol_2);

		//collect the objective values for makespan and costs
		opt_pair.first = z1;
		opt_pair.second = z2;
		cout << "Current Solution: " << z1 << " " << z2 << endl;
		opt_sol.push_back(opt_pair);

		//set epsilon to the objective value of makespan - 1 (trying to find better makespan, while sacrificing costs)
		epsilon = z1 - omega;

		//update upper bound constraint on f1
		cons_f1.setUB(epsilon);//
	}

	opt_pair.first = makespan_min;
	opt_pair.second = costs_max;
	opt_sol.push_back(opt_pair);


	// print the pareto frontier
	cout << "f1: makespan" << "\tf2: costs" << endl;
	for (int i = 0; i < opt_sol.size(); ++i)
	{
		cout << opt_sol[i].first << "\t\t" << opt_sol[i].second << endl;
	}

}


void CP_Model::epsilon_fake(const DRCRFFSP_Instance& instance) {

	objeps1_ = IloMinimize(env_, IloMax(makespan_));
	objeps2_ = IloMinimize(env_, IloSum(costs_));

	vector< pair<float, float> > opt_sol;
	pair<float, float> opt_pair;

	double epsilon = IloInfinity;
	float omega = 1;// 0.10;

	float z1, z2;
	//int f1, f2;

	int count = 0;

	IloExpr expr_1 = objeps1_.getExpr();
	IloExpr expr_2 = objeps2_.getExpr();

	IloRange cons_f1;
	IloRange cons_f2;

	//cons_f2 = (expr_2 <= epsilon);
	//model_.add(cons_f2);

	int k = 0;
	//while (true) {
	while (k <= 10) {
		//Add and solve only with makespan objective
		model_.add(objeps1_);
		z1 = solve();

		if (cp_.getStatus() != IloAlgorithm::Feasible && cp_.getStatus() != IloAlgorithm::Optimal) {
			break;
		}
		IloSolution sol_1(env_);
		//for (int w = 0; w < w_; w++) {
			//sol_1.setValue(working_[w], cp_.getValue(working_[w]));
		//}

		/*for (int m = 0; m < m_; m++) {
			for (int mode = 0; mode < machines_[m].getSize(); mode++) {
				if (cp_.isPresent(machines_[m][mode]) == true) {
					sol_1.setPresent(machines_[m][mode]);
					sol_1.setStart(machines_[m][mode], cp_.getStart(machines_[m][mode]));
				}
			}
		}*/
		for (int w = 0; w < w_; w++) {
			for (int mode = 0; mode < workers_[w].getSize(); mode++) {
				if (cp_.isPresent(workers_[w][mode]) == true) {
					sol_1.setPresent(workers_[w][mode]);
					sol_1.setStart(workers_[w][mode], cp_.getStart(workers_[w][mode]));
				}
			}
		}

		cp_.setStartingPoint(sol_1);
		//changing the Objecitve from makespan to costs 
		model_.remove(objeps1_);
		model_.add(objeps2_);

		//bounding the makespan
		IloRange cons_f1 = (expr_1 <= z1);
		model_.add(cons_f1);

		// Solving only with the cost objective
		z2 = solve();
		
		if (cp_.getStatus() != IloAlgorithm::Feasible && cp_.getStatus() != IloAlgorithm::Optimal) {
			break;
		}
		IloSolution sol_2(env_);
		//for (int w = 0; w < w_; w++) {
			//sol_2.setValue(working_[w], cp_.getValue(working_[w]));
		//}

		/*for (int m = 0; m < m_; m++) {
			for (int mode = 0; mode < machines_[m].getSize(); mode++) {
				if (cp_.isPresent(machines_[m][mode]) == true) {
					sol_2.setPresent(machines_[m][mode]);
					sol_2.setStart(machines_[m][mode], cp_.getStart(machines_[m][mode]));
				}
			}
		}*/
		for (int w = 0; w < w_; w++) {
			for (int mode = 0; mode < workers_[w].getSize(); mode++) {
				if (cp_.isPresent(workers_[w][mode]) == true) {
					sol_2.setPresent(workers_[w][mode]);
					sol_2.setStart(workers_[w][mode], cp_.getStart(workers_[w][mode]));
				}
			}
		}

		cp_.setStartingPoint(sol_2);
		//collect the objective values for makespan and costs
		opt_pair.first = z1;
		opt_pair.second = z2;

		opt_sol.push_back(opt_pair);

		//remove bound on makespan objective
		model_.remove(cons_f1);

		//set epsilon to the objective value +1  (trying to find better costs, while sacrificing makespan)
		epsilon = z1 + omega;
		//epsilon = z2 - omega;

		//Removing the Objective for costs
		model_.remove(objeps2_);

		//update upper bound constraint on f2
		//cons_f2.setUB(epsilon);//
		cons_f2 = (expr_1 >= epsilon);
		cons_f2.setLB(epsilon);
		model_.add(cons_f2);
		k++;
	}

	// print the pareto frontier
	cout << "f1: cmax" << "\tf2: cost" << endl;
	for (int i = 0; i < opt_sol.size(); ++i)
	{
		cout << opt_sol[i].first << "\t\t" << opt_sol[i].second << endl;
	}

}

void CP_Model::create_schedule_from_CP_epsilon(const DRCRFFSP_Instance& instance,int k,int cmax) {

	// function that creates a schedule from a solution generated by the CP
	float opt = -1;
	int sum = 0;

	try {
		if (cp_.getStatus() == IloAlgorithm::Feasible) {
			opt = cp_.getObjValue();

			cout << "Creating schedule with feasible solution with makespan: " << cmax << endl;
		}
		else if (cp_.getStatus() == IloAlgorithm::Optimal) {
			opt = cp_.getObjValue();
			cout << "Creating schedule with optimal solution with makespan: " << cmax << endl;
		}
		else {
			cout << "No feasible solution found" << endl;
		}
		//string filename = "schedule.txt";
		string filename = instance.filename().erase(0, 24);
		ofstream myfile("Schedules/Solution_" + to_string(k) + "_" + filename);
		myfile << "Jobs, Stages, Machines, Workers, Reentrance, Cmax\n";
		myfile << instance.n() << " " << instance.s() << " " << instance.m() << " " << instance.w() << " " << instance.r() << " " << cmax << "\n";
		for (int w = 0; w < instance.w(); w++) {
			if (w != 0) {
				myfile << " ";
			}
			myfile << (instance.workers_begin() + w)->costs();
		}
		myfile << "\n";
		myfile << "Operation on machine with worker starting at ending at\n";
		int operation_count = 1;
		for (int n = 0; n < n_; n++) {
			int number_operations_of_job = (instance.jobs_begin() + n)->number_operations();
			for (int o = 0; o < number_operations_of_job; o++) {
				int stageId = ((instance.jobs_begin() + n)->operations_begin() + o)->stageId();
				int number_workers = (instance.stage_begin() + stageId - 1)->number_workers();
				for (int w = 0; w < number_workers; w++) {
					int workerId = *((instance.stage_begin() + stageId - 1)->worker_begin() + w);
					for (int mode = 0; mode < workers_[workerId - 1].getSize(); mode++) {
						if (cp_.isPresent(workers_[workerId - 1][mode]) == true) {
							string name = workers_[workerId - 1][mode].getName();
							string inter = to_string(operation_count) + "_" + to_string(workerId);
							if (name == inter) {
								//cout << name << endl;
								myfile << operation_count << " " << stageId << " " << workerId << " " << cp_.getStart(workers_[workerId - 1][mode]) << " " <<
									cp_.getEnd(workers_[workerId - 1][mode]) << "\n";
								break;
							}
						}
					}

				}
				operation_count += 1;

			}
		}
		myfile.close();
	}


	catch (IloException& e) {
		cerr << "Error solving model: " << e.getMessage() << endl;
		e.end();
	}

}

void CP_Model::create_schedule_from_CP(const DRCRFFSP_Instance& instance) {
	// Function that creates a schedule from a solution generated by the CP
	float opt = -1;
	int sum = 0;
	try {
		if (cp_.getStatus() == IloAlgorithm::Feasible) {
			opt = cp_.getObjValue();

			cout << "Feasible Solution with objective: " << opt << endl;
		}
		else if (cp_.getStatus() == IloAlgorithm::Optimal) {
			opt = cp_.getObjValue();
			cout << "Optimal Solution with objective: " << opt << endl;
		}
		else {
			cout << "No feasible solution found" << endl;
		}
		//string filename = " Solution_schedule_costs_less_workers_3.txt";
		string filename = instance.filename().erase(0, 25);
		ofstream myfile("Schedules/Solution_" + filename);
		myfile << "Jobs, Stages, Machines, Workers, Reentrance, Cmax\n";
		myfile << instance.n() << " " << instance.s() << " " << instance.m() << " " << instance.w() << " " << instance.r() << " " << opt << "\n";
		for (int w = 0; w < instance.w(); w++) {
			if (w != 0) {
				myfile << " ";
			}
			myfile << (instance.workers_begin() + w)->costs();
		}
		myfile << "\n";
		myfile << "Operation on machine with worker starting at ending at\n";
		int operation_count = 0;
		for (int n = 0; n < n_; n++) {
			int number_operations_of_job = (instance.jobs_begin() + n)->number_operations();
			for (int o = 0; o < number_operations_of_job; o++) {
				int stageId = ((instance.jobs_begin() + n)->operations_begin() + o)->stageId();
				int number_workers = (instance.stage_begin() + stageId - 1)->number_workers();
				for (int w = 0; w < number_workers; w++) {
					int workerId = *((instance.stage_begin() + stageId - 1)->worker_begin() + w);
					for (int mode = 0; mode < workers_[workerId - 1].getSize(); mode++) {
						if (cp_.isPresent(workers_[workerId - 1][mode]) == true) {
							string name = workers_[workerId - 1][mode].getName();
							string inter = to_string(operation_count) + "_" + to_string(workerId);
							if (name == inter) {
								//cout << name << endl;
								myfile << operation_count << " " << stageId << " " << workerId << " " << cp_.getStart(workers_[workerId - 1][mode]) << " " <<
									cp_.getEnd(workers_[workerId - 1][mode]) << "\n";
								break;
							}
						}
					}

				}
				operation_count += 1;

			}
		}
		myfile.close();
	}


	catch (IloException& e) {
		cerr << "Error solving model: " << e.getMessage() << endl;
		e.end();
	}

}

void CP_Model::create_schedule_from_CP_bi_obj(const DRCRFFSP_Instance& instance) {
	// function that creates a schedule from a solution generated by the CP
	//IloNumExprArray objArray(env_);
	//obj1_ = IloMax(makespan_);
	//obj2_ = IloSum(costs_);
	//objArray.add(obj1_);
	//objArray.add(obj2_);

	//IloMultiCriterionExpr biExpr = IloStaticLex(env_, objArray);
	//static_lex_ = IloMinimize(env_, biExpr);
	//model_.add(static_lex_);
	float opt_1 = -1;
	float opt_2 = -1;
	int sum = 0;
	try {
		//cp_.solve();
		if (cp_.getStatus() == IloAlgorithm::Feasible) {
			opt_1 = cp_.getValue(obj1_);
			opt_2 = cp_.getValue(obj2_);

			cout << "Feasible Solution with makesspan: " << opt_1 << " and costs: " << opt_2 << endl;
		}
		else if (cp_.getStatus() == IloAlgorithm::Optimal) {
			opt_1 = cp_.getValue(obj1_);
			opt_2 = cp_.getValue(obj2_);
			cout << "Optimal Solution with makesspan: " << opt_1 << " and costs: " << opt_2 << endl;
		}
		else {
			cout << "No feasible solution found" << endl;
		}
		//string filename = " Solution_schedule_bi_objective.txt";
		string filename = instance.filename().erase(0, 28);
		ofstream myfile("Schedules/Small/Solution_lex_" + filename);
		myfile << "Jobs, Stages, Machines, Workers, Reentrance, Cmax\n";
		myfile << instance.n() << " " << instance.s() << " " << instance.m() << " " << instance.w() << " " << instance.r() << " " << opt_1 << " " << opt_2 << "\n";
		for (int w = 0; w < instance.w(); w++) {
			if (w != 0) {
				myfile << " ";
			}
			myfile << (instance.workers_begin() + w)->costs();
		}
		myfile << "\n";
		myfile << "Operation on machine with worker starting at ending at\n";
		int operation_count = 1;
		for (int n = 0; n < n_; n++) {
			int number_operations_of_job = (instance.jobs_begin() + n)->number_operations();
			for (int o = 0; o < number_operations_of_job; o++) {
				int stageId = ((instance.jobs_begin() + n)->operations_begin() + o)->stageId();
				int number_machines = (instance.stage_begin() + stageId - 1)->number_machines();
				int number_workers = (instance.stage_begin() + stageId - 1)->number_workers();
				for (int m = 0; m < number_machines; m++) {
					for (int w = 0; w < number_workers; w++) {
						int machineId = ((instance.stage_begin() + stageId - 1)->machine_begin() + m)->mId();
						int workerId = *((instance.stage_begin() + stageId - 1)->worker_begin() + w);
						for (int mode = 0; mode < machines_[machineId - 1].getSize(); mode++) {
							if (cp_.isPresent(machines_[machineId - 1][mode]) == true) {
								string name = machines_[machineId - 1][mode].getName();
								string inter = to_string(operation_count) + to_string(machineId) + to_string(workerId);
								if (name == inter) {
									myfile << operation_count << " " << machineId << " " << workerId << " " << cp_.getStart(machines_[machineId - 1][mode]) << " " <<
										cp_.getEnd(machines_[machineId - 1][mode]) << "\n";
									break;
								}
							}
						}
					}

				}
				operation_count += 1;
			}
		}
		myfile.close();
	}


	catch (IloException& e) {
		cerr << "Error solving model: " << e.getMessage() << endl;
		e.end();
	}	
	/*int sum_hours = 0;
	for (int w = 0; w < w_; w++) {
		vector<float> worker_begin;
		vector<float> worker_end;
		for (int mode = 0; mode < workers_[w].getSize(); mode++) {
			if (cp_.isPresent(workers_[w][mode]) == true) {
				//cout << cp_.getStart(workers_[w][mode]) << endl;
				//cout << cp_.getEnd(workers_[w][mode]) << endl;
				worker_begin.push_back(cp_.getStart(workers_[w][mode]));
				worker_end.push_back(cp_.getEnd(workers_[w][mode]));
			}
		}
		cout << "Max end: " << *max_element(worker_end.begin(), worker_end.end()) << endl;
		cout << "Min start: " << *min_element(worker_begin.begin(), worker_begin.end()) << endl;
		sum_hours += *max_element(worker_end.begin(), worker_end.end()) - *min_element(worker_begin.begin(), worker_begin.end());
		
			//cout << *max_element(worker_end.begin(),worker_end.end()) - *min_element(worker_begin.begin(),worker_begin.end()) << endl;
	}
	cout << sum_hours << endl;*/

}



void CP_Model::set_time_limit(double seconds)
{
    if (seconds <= 0.0) {
        seconds = 0.01;
    }
    cp_.setParameter(IloCP::TimeLimit, seconds);
}
