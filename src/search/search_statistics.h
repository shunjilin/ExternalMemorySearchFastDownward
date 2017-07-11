#ifndef SEARCH_STATISTICS_H
#define SEARCH_STATISTICS_H

#include <cstddef>

/*
  This class keeps track of search statistics.

  It keeps counters for expanded, generated and evaluated states (and
  some other statistics) and provides uniform output for all search
  methods.
*/

class SearchStatistics {
    // General statistics
    size_t expanded_states;  // no states for which successors were generated
    size_t evaluated_states; // no states for which h fn was computed
    size_t evaluations;      // no of heuristic evaluations performed
    size_t generated_states; // no states created in total (plus those removed since already in close list)
    size_t reopened_states;  // no of *closed* states which we reopened
    size_t dead_end_states;

    size_t generated_ops;    // no of operators that were returned as applicable

    // Statistics related to f values
    int lastjump_f_value; //f value obtained in the last jump
    size_t lastjump_expanded_states; // same guy but at posize_t where the last jump in the open list
    size_t lastjump_reopened_states; // occurred (jump == f-value of the first node in the queue increases)
    size_t lastjump_evaluated_states;
    size_t lastjump_generated_states;

    void print_f_line() const;
public:
    SearchStatistics();
    ~SearchStatistics() = default;

    // Methods that update statistics.
    void inc_expanded(size_t inc = 1) {expanded_states += inc; }
    void inc_evaluated_states(size_t inc = 1) {evaluated_states += inc; }
    void inc_generated(size_t inc = 1) {generated_states += inc; }
    void inc_reopened(size_t inc = 1) {reopened_states += inc; }
    void inc_generated_ops(size_t inc = 1) {generated_ops += inc; }
    void inc_evaluations(size_t inc = 1) {evaluations += inc; }
    void inc_dead_ends(size_t inc = 1) {dead_end_states += inc; }

    // Methods that access statistics.
    size_t get_expanded() const {return expanded_states; }
    size_t get_evaluated_states() const {return evaluated_states; }
    size_t get_evaluations() const {return evaluations; }
    size_t get_generated() const {return generated_states; }
    size_t get_reopened() const {return reopened_states; }
    size_t get_generated_ops() const {return generated_ops; }

    /*
      Call the following method with the f value of every expanded
      state. It will notice "jumps" (i.e., when the expanded f value
      is the highest f value encountered so far), print_t some
      statistics on jumps, and keep track of expansions etc. up to the
      last jump.

      Statistics until the final jump are often useful to report in
      A*-style searches because they are not affected by tie-breaking
      as the overall statistics. (With a non-random, admissible and
      consistent heuristic, the number of expanded, evaluated and
      generated states until the final jump is fully determined by the
      state space and heuristic, independently of things like the
      order in which successors are generated or the tie-breaking
      performed by the open list.)
    */
    void report_f_value_progress(int f);

    // output
    void print_basic_statistics() const;
    void print_detailed_statistics() const;
};

#endif
