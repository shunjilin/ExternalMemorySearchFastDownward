#ifndef EXTERNAL_CLOSED_LIST_FACTORY_H
#define EXTERNAL_CLOSED_LIST_FACTORY_H

#include "closed_list.h"
#include <memory>

class ClosedListFactory {
 public:
    ClosedListFactory() = default;
    virtual ~ClosedListFactory() = default;

    ClosedListFactory(const ClosedListFactory &) = delete;

    virtual std::unique_ptr<StateClosedList> create_state_closed_list() = 0;

    template<typename T>
    std::unique_ptr<ClosedList<T>> create_closed_list();
};



#endif
