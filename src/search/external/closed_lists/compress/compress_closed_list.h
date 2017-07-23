#ifndef EXTERNAL_CLOSED_LISTS_COMPRESS_H
#define EXTERNAL_CLOSED_LISTS_COMPRESS_H

#include "../../closed_list_factory.h"
#include "../../../option_parser_util.h"

namespace compress_closed_list {
    class CompressClosedListFactory : public ClosedListFactory {
        Options options;
    public:
        explicit CompressClosedListFactory(const Options &options);
        virtual ~CompressClosedListFactory() override = default;

        virtual std::unique_ptr<StateClosedList>
            create_state_closed_list() override;
    };
}

#endif
