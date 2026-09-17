// DRCRFFSP.cpp :
//

#include <iostream>
#include <vector>
#include <cstdlib>
#include <vector>
//#include <crtdbg.h>
#include <chrono>
#include "DRCRFFSP_Instance.h"
#include "CP_Model.h"
#include "Masterproblem.h"
#include "Decomposition.h"
#include "ExperimentRunner.h"


using namespace std;


int main(int argc, char** argv)
{
    // Computational-experiment CLI (see experiments/README.md): any
    // invocation using --flag style arguments (e.g. --instance <file>
    // --method cp_wws --time-limit 3600) is routed to run_experiment_cli()
    // instead of the legacy positional CLI below, which is left completely
    // unchanged for existing manual testing.
    if (argc > 1 && string(argv[1]).rfind("--", 0) == 0) {
        return run_experiment_cli(argc, argv);
    }

    // input file: default or first command line argument
    string input_file = "C:/Users/Mlekusch/source/repos/Cost_decomposition/Instances/Toy/Instance_Toy_3.txt";
    //10_5_3_3 10_10_2_2
    if (argc > 1) {
        input_file = argv[1];
    }

    // Second command-line argument selects which method(s) to run, so
    // each can be time-budgeted independently (e.g. via an external
    // `timeout` wrapper) for a fair comparison instead of always running
    // all three back-to-back in one process:
    //     decomposition -> only Decomposition::run() (unconstrained WWS)
    //     epsilon       -> only Decomposition::run_for_epsilon(epsilon)
    //     obj2          -> only the integrated baseline's solve_obj(I, 2)
    //     lex           -> only the integrated baseline's solve_static_lex()
    //     makespan      -> only the integrated baseline's solve_obj(I, 1)
    //                      (pure Cmax minimization, ignores WWS entirely --
    //                      used to establish Cmax_min, the epsilon range's
    //                      lower extreme)
    //     all (default) -> decomposition + obj2 + lex, sequentially
    string mode = "all";
    if (argc > 2) {
        mode = argv[2];
    }

    // Optional 3rd/4th args, meaning depends on mode:
    //   decomposition: best_effort_time_limit, seed_ub_time_limit
    //   epsilon:       epsilon (REQUIRED), best_effort_time_limit
    // seed_ub defaults to 0 (disabled) -- see Decomposition::run()'s doc
    // comment for why an unconditional seed is unsafe as a global default.
    double best_effort_time_limit = 10.0;
    double seed_ub_time_limit = 0.0;
    double epsilon_value = -1.0;

    if (mode == "epsilon") {

        if (argc > 3) {
            epsilon_value = atof(argv[3]);
        }
        if (argc > 4) {
            best_effort_time_limit = atof(argv[4]);
        }
    }
    else {

        if (argc > 3) {
            best_effort_time_limit = atof(argv[3]);
        }
        if (argc > 4) {
            seed_ub_time_limit = atof(argv[4]);
        }
    }

    // Creating the Instance from the Input File
    DRCRFFSP_Instance I(input_file);

    // Wall-clock cap applied to EACH approach (decomposition, obj2, lex)
    // independently, so a hard instance cannot let one approach consume the
    // whole job's time budget at the expense of the others.
    const double kApproachTimeLimitSeconds = 3600.0;

    if (mode == "decomposition" || mode == "all") {

        MASTER_Model MM(I);
        auto start = chrono::steady_clock::now();

        // Runs the full multi-level decomposition (master enumeration,
        // zero-idle checks, persistent no-good-cut pool, candidate LB
        // certification, exact-match checks, best-effort UB improvement)
        // to completion -- see Decomposition.h for the algorithm.
        Decomposition decomposition(I, MM);
        DecompositionResult result =
            decomposition.run(best_effort_time_limit, seed_ub_time_limit,
                               kApproachTimeLimitSeconds);

        auto finish = std::chrono::steady_clock::now();
        chrono::duration<double> elapsed = finish - start;
        cout << "\nElapsed (decomposition): " << elapsed.count() << " s" << endl;

        if (result.optimality_proven) {

            cout << "Decomposition result: WWS* = " << result.global_ub << endl;
        }
        else if (result.unresolved) {

            cout << "Decomposition result: UNRESOLVED. LB = " << result.global_lb;
            if (result.has_incumbent) {
                cout << ", UB = " << result.global_ub;
            }
            cout << endl;
        }
        else {

            cout << "Decomposition result: LB = " << result.global_lb;
            if (result.has_incumbent) {
                cout << ", UB = " << result.global_ub;
            }
            else {
                cout << ", UB = none";
            }
            cout << " (gap remains)" << endl;
        }

        if (result.has_incumbent && result.achieved_makespan >= 0.0) {
            cout << "Achieved Cmax (of incumbent/optimal WWS schedule): "
                 << result.achieved_makespan << endl;
        }
    }

    if (mode == "makespan") {

        cout << "\n==============================" << endl;
        cout << "BASELINE: solve_obj(I, 1)  [pure Cmax minimization]" << endl;
        cout << "==============================" << endl;

        auto start_ms = chrono::steady_clock::now();
        CP_Model baseline_ms(I);
        float cmax_min = baseline_ms.solve_obj(I, 1);
        auto finish_ms = chrono::steady_clock::now();
        chrono::duration<double> elapsed_ms = finish_ms - start_ms;

        cout << "solve_obj(I, 1) result: Cmax_min = " << cmax_min
             << ", elapsed = " << elapsed_ms.count() << " s" << endl;
    }

    if (mode == "epsilon") {

        if (epsilon_value <= 0) {

            cerr << "epsilon mode requires a positive epsilon value as "
                    "the 3rd argument." << endl;
            return 1;
        }

        MASTER_Model MM(I);   // constructed unconstrained; run_for_epsilon()
                               // builds its OWN fresh epsilon-specific
                               // master internally and does not use this
                               // one -- kept only because Decomposition's
                               // constructor requires a MASTER_Model& to
                               // bind to (see Decomposition.h).
        Decomposition decomposition(I, MM);

        EpsilonResult result =
            decomposition.run_for_epsilon(epsilon_value, best_effort_time_limit);

        cout << "\nDecomposition result for epsilon=" << epsilon_value
             << ": ";
        if (result.optimality_proven) {
            cout << "WWS*(epsilon) = " << result.wws_ub;
        }
        else {
            cout << "LB = " << result.wws_lb;
            if (result.has_incumbent) {
                cout << ", UB = " << result.wws_ub;
            }
            cout << (result.unresolved ? " (UNRESOLVED)" : " (gap remains)");
        }
        cout << endl;
    }

    // ------------------------------------------------------------
    // Baseline comparison: solve the SAME instance directly with the
    // integrated (non-decomposed) CP model, using its existing
    // solve_obj(I, 2) (minimize WWS directly) and solve_static_lex()
    // (lexicographic makespan-then-WWS) methods. Each gets its own fresh
    // CP_Model instance, since both methods permanently add an objective
    // to whatever object they are called on. CP_Model's integrated
    // baseline constructor sets a 300-second IloCP::TimeLimit by default;
    // both calls below raise it to kApproachTimeLimitSeconds explicitly, so
    // a result may only be feasible (not proven optimal) if that is not
    // enough for this instance.
    // ------------------------------------------------------------

    if (mode == "obj2" || mode == "all") {

        cout << "\n==============================" << endl;
        cout << "BASELINE: solve_obj(I, 2)" << endl;
        cout << "==============================" << endl;

        auto start_obj2 = chrono::steady_clock::now();
        CP_Model baseline_obj2(I);
        baseline_obj2.set_time_limit(kApproachTimeLimitSeconds);
        float wws_obj2 = baseline_obj2.solve_obj(I, 2);
        auto finish_obj2 = chrono::steady_clock::now();
        chrono::duration<double> elapsed_obj2 = finish_obj2 - start_obj2;

        cout << "solve_obj(I, 2) result: WWS = " << wws_obj2
             << ", elapsed = " << elapsed_obj2.count() << " s" << endl;
    }

    if (mode == "lex" || mode == "all") {

        cout << "\n==============================" << endl;
        cout << "BASELINE: solve_static_lex()" << endl;
        cout << "==============================" << endl;

        auto start_lex = chrono::steady_clock::now();
        CP_Model baseline_lex(I);
        auto lex_result = baseline_lex.solve_static_lex(kApproachTimeLimitSeconds);
        auto finish_lex = chrono::steady_clock::now();
        chrono::duration<double> elapsed_lex = finish_lex - start_lex;

        cout << "solve_static_lex() result: makespan = " << get<0>(lex_result)
             << ", WWS = " << get<1>(lex_result)
             << ", elapsed = " << elapsed_lex.count() << " s" << endl;
    }
}