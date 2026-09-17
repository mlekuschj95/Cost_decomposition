// windows.h must be included FIRST, before any header that does
// `using namespace std;` at global scope (CP_Model.h does) -- otherwise
// its `byte` typedef collides with C++17's std::byte once that using-
// directive is in effect for the rest of the translation unit.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "ExperimentRunner.h"
#include "DRCRFFSP_Instance.h"
#include "CP_Model.h"
#include "Masterproblem.h"
#include "Decomposition.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <map>

using namespace std;

namespace {

// ------------------------------------------------------------------
// Small CSV row builder: an ordered list of (column, value) pairs, all
// columns from the ONE superset schema (section 30 of the experiment
// spec) so every method's row can later be concatenated/merged directly.
// Absent/not-applicable values are written as an empty field (NA), never
// invented -- see section 6/19 of the spec.
// ------------------------------------------------------------------
using Row = vector<pair<string, string>>;

void add_str(Row& row, const string& name, const string& value) {
    row.push_back({name, value});
}

void add_na(Row& row, const string& name) {
    row.push_back({name, ""});
}

void add_double(Row& row, const string& name, double value) {
    ostringstream ss;
    ss.setf(std::ios::fixed);
    ss << setprecision(6) << value;
    row.push_back({name, ss.str()});
}

string csv_escape(const string& field) {
    // Always quote: harmless for plain numbers/identifiers, and safe for
    // instance paths / error messages that might contain a comma.
    string out = "\"";
    for (char c : field) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += "\"";
    return out;
}

void write_csv_row(const Row& row, const string& path) {
    ofstream out(path);
    if (!out) {
        cerr << "ERROR: could not open output file for writing: " << path << endl;
        return;
    }
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) out << ",";
        out << csv_escape(row[i].first);
    }
    out << "\n";
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) out << ",";
        out << csv_escape(row[i].second);
    }
    out << "\n";
}

// ------------------------------------------------------------------
// Small path / string helpers (no external dependency; instance paths on
// the cluster are plain POSIX paths, but this also tolerates a Windows
// path for local smoke-testing).
// ------------------------------------------------------------------

string basename_no_ext(const string& path) {
    size_t slash = path.find_last_of("/\\");
    string base = (slash == string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != string::npos) base = base.substr(0, dot);
    return base;
}

// The parent directory name of the instance file -- e.g.
// ".../Instances/Tiny/Instance_5_2_2_1.txt" -> "Tiny". This is how the
// TINY/SMALL instance sets are distinguished in this repository (see
// Instances/Tiny/, Instances/Small/); no separate instance-list file
// exists in the project, so size_class is derived directly from the
// on-disk directory structure, per section 2 of the experiment spec.
string size_class_from_path(const string& path) {
    string trimmed = path;
    while (!trimmed.empty() && (trimmed.back() == '/' || trimmed.back() == '\\')) {
        trimmed.pop_back();
    }
    size_t last_slash = trimmed.find_last_of("/\\");
    if (last_slash == string::npos) return "";
    string parent = trimmed.substr(0, last_slash);
    size_t prev_slash = parent.find_last_of("/\\");
    string dir_name = (prev_slash == string::npos) ? parent : parent.substr(prev_slash + 1);
    return dir_name;
}

string sanitize_for_filename(const string& s) {
    string out;
    out.reserve(s.size());
    for (char c : s) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            out += c;
        } else {
            out += '_';
        }
    }
    return out;
}

string to_upper(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return toupper(c); });
    return s;
}

// Formats epsilon for a run_id: integers print without a trailing ".0",
// non-integers keep one decimal place, so e.g. 120 -> "120", 12.5 -> "12_5".
string epsilon_tag(double epsilon) {
    ostringstream ss;
    if (std::abs(epsilon - std::round(epsilon)) < 1e-9) {
        ss << static_cast<long long>(std::round(epsilon));
    } else {
        ss << fixed << setprecision(1) << epsilon;
    }
    string s = ss.str();
    replace(s.begin(), s.end(), '.', '_');
    return s;
}

string get_hostname() {
    char buf[256] = {0};
#ifdef _WIN32
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) return string(buf);
#else
    if (gethostname(buf, sizeof(buf)) == 0) return string(buf);
#endif
    return "";
}

string get_timestamp_utc() {
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &t);
#else
    gmtime_r(&t, &utc);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return string(buf);
}

