#ifndef EXTERNAL_OPEN_LISTS_EXTERNAL_ASTAR_OPEN_LIST_H
#define EXTERNAL_OPEN_LISTS_EXTERNAL_ASTAR_OPEN_LIST_H

#include "../../open_list_factory.h"
#include "../../option_parser_util.h"

namespace external_astar_open_list {
class ExternalAStarOpenListFactory : public OpenListFactory {
    Options options;
public:
    explicit ExternalAStarOpenListFactory(const Options &options);
    virtual ~ExternalAStarOpenListFactory() override = default;

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
};
}

#endif
