# CLAUDE.md

## 1. Project purpose

This repository contains the C++ implementation used for a research project
on a Dual-Resource-Constrained Re-entrant Flexible Flow Shop Scheduling Problem
(DRCRFFSP).

The project studies scheduling decisions involving:

- jobs and operations,
- production stages,
- parallel/flexible machines,
- workers,
- worker qualifications/skills,
- re-entrant production flows,
- machine capacity,
- worker capacity,
- makespan,
- and worker-related cost/time objectives.

The implementation is part of an academic research paper.

Preserving the mathematical meaning of the scheduling problem is more important
than simplifying or aggressively refactoring the code.


## 2. Current research direction

The research is developing a bi-objective scheduling approach.

The objectives are:

1. Makespan

   C_max

2. Weighted Worker Span (WWS)

   For worker w:

   T_w = E_w^max - S_w^min

   where:

   - S_w^min is the start time of the first operation assigned to worker w,
   - E_w^max is the end time of the last operation assigned to worker w.

   The weighted worker-span objective is:

   WWS = sum_w c_w * T_w

   where c_w is the cost/weight associated with worker w.

The interpretation of worker span is important:
it measures the interval between a worker's first and last assigned operation,
not simply the sum of processing times assigned to the worker.


## 3. Status of the bi-objective work

The bi-objective formulation and solution procedure are under active development.

Do NOT assume that every class or source file already implements the final
mathematical formulation described above.

In particular, experimental or partially implemented code may exist.

When modifying optimization code:

- distinguish the current implementation from the intended formulation,
- do not silently replace one formulation with another,
- explain changes to objectives or constraints,
- and ask before making a major modeling assumption that is not evident
  from the existing code or research specification.


## 4. Technology

The project currently uses:

- C++17
- CMake
- Visual Studio Code
- Microsoft Visual C++ / Visual Studio 2022
- IBM ILOG CPLEX Optimization Studio 20.1
- IBM ILOG CP Optimizer
- IBM ILOG Concert Technology

CPLEX Optimization Studio is currently installed at:

    C:/Program Files/IBM/ILOG/CPLEX_Studio201

The scheduling model uses the C++ Concert/CP Optimizer API.

Typical CP Optimizer include:

    #include <ilcp/cp.h>


## 5. Build system

The project uses CMake.

Main executable target:

    drcrffsp

Both Debug and Release configurations are supported.

Typical executables:

    build/Debug/drcrffsp.exe
    build/Release/drcrffsp.exe

Use Release builds for computational experiments and performance measurements.

Debug builds are primarily for development and debugging.


## 6. IBM library configuration

Debug and Release IBM libraries must NOT be mixed.

Current intended mapping:

    Debug   -> stat_mdd
    Release -> stat_mda

Mixing these configurations can cause linker errors involving:

    _ITERATOR_DEBUG_LEVEL
    RuntimeLibrary
    MDd_DynamicDebug
    MD_DynamicRelease

The project currently links CP Optimizer, Concert, and CPLEX.

For the installed CPLEX 20.1 distribution, the CPLEX library is named:

    cplex2010.lib

Do not arbitrarily change the IBM library configuration if the existing
configuration builds successfully.


## 7. Project structure