string status_string(FeasibilityStatus s) {
    switch (s) {
        case FeasibilityStatus::FEASIBLE: return "FEASIBLE";
        case FeasibilityStatus::INFEASIBLE: return "INFEASIBLE";
        default: return "UNKNOWN";
    }
}

string status_string(IloAlgorithm::Status s) {
    switch (s) {
        case IloAlgorithm::Optimal: return "OPTIMAL";
        case IloAlgorithm::Feasible: return "FEASIBLE_TIME_LIMIT";
        case IloAlgorithm::Infeasible: return "INFEASIBLE";
        default: return "UNKNOWN";
    }
}

double gap_percent(double value, double bound) {
    if (bound == 0.0) return 0.0;
    return (value - bound) / bound * 100.0;
}

// ------------------------------------------------------------------
// Parsed CLI options.
// ------------------------------------------------------------------
struct Options {
    string instance_path;
    string method;
    bool has_epsilon = false;
    double epsilon = -1.0;
    double time_limit = 3600.0;
    double best_effort_time_limit = -1.0;   // resolved below if not given
    double seed_ub_time_limit = 0.0;
    string output_path;
    string run_id_override;
};

bool parse_args(int argc, char** argv, Options& opt, string& error) {
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        auto next_value = [&](const char* flag_name) -> string {
            if (i + 1 >= argc) {
                error = string("missing value for ") + flag_name;
                return string();
            }
            return string(argv[++i]);
        };

        if (arg == "--instance") { opt.instance_path = next_value("--instance"); }
        else if (arg == "--method") { opt.method = next_value("--method"); }
        else if (arg == "--epsilon") {
            opt.epsilon = atof(next_value("--epsilon").c_str());
            opt.has_epsilon = true;
        }
        else if (arg == "--time-limit") { opt.time_limit = atof(next_value("--time-limit").c_str()); }
        else if (arg == "--best-effort-time-limit") { opt.best_effort_time_limit = atof(next_value("--best-effort-time-limit").c_str()); }
        else if (arg == "--seed-ub-time-limit") { opt.seed_ub_time_limit = atof(next_value("--seed-ub-time-limit").c_str()); }
        else if (arg == "--output") { opt.output_path = next_value("--output"); }
        else if (arg == "--run-id") { opt.run_id_override = next_value("--run-id"); }
        else {
            error = "unrecognized argument: " + arg;
            return false;
        }
        if (!error.empty()) return false;
    }

    if (opt.instance_path.empty()) { error = "--instance is required"; return false; }
    if (opt.method.empty()) { error = "--method is required"; return false; }

    static const vector<string> valid_methods = {
        "cp_wws", "cp_cmax", "cp_lex", "lb_ap", "decomp_wws",
        "cp_wws_eps", "lb_ap_eps", "decomp_eps"
    };
    if (find(valid_methods.begin(), valid_methods.end(), opt.method) == valid_methods.end()) {
        error = "unknown --method: " + opt.method;
        return false;
    }

    bool needs_epsilon = (opt.method == "cp_wws_eps" || opt.method == "lb_ap_eps" || opt.method == "decomp_eps");
    if (needs_epsilon && (!opt.has_epsilon || opt.epsilon <= 0)) {
        error = "--method " + opt.method + " requires a positive --epsilon";
        return false;
    }

    if (opt.best_effort_time_limit < 0) {
        // Default: a short per-assignment cap, never larger than the
        // overall budget (matches the existing Decomposition::run()
        // default of 10.0 for the legacy CLI).
        opt.best_effort_time_limit = std::min(10.0, opt.time_limit);
    }

    return true;
}

// Threads/seed are FIXED by this codebase, not user-configurable through
// this CLI (see MASTER_Model's constructor and CP_Model's integrated
// baseline constructor): CP Optimizer Workers=1 and CPLEX Threads=1/
// RandomSeed=1, for reproducibility and to avoid oversubscribing a
// single-CPU cluster allocation (sections 5/26/29 of the experiment spec).
// Recorded here as constants rather than exposed as a CLI knob that would
// not actually change solver behavior.
const int kThreads = 1;

}  // namespace

