#ifndef EXTERNAL_OPEN_LIST_ASTAR_DDD_OPEN_LIST_H
#define EXTERNAL_OPEN_LIST_ASTAR_DDD_OPEN_LIST_H

#include "../../open_list_factory.h"
#include "../../option_parser_util.h"
// hash based duplicate detection

namespace astar_ddd_open_list {
class AStarDDDOpenListFactory : public OpenListFactory {
    Options options;
public:
    explicit AStarDDDOpenListFactory(const Options &options);
    virtual ~AStarDDDOpenListFactory() override = default;

    virtual std::unique_ptr<StateOpenList> create_state_open_list() override;
};
}

#endif
