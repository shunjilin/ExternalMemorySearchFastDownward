#ifndef EXTERNAL_HEURISTIC_H
#define EXTERNAL_HEURISTIC_H

#include "node.h"
#include "evaluator.h"
#include "../operator_id.h"
#include "../task_proxy.h"

#include "../algorithms/ordered_set.h"

#include <memory>
#include <vector>

class GlobalOperator;
class GlobalState;
class TaskProxy;

namespace options {
class OptionParser;
class Options;
}

class Heuristic : public Evaluator {
    int h;
    std::string description;

protected:
    // Hold a reference to the task implementation and pass it to objects that need it.
    const std::shared_ptr<AbstractTask> task;
    // Use task_proxy to access task information.
    TaskProxy task_proxy;

    enum {DEAD_END = -1, NO_VALUE = -2};

    // TODO: Call with State directly once all heuristics support it.
    virtual int compute_heuristic(const Node &node) = 0;

    /*
      Usage note: Marking the same operator as preferred multiple times
      is OK -- it will only appear once in the list of preferred
      operators for this heuristic.
    */
    void set_preferred(const OperatorProxy &op);

    /* TODO: Make private and use State instead of GlobalState once all
       heuristics use the TaskProxy class. */
    State convert_node(const Node &node) const;

public:
    explicit Heuristic(const options::Options &options);
    virtual ~Heuristic() override;

    std::string get_description() const;
};

#endif
