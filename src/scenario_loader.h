// scenario_loader.h
// Reads and validates a scenario file (format of scenario_provided.txt).
// The loader produces plain records only. It never touches simulation state.
#ifndef SCENARIO_LOADER_H
#define SCENARIO_LOADER_H

#include "container.h"
#include "types.h"

struct Params {
    int maxSteps;
    int speed;
    int batteryPerCostUnit;
    int batteryPerWorkStep;
    int rechargePerStep;
    // optional extensions, defaults used when the key is absent
    int requestTimeout;   // steps a request may stay unassigned before FAILED
    int rerouteWait;      // steps a robot waits at a node for a blocked route to reopen
    int lowBatteryPct;    // idle robot below this percent of max battery goes to charge
    bool maxStepsGiven;

    Params()
        : maxSteps(250), speed(1), batteryPerCostUnit(1), batteryPerWorkStep(2),
          rechargePerStep(5), requestTimeout(150), rerouteWait(5), lowBatteryPct(40),
          maxStepsGiven(false) {}
};

struct LocRec {
    int id; ds::String name; int type; int line;
    LocRec() : id(0), type(0), line(0) {}
};
struct ConnRec {
    int id; int a; int b; int cost; bool blocked; int line;
    ConnRec() : id(0), a(0), b(0), cost(0), blocked(false), line(0) {}
};
struct RobotRec {
    int id; int start; int capability; int maxBattery; int initBattery; int line;
    RobotRec() : id(0), start(0), capability(0), maxBattery(0), initBattery(0), line(0) {}
};
struct ReqRec {
    int id; int time; int location; int category; int urgency; int duration; int line;
    ReqRec() : id(0), time(0), location(0), category(0), urgency(1), duration(1), line(0) {}
};
struct EventRec {
    int time;
    int type;     // EventType
    int target;   // connection, location, request or robot id, depending on type
    int value;    // new urgency for SET_URGENCY, otherwise 0
    int line;
    EventRec() : time(0), type(0), target(0), value(0), line(0) {}
};

struct Scenario {
    ds::String name;
    Params params;
    ds::Vector<LocRec> locations;
    ds::Vector<ConnRec> connections;
    ds::Vector<RobotRec> robots;
    ds::Vector<ReqRec> requests;      // stable-sorted by report time
    ds::Vector<EventRec> events;      // stable-sorted by time, file order kept within a time

    int blockedCount() const {
        int n = 0;
        for (int i = 0; i < connections.size(); i++) if (connections[i].blocked) n++;
        return n;
    }
};

struct LoadReport {
    ds::Vector<ds::String> errors;     // each "line N: message"
    ds::Vector<ds::String> warnings;
};

// Returns true when the file is valid (no errors). Warnings do not make it fail.
// On false, the scenario must not be used. Never crashes on bad input.
bool loadScenario(const char* path, Scenario& sc, LoadReport& report);

#endif