The current source organization is approximately:

    Cost_decomposition/
    |
    |-- CMakeLists.txt
    |-- CLAUDE.md
    |
    |-- include/
    |   |-- CP_Model.h
    |   |-- DRCRFFSP_Instance.h
    |   |-- Job.h
    |   |-- Machine.h
    |   |-- Masterproblem.h
    |   |-- Operation.h
    |   |-- Stage.h
    |   `-- Worker.h
    |
    `-- src/
        |-- CP_Model.cpp
        |-- DRCRFFSP_Instance.cpp
        |-- Job.cpp
        |-- Machine.cpp
        |-- main.cpp
        |-- Masterproblem.cpp
        |-- Operation.cpp
        |-- Stage.cpp
        `-- Worker.cpp

Check the actual repository before assuming that this list is exhaustive.


## 8. Main domain classes

Important classes currently include:

### DRCRFFSP_Instance

Represents the scheduling instance and contains the jobs, stages, workers,
processing information, and instance dimensions.

Useful dimensions include:

- n: number of jobs
- s: number of stages
- m: number of machines
- w: number of workers
- r: number of levels
- o: number of operations


### Job

Represents a job and its operations.


### Operation

Represents an operation.

Relevant information includes:

- job ID,
- operation ID,
- processing duration,
- stage,
- level,
- assigned machine,
- start time,
- finish time.


### Stage

Represents a production stage.


### Machine

Represents a machine/resource.


### Worker

Represents a worker.

Worker information includes:

- worker ID,
- worker cost,
- eligible stages/skills,
- availability/scheduling information.


### CP_Model

Contains CP Optimizer scheduling-model functionality.


### MASTER_Model

Contains work related to the master/decomposition formulation.

This part of the project is under development and must not automatically
be interpreted as the final research formulation.


## 9. Worker eligibility

Workers have stage qualifications/skills.

An operation requiring a particular stage can only be assigned to an
eligible worker.

Eligibility must be derived from the actual instance representation.

Do not assume that every worker can process every operation.


## 10. CP Optimizer modeling rules

Use actual IBM ILOG Concert / CP Optimizer C++ API constructs.

Do NOT invent Concert types or functions.

For example, do not assume types such as:

    IloBoolVarArray2

exist unless verified.

For multidimensional Concert structures, use supported nested arrays where
appropriate, for example:

    IloArray<IloBoolVarArray>

Preserve CP Optimizer interval-variable and scheduling semantics.

Before replacing a CP Optimizer construct with a custom C++ implementation,
consider whether doing so changes propagation or the mathematical model.


## 11. Development rules

When changing the repository:

1. Read the relevant headers and implementations before modifying interfaces.

2. Preserve the mathematical meaning of the scheduling model.

3. Prefer small, targeted changes over broad refactoring.

4. Do not rename major model concepts merely for stylistic reasons.

5. Do not invent missing API functions without checking the surrounding
   class structure.

6. Keep mathematical-model changes distinguishable from software-engineering
   changes.

7. After changing C++ code, build the project.

8. Resolve compilation and linker errors before considering a change complete.

9. Treat compiler warnings separately from actual build failures.

10. Do not change a working CMake/CPLEX configuration unnecessarily.

11. Use the Release configuration for computational experiments.

12. When changing an objective or constraint, explain what mathematical
    expression the new code represents.


## 12. Research-code discipline

This repository produces results intended for an academic paper.

Therefore:

- reproducibility matters,
- objective values must be computed consistently,
- runtime measurements must use appropriate builds,
- solver parameters should be recorded,
- time limits should be distinguished from proofs of optimality,
- and heuristic/time-limited solutions must not be described as exact
  optima without evidence.

A difference between two solutions obtained under a time limit is not
automatically evidence of a true Pareto trade-off.


## 13. Experimental interpretation

When analyzing computational results, distinguish between:

- best solution found,
- proven optimum,
- solver bound,
- optimality gap,
- time-limit termination,
- infeasibility,
- and proven infeasibility.

Do not infer a structural trade-off between makespan and worker span solely
because two time-limited solver runs return different makespans.


## 14. Planned work

The following should be treated as research directions, not necessarily
already implemented features:

- development of the bi-objective solution approach,
- analysis of makespan versus Weighted Worker Span,
- generation/analysis of Pareto-efficient solutions,
- workforce configuration experiments,
- cross-training/flexibility experiments,
- comparison of fixed and reduced workforce configurations,
- and computational experiments for the paper.

Before implementing a planned algorithmic approach, check the current
research specification and existing code rather than assuming a particular
multi-objective method.


## 15. Working with this repository

When asked to implement a change:

1. Inspect the relevant existing code.
2. State what part of the mathematical/model logic is affected.
3. Make the smallest coherent change.
4. Compile the project.
5. Diagnose errors from the actual compiler/linker output.
6. Do not guess at IBM Concert APIs.
7. Verify that the resulting code still represents the intended scheduling
   formulation.

When there is ambiguity between software behavior and research intent,
preserve the existing implementation and ask for clarification rather than
silently changing the model.


## 16. Updating this file

CLAUDE.md should evolve with the project.

Update it when stable facts change, for example:

- the final bi-objective algorithm is selected,
- the mathematical formulation changes,
- new experiment types become established,
- new major source components are introduced,
- build requirements change,
- or important modeling conventions are established.

Do not fill this file with temporary debugging information, individual
compiler errors that have already been resolved, or speculative algorithmic
ideas that have not been adopted.