#ifndef EXTERNAL_CLOSED_LIST_H
#define EXTERNAL_CLOSED_LIST_H

#include <utility>
#include "../global_state.h"
#include "../global_operator.h"

using found = bool;
using reopened = bool;

template<class Entry>
class ClosedList {
    bool reopen_closed;
 public:
    explicit ClosedList(bool reopen_closed = true);
    virtual ~ClosedList() = default;
    virtual std::pair<found, reopened> find_insert(const Entry &entry) = 0;
    virtual std::vector<const GlobalOperator *>
        trace_path(const Entry &entry) const = 0;
};

using StateClosedListEntry = GlobalState;
using StateClosedList = ClosedList<StateClosedListEntry>;

template<class Entry>
ClosedList<Entry>::ClosedList(bool reopen_closed)
: reopen_closed(reopen_closed) {
}


#endif