int run_experiment_cli(int argc, char** argv) {

    Options opt;
    string parse_error;
    if (!parse_args(argc, argv, opt, parse_error)) {
        cerr << "Usage: drcrffsp --instance <file> --method <method> "
                "[--epsilon <value>] [--time-limit <seconds>] "
                "[--best-effort-time-limit <seconds>] "
                "[--seed-ub-time-limit <seconds>] [--output <path>] "
                "[--run-id <id>]\n";
        cerr << "Error: " << parse_error << endl;
        return 2;
    }

    string instance_base = basename_no_ext(opt.instance_path);
    string size_class = size_class_from_path(opt.instance_path);

    string run_id = opt.run_id_override;
    if (run_id.empty()) {
        run_id = sanitize_for_filename(instance_base) + "__" + to_upper(opt.method);
        if (opt.has_epsilon) {
            run_id += "__eps_" + epsilon_tag(opt.epsilon);
        }
    }

    if (opt.output_path.empty()) {
        opt.output_path = "results/raw/" + run_id + ".csv";
    }

    Row row;
    add_str(row, "run_id", run_id);
    add_str(row, "instance", opt.instance_path);
    add_str(row, "size_class", size_class);
    add_str(row, "method", opt.method);
    if (opt.has_epsilon) add_double(row, "epsilon", opt.epsilon); else add_na(row, "epsilon");
    add_double(row, "time_limit_sec", opt.time_limit);

    // Fields filled in per-method below; pre-populate as NA so every
    // column is always present regardless of which branch runs or
    // whether an exception is thrown partway through.
    const vector<string> remaining_columns = {
        "runtime_sec", "time_to_best_sec", "solver_status", "feasible",
        "optimality_proven", "best_WWS", "WWS_lower_bound", "WWS_gap_percent",
        "best_Cmax", "Cmax_lower_bound", "Cmax_gap_percent", "actual_Cmax",
        "LB_AP", "gap_to_assignment_lb_percent", "global_LB", "global_UB",
        "master_assignments", "master_levels", "pool_size",
        "zero_idle_checks", "zero_idle_feasible", "zero_idle_infeasible",
        "zero_idle_unknown", "threshold_calls", "threshold_skipped_analytical",
        "exact_match_calls", "best_effort_calls", "threads", "seed",
        "exit_code", "error_message", "timestamp_utc", "git_commit", "hostname"
    };
    map<string, size_t> col_index;
    for (const auto& c : remaining_columns) {
        col_index[c] = row.size();
        add_na(row, c);
    }
    auto set_field = [&](const string& name, const string& value) {
        row[col_index.at(name)].second = value;
    };
    auto set_field_d = [&](const string& name, double value) {
        ostringstream ss;
        ss.setf(std::ios::fixed);
        ss << setprecision(6) << value;
        set_field(name, ss.str());
    };
    auto set_field_i = [&](const string& name, long long value) {
        set_field(name, to_string(value));
    };
    auto set_field_b = [&](const string& name, bool value) {
        set_field(name, value ? "1" : "0");
    };

    set_field_i("threads", kThreads);
    set_field("timestamp_utc", get_timestamp_utc());
    set_field("hostname", get_hostname());
    // git_commit is intentionally left NA here: captured once per
    // experiment batch by the job generator (see experiments/README.md),
    // not re-derived per solver run (avoids spawning a subprocess from
    // inside a timed solver process).

    int exit_code = 0;

    try {
        DRCRFFSP_Instance instance(opt.instance_path);

        if (opt.method == "cp_wws" || opt.method == "cp_wws_eps") {

            CP_Model model(instance);
            model.set_time_limit(opt.time_limit);
            CPSolveInfo info;
            double cmax_epsilon = (opt.method == "cp_wws_eps") ? opt.epsilon : -1.0;
            float wws = model.solve_obj(instance, 2, cmax_epsilon, &info);

            set_field("solver_status", status_string(info.status));
            bool feasible = (info.status == IloAlgorithm::Optimal || info.status == IloAlgorithm::Feasible);
            set_field_b("feasible", feasible);
            set_field_b("optimality_proven", info.status == IloAlgorithm::Optimal);
            set_field_d("runtime_sec", info.solve_time_sec >= 0 ? info.solve_time_sec : 0.0);
            // time_to_best_sec: NA -- solve_obj() uses a single blocking
            // cp_.solve() call (unchanged, reused as-is), which does not
            // expose per-incumbent timestamps; only total solve time is
            // available. See CPSolveInfo's doc comment in CP_Model.h.

            if (feasible) {
                set_field_d("best_WWS", wws);
                set_field_d("actual_Cmax", info.secondary_value);
                if (info.bound >= 0) {
                    set_field_d("WWS_lower_bound", info.bound);
                    set_field_d("WWS_gap_percent", gap_percent(wws, info.bound));
                }
            }

        }
        else if (opt.method == "cp_cmax") {

            CP_Model model(instance);
            model.set_time_limit(opt.time_limit);
            CPSolveInfo info;
            float cmax = model.solve_obj(instance, 1, -1.0, &info);

            set_field("solver_status", status_string(info.status));
            bool feasible = (info.status == IloAlgorithm::Optimal || info.status == IloAlgorithm::Feasible);
            set_field_b("feasible", feasible);
            set_field_b("optimality_proven", info.status == IloAlgorithm::Optimal);
            set_field_d("runtime_sec", info.solve_time_sec >= 0 ? info.solve_time_sec : 0.0);

            if (feasible) {
                set_field_d("best_Cmax", cmax);
                set_field_d("best_WWS", info.secondary_value);   // WWS of the returned schedule
                if (info.bound >= 0) {
                    set_field_d("Cmax_lower_bound", info.bound);
                    set_field_d("Cmax_gap_percent", gap_percent(cmax, info.bound));
                }
            }

        }
        else if (opt.method == "cp_lex") {

            CP_Model model(instance);
            model.set_time_limit(opt.time_limit);
            CPSolveInfo info;
            auto lex_result = model.solve_static_lex(opt.time_limit, &info);
            float cmax = get<0>(lex_result);
            float wws = get<1>(lex_result);

            bool feasible = (info.status == IloAlgorithm::Optimal || info.status == IloAlgorithm::Feasible);
            // See section 8: never report ambiguous "OPTIMAL" -- an
            // Optimal status here proves the COMPLETE lexicographic
            // optimum (both criteria jointly), which is why this uses a
            // distinct label rather than reusing status_string()'s
            // generic OPTIMAL. CP Optimizer's static-lex objective does
            // not expose a verified per-criterion (Cmax-only) proof
            // through solve_static_lex()'s existing single-solve()
            // pattern, so that finer distinction is not reported --
            // WWS_lower_bound/Cmax_lower_bound stay NA for this method.
            if (info.status == IloAlgorithm::Optimal) {
                set_field("solver_status", "LEX_OPTIMAL");
            } else {
                set_field("solver_status", status_string(info.status));
            }
            set_field_b("feasible", feasible);
            set_field_b("optimality_proven", info.status == IloAlgorithm::Optimal);
            set_field_d("runtime_sec", info.solve_time_sec >= 0 ? info.solve_time_sec : 0.0);

            if (feasible) {
                set_field_d("best_Cmax", cmax);
                set_field_d("best_WWS", wws);
                set_field_d("actual_Cmax", cmax);
            }

        }
        else if (opt.method == "lb_ap" || opt.method == "lb_ap_eps") {

            double cmax_epsilon = (opt.method == "lb_ap_eps") ? opt.epsilon : -1.0;
            auto t0 = chrono::steady_clock::now();
            MASTER_Model master(instance, cmax_epsilon, opt.time_limit);
            MasterSolution sol = master.solve_master();
            auto t1 = chrono::steady_clock::now();

            set_field_d("runtime_sec", chrono::duration<double>(t1 - t0).count());
            set_field_b("feasible", sol.feasible);
            set_field_b("optimality_proven", sol.feasible && sol.proven_optimal);

            if (sol.feasible) {
                set_field("solver_status", sol.proven_optimal ? "OPTIMAL" : "FEASIBLE_TIME_LIMIT");
                set_field_d("LB_AP", sol.objective_value);
                // Intentionally NOT reported as best_WWS -- LB_AP is a
                // reference lower bound, not a feasible schedule (section 9).
            } else {
                set_field("solver_status", "INFEASIBLE");
            }
            // MASTER_Model's constructor always pins CPLEX RandomSeed=1.
            set_field_i("seed", 1);

        }
        else if (opt.method == "decomp_wws" || opt.method == "decomp_eps") {

            MASTER_Model master(instance);   // unconstrained; decomp_eps
                                              // builds its OWN fresh
                                              // epsilon-specific master
                                              // internally via
                                              // run_for_epsilon() and does
                                              // not use this one -- kept
                                              // only to satisfy
                                              // Decomposition's
                                              // constructor (see main.cpp's
                                              // legacy "epsilon" mode,
                                              // which does the same thing).
            Decomposition decomp(instance, master);

            bool has_incumbent, optimality_proven, unresolved, time_limit_reached;
            double global_lb, global_ub = -1.0, achieved_makespan = -1.0;
            int pool_size;
            DecompositionStats stats;

            if (opt.method == "decomp_wws") {
                auto t0 = chrono::steady_clock::now();
                DecompositionResult r = decomp.run(
                    opt.best_effort_time_limit, opt.seed_ub_time_limit, opt.time_limit);
                auto t1 = chrono::steady_clock::now();
                has_incumbent = r.has_incumbent;
                optimality_proven = r.optimality_proven;
                unresolved = r.unresolved;
                time_limit_reached = r.time_limit_reached;
                global_lb = r.global_lb;
                global_ub = r.global_ub;
                achieved_makespan = r.achieved_makespan;
                stats = r.stats;
                pool_size = stats.persistent_pool_size;
                set_field_d("runtime_sec", chrono::duration<double>(t1 - t0).count());
            }
            else {
                EpsilonResult r = decomp.run_for_epsilon(
                    opt.epsilon, opt.best_effort_time_limit, opt.seed_ub_time_limit, opt.time_limit);
                has_incumbent = r.has_incumbent;
                optimality_proven = r.optimality_proven;
                unresolved = r.unresolved;
                time_limit_reached = r.time_limit_reached;
                global_lb = r.wws_lb;
                global_ub = r.wws_ub;
                achieved_makespan = r.achieved_makespan;
                stats = r.stats;
                pool_size = r.pool_size;
                set_field_d("runtime_sec", r.runtime_seconds);
            }

            set_field_b("feasible", has_incumbent);
            set_field_b("optimality_proven", optimality_proven);
            set_field_d("global_LB", global_lb);
            if (has_incumbent) {
                set_field_d("global_UB", global_ub);
                set_field_d("best_WWS", global_ub);
                set_field_d("actual_Cmax", achieved_makespan);
            }

            if (optimality_proven) {
                set_field("solver_status", "OPTIMAL");
            } else if (has_incumbent) {
                set_field("solver_status", "FEASIBLE_TIME_LIMIT");
            } else if (unresolved) {
                set_field("solver_status", "UNKNOWN");
            } else {
                set_field("solver_status", "INFEASIBLE");
            }
            if (time_limit_reached) {
                set_field("error_message", "total_decomposition_time_budget_exhausted");
            }

            set_field_i("master_assignments", stats.master_assignments_examined);
            set_field_i("master_levels", stats.master_levels_examined);
            set_field_i("pool_size", pool_size);
            set_field_i("zero_idle_checks", stats.zero_idle_checks);
            set_field_i("zero_idle_feasible", stats.zero_idle_feasible);
            set_field_i("zero_idle_infeasible", stats.zero_idle_infeasible);
            set_field_i("zero_idle_unknown", stats.zero_idle_unknown);
            set_field_i("threshold_calls", stats.threshold_calls);
            set_field_i("threshold_skipped_analytical", stats.threshold_skipped_analytical);
            set_field_i("exact_match_calls", stats.exact_match_calls);
            set_field_i("best_effort_calls", stats.best_effort_calls);

            // CPLEX RandomSeed is fixed to 1 in every MASTER_Model this
            // method constructs (see Masterproblem.cpp); CP Optimizer's
            // per-assignment subproblems use its own default seed (not
            // pinned -- see experiments/README.md's reproducibility note).
            set_field_i("seed", 1);
        }

        set_field_i("exit_code", 0);

    }
    catch (const IloException& e) {
        cerr << "IloException: " << e.getMessage() << endl;
        set_field("solver_status", "ERROR");
        set_field("error_message", string("IloException: ") + e.getMessage());
        set_field_i("exit_code", 1);
        exit_code = 1;
    }
    catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        set_field("solver_status", "ERROR");
        set_field("error_message", string("exception: ") + e.what());
        set_field_i("exit_code", 1);
        exit_code = 1;
    }
    catch (...) {
        cerr << "Unknown exception during experiment run." << endl;
        set_field("solver_status", "ERROR");
        set_field("error_message", "unknown exception");
        set_field_i("exit_code", 1);
        exit_code = 1;
    }

    write_csv_row(row, opt.output_path);
    cout << "Result written to " << opt.output_path << endl;

    return exit_code;
}
