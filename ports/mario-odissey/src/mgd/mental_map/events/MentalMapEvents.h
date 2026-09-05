#pragma once

#include "../Entity.h"

namespace mgd {

struct EntityAddedEvent {
    EntityID id;
};

struct EntityRemovedEvent {
    EntityID id;
};

struct EntityMovedEvent {
    EntityID id;
    Vec3 old_pos;
    Vec3 new_pos;
};

struct EntityUpdatedEvent {
    EntityID id;
};

struct RegionLoadedEvent {
    RegionID id;
};

struct RegionUnloadedEvent {
    RegionID id;
};

} // namespace mgd
