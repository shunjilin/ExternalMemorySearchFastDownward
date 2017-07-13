#ifndef EXTERNAL_OPEN_LISTS_EXTERNAL_TIEBREAKING_OPEN_LIST_H
#define EXTERNAL_OPEN_LISTS_EXTERNAL_TIEBREAKING_OPEN_LIST_H

#include "../../open_list_factory.h"
#include "../../option_parser_util.h"

namespace external_tiebreaking_open_list {
class ExternalTieBreakingOpenListFactory : public OpenListFactory {
    Options options;
public:
    explicit ExternalTieBreakingOpenListFactory(const Options &options);
    virtual ~ExternalTieBreakingOpenListFactory() override = default;

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
};
}

#endif
