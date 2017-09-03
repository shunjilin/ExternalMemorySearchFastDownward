#ifndef EXTERNAL_CLOSED_LISTS_REGULAR_H
#define EXTERNAL_CLOSED_LISTS_REGULAR_H

#include "../closed_list_factory.h"
#include "../../option_parser_util.h"

namespace regular_closed_list {
    class RegularClosedListFactory : public ClosedListFactory {
        Options options;
    public:
        explicit RegularClosedListFactory(const Options &options);
        virtual ~RegularClosedListFactory() override = default;

        virtual std::unique_ptr<StateClosedList>
            create_state_closed_list() override;
    };
}

#endif
