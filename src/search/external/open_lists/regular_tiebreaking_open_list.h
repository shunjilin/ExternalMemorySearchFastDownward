#ifndef EXTERNAL_OPEN_LISTS_REGULAR_TIEBREAKING_OPEN_LIST_H
#define EXTERNAL_OPEN_LISTS_REGULAR_TIEBREAKING_OPEN_LIST_H

#include "../../open_list_factory.h"
#include "../../option_parser_util.h"

namespace regular_tiebreaking_open_list {
class RegularTieBreakingOpenListFactory : public OpenListFactory {
    Options options;
public:
    explicit RegularTieBreakingOpenListFactory(const Options &options);
    virtual ~RegularTieBreakingOpenListFactory() override = default;

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
};
}

#endif
