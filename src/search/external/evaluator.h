#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <set>

class Heuristic;

class Evaluator {
public:
    Evaluator() = default;
    virtual ~Evaluator() = default;

    /*
      dead_ends_are_reliable should return true if the evaluator is
      "safe", i.e., infinite estimates can be trusted.

      The default implementation returns true.
    */
    virtual bool dead_ends_are_reliable() const;
};

#endif
