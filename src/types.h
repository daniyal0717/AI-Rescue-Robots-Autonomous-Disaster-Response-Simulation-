// types.h
// Enumerations shared by all modules, with the exact keywords used in scenario files.
#ifndef TYPES_H
#define TYPES_H

#include "container.h"

enum LocType { LT_JUNCTION, LT_BUILDING, LT_HOSPITAL, LT_DEPOT, LT_CHARGING, LT_COMMAND };

// Category and Capability share their first four values on purpose:
// a robot of capability c serves category g if c == CAP_GENERAL or c == g.
enum Category { CAT_MEDICAL, CAT_FIRE, CAT_SUPPLY, CAT_SEARCH };
enum Capability { CAP_MEDICAL, CAP_FIRE, CAP_SUPPLY, CAP_SEARCH, CAP_GENERAL };

enum EventType {
    EV_BLOCK, EV_UNBLOCK, EV_STATION_DOWN, EV_STATION_UP,
    EV_SET_URGENCY, EV_ROBOT_OFFLINE, EV_ROBOT_ONLINE
};

static const char* const LOC_TYPE_NAMES[] = {
    "JUNCTION", "BUILDING", "HOSPITAL", "DEPOT", "CHARGING", "COMMAND" };
static const char* const CATEGORY_NAMES[] = { "MEDICAL", "FIRE", "SUPPLY", "SEARCH" };
static const char* const CAPABILITY_NAMES[] = { "MEDICAL", "FIRE", "SUPPLY", "SEARCH", "GENERAL" };
static const char* const EVENT_NAMES[] = {
    "BLOCK", "UNBLOCK", "STATION_DOWN", "STATION_UP",
    "SET_URGENCY", "ROBOT_OFFLINE", "ROBOT_ONLINE" };

// index of name in names[0..count-1], or -1
inline int lookupName(const char* name, const char* const* names, int count) {
    for (int i = 0; i < count; i++)
        if (strcmp(name, names[i]) == 0) return i;
    return -1;
}

inline bool capabilityServes(int capability, int category) {
    return capability == CAP_GENERAL || capability == category;
}

#endif
