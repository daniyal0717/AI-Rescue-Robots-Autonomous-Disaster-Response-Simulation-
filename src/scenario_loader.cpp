// scenario_loader.cpp
#include "scenario_loader.h"

using namespace ds;

namespace {

const int MAX_ERRORS = 50;
enum { SEC_LOC, SEC_CON, SEC_ROB, SEC_REQ, SEC_EVT, SEC_COUNT };

struct Line {
    String text;
    int no;
};

struct Ctx {
    Scenario& sc;
    LoadReport& rep;
    Vector<Line> lines;       // comments and blank lines already removed
    int pos;
    bool seen[SEC_COUNT];
    Ctx(Scenario& s, LoadReport& r) : sc(s), rep(r), pos(0) {
        for (int i = 0; i < SEC_COUNT; i++) seen[i] = false;
    }
};

// ------------------------------------------------------------ messages
void addError(LoadReport& rep, int line, const String& msg) {
    if (rep.errors.size() > MAX_ERRORS) return;
    if (rep.errors.size() == MAX_ERRORS) {
        rep.errors.push_back(String("too many errors, the rest are not shown"));
        return;
    }
    String e;
    if (line > 0) e.append("line ").appendInt(line).append(": ");
    e.append(msg);
    rep.errors.push_back(e);
}
void addError(LoadReport& rep, int line, const char* msg) { addError(rep, line, String(msg)); }

void addWarning(LoadReport& rep, int line, const String& msg) {
    String w;
    if (line > 0) w.append("line ").appendInt(line).append(": ");
    w.append(msg);
    rep.warnings.push_back(w);
}

// "<what> <id>: <msg>"
void addIdError(LoadReport& rep, int line, const char* what, int id, const char* msg) {
    String m;
    m.append(what).append(' ').appendInt(id).append(": ").append(msg);
    addError(rep, line, m);
}

// ------------------------------------------------------------ text helpers
bool isBlankOrComment(const String& t) {
    const char* s = t.c_str();
    int i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    return s[i] == '\0' || s[i] == '#';
}

void tokenize(const String& t, Vector<String>& toks) {
    toks.clear();
    const char* s = t.c_str();
    int i = 0;
    while (s[i]) {
        while (s[i] == ' ' || s[i] == '\t') i++;
        if (!s[i]) break;
        String tok;
        while (s[i] && s[i] != ' ' && s[i] != '\t') { tok.append(s[i]); i++; }
        toks.push_back(tok);
    }
}

// strict integer: optional '-', digits only, magnitude up to 1,000,000,000
bool parseInt(const String& s, int& out) {
    const char* c = s.c_str();
    int i = 0;
    bool neg = false;
    if (c[0] == '-') { neg = true; i = 1; }
    if (c[i] == '\0') return false;
    long long v = 0;
    for (; c[i]; i++) {
        if (c[i] < '0' || c[i] > '9') return false;
        v = v * 10 + (c[i] - '0');
        if (v > 1000000000LL) return false;
    }
    out = (int)(neg ? -v : v);
    return true;
}

bool isKeyword(const String& t) {
    return t == "SCENARIO" || t == "PARAM" || t == "LOCATIONS" || t == "CONNECTIONS" ||
           t == "ROBOTS" || t == "REQUESTS" || t == "EVENTS" || t == "END";
}

// ------------------------------------------------------------ reading
void pushLine(Ctx& c, const String& text, int no) {
    if (isBlankOrComment(text)) return;
    Line l;
    l.text = text;
    l.no = no;
    c.lines.push_back(l);
}

bool readLines(const char* path, Ctx& c) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        String m;
        m.append("cannot open file '").append(path).append("'");
        addError(c.rep, 0, m);
        return false;
    }
    String cur;
    int no = 1;
    int ch;
    while ((ch = fgetc(f)) != EOF) {
        if (ch == '\n') { pushLine(c, cur, no); cur.clear(); no++; }
        else if (ch == '\0') cur.append(' ');
        else if (ch != '\r') cur.append((char)ch);
    }
    if (cur.length() > 0) pushLine(c, cur, no);
    fclose(f);
    return true;
}

// ------------------------------------------------------------ field parsing
bool intField(Ctx& c, int ln, const String& tok, const char* field, int& out) {
    if (parseInt(tok, out)) return true;
    String m;
    m.append(field).append(" '").append(tok).append("' is not a valid number");
    addError(c.rep, ln, m);
    return false;
}

// ------------------------------------------------------------ PARAM
bool setParam(Ctx& c, int ln, const char* key, int v, int lo, int hi, int& dst) {
    if (v < lo || v > hi) {
        String m;
        m.append("PARAM ").append(key).append(" must be between ").appendInt(lo)
         .append(" and ").appendInt(hi);
        addError(c.rep, ln, m);
        return false;
    }
    dst = v;
    return true;
}

void parseParam(Ctx& c, const Vector<String>& t, int ln) {
    if (t.size() != 3) { addError(c.rep, ln, "PARAM needs: PARAM key value"); return; }
    const char* k = t[1].c_str();
    static const char* const KNOWN[] = {
        "max_steps", "speed", "battery_per_cost_unit", "battery_per_work_step",
        "recharge_per_step", "request_timeout", "reroute_wait", "low_battery_pct" };
    if (lookupName(k, KNOWN, 8) < 0) {   // unknown keys are ignored whatever their value is
        String m;
        m.append("unknown PARAM key '").append(k).append("' ignored");
        addWarning(c.rep, ln, m);
        return;
    }
    int v;
    if (!intField(c, ln, t[2], "PARAM value", v)) return;
    Params& p = c.sc.params;
    if (strcmp(k, "max_steps") == 0) {
        if (setParam(c, ln, k, v, 1, 10000000, p.maxSteps)) p.maxStepsGiven = true;
    }
    else if (strcmp(k, "speed") == 0) setParam(c, ln, k, v, 1, 1000, p.speed);
    else if (strcmp(k, "battery_per_cost_unit") == 0) setParam(c, ln, k, v, 0, 1000, p.batteryPerCostUnit);
    else if (strcmp(k, "battery_per_work_step") == 0) setParam(c, ln, k, v, 0, 1000, p.batteryPerWorkStep);
    else if (strcmp(k, "recharge_per_step") == 0) setParam(c, ln, k, v, 1, 1000000, p.rechargePerStep);
    else if (strcmp(k, "request_timeout") == 0) setParam(c, ln, k, v, 1, 10000000, p.requestTimeout);
    else if (strcmp(k, "reroute_wait") == 0) setParam(c, ln, k, v, 0, 10000000, p.rerouteWait);
    else if (strcmp(k, "low_battery_pct") == 0) setParam(c, ln, k, v, 0, 100, p.lowBatteryPct);
}

// ------------------------------------------------------------ records
void parseLocation(Ctx& c, const Vector<String>& t, int ln) {
    if (t.size() != 3) { addError(c.rep, ln, "location record needs: id name type"); return; }
    LocRec r;
    r.line = ln;
    bool ok = intField(c, ln, t[0], "location id", r.id);
    if (ok && r.id < 0) { addError(c.rep, ln, "location id must not be negative"); ok = false; }
    r.name = t[1];
    r.type = lookupName(t[2].c_str(), LOC_TYPE_NAMES, 6);
    if (r.type < 0) {
        String m;
        m.append("unknown location type '").append(t[2]).append("'");
        addError(c.rep, ln, m);
        ok = false;
    }
    if (ok) c.sc.locations.push_back(r);
}

void parseConnection(Ctx& c, const Vector<String>& t, int ln) {
    if (t.size() != 5) {
        addError(c.rep, ln, "connection record needs: id location_a location_b cost status");
        return;
    }
    ConnRec r;
    r.line = ln;
    bool ok = intField(c, ln, t[0], "connection id", r.id);
    ok = intField(c, ln, t[1], "location_a", r.a) && ok;
    ok = intField(c, ln, t[2], "location_b", r.b) && ok;
    bool costOk = intField(c, ln, t[3], "cost", r.cost);
    if (costOk && r.cost <= 0) { addError(c.rep, ln, "connection cost must be greater than 0"); costOk = false; }
    ok = ok && costOk;
    if (t[4] == "OPEN") r.blocked = false;
    else if (t[4] == "BLOCKED") r.blocked = true;
    else {
        String m;
        m.append("unknown connection status '").append(t[4]).append("' (use OPEN or BLOCKED)");
        addError(c.rep, ln, m);
        ok = false;
    }
    if (ok && r.id < 0) { addError(c.rep, ln, "connection id must not be negative"); ok = false; }
    if (ok) c.sc.connections.push_back(r);
}

void parseRobot(Ctx& c, const Vector<String>& t, int ln) {
    if (t.size() != 5) {
        addError(c.rep, ln, "robot record needs: id start_location capability max_battery initial_battery");
        return;
    }
    RobotRec r;
    r.line = ln;
    bool ok = intField(c, ln, t[0], "robot id", r.id);
    ok = intField(c, ln, t[1], "start_location", r.start) && ok;
    r.capability = lookupName(t[2].c_str(), CAPABILITY_NAMES, 5);
    if (r.capability < 0) {
        String m;
        m.append("unknown capability '").append(t[2]).append("'");
        addError(c.rep, ln, m);
        ok = false;
    }
    bool mb = intField(c, ln, t[3], "max_battery", r.maxBattery);
    bool ib = intField(c, ln, t[4], "initial_battery", r.initBattery);
    if (mb && r.maxBattery <= 0) { addError(c.rep, ln, "max battery must be greater than 0"); mb = false; }
    if (ib && r.initBattery < 0) { addError(c.rep, ln, "initial battery must not be negative"); ib = false; }
    if (mb && ib && r.initBattery > r.maxBattery) {
        addError(c.rep, ln, "initial battery is above max battery");
        ib = false;
    }
    if (ok && r.id < 0) { addError(c.rep, ln, "robot id must not be negative"); ok = false; }
    if (ok && mb && ib) c.sc.robots.push_back(r);
}

void parseRequest(Ctx& c, const Vector<String>& t, int ln) {
    if (t.size() != 6) {
        addError(c.rep, ln, "request record needs: id report_time location category urgency duration_steps");
        return;
    }
    ReqRec r;
    r.line = ln;
    bool ok = intField(c, ln, t[0], "request id", r.id);
    bool tm = intField(c, ln, t[1], "report_time", r.time);
    if (tm && r.time < 0) { addError(c.rep, ln, "report time must not be negative"); tm = false; }
    ok = intField(c, ln, t[2], "location", r.location) && ok;
    r.category = lookupName(t[3].c_str(), CATEGORY_NAMES, 4);
    if (r.category < 0) {
        String m;
        m.append("unknown category '").append(t[3]).append("'");
        addError(c.rep, ln, m);
        ok = false;
    }
    bool ur = intField(c, ln, t[4], "urgency", r.urgency);
    if (ur && (r.urgency < 1 || r.urgency > 5)) { addError(c.rep, ln, "urgency must be between 1 and 5"); ur = false; }
    bool du = intField(c, ln, t[5], "duration_steps", r.duration);
    if (du && r.duration < 1) { addError(c.rep, ln, "duration_steps must be at least 1"); du = false; }
    if (ok && r.id < 0) { addError(c.rep, ln, "request id must not be negative"); ok = false; }
    if (ok && tm && ur && du) c.sc.requests.push_back(r);
}

void parseEvent(Ctx& c, const Vector<String>& t, int ln) {
    if (t.size() < 3) { addError(c.rep, ln, "event record needs: time TYPE argument"); return; }
    EventRec e;
    e.line = ln;
    bool ok = intField(c, ln, t[0], "event time", e.time);
    if (ok && e.time < 0) { addError(c.rep, ln, "event time must not be negative"); ok = false; }
    e.type = lookupName(t[1].c_str(), EVENT_NAMES, 7);
    if (e.type < 0) {
        String m;
        m.append("unknown event type '").append(t[1]).append("'");
        addError(c.rep, ln, m);
        return;
    }
    int expected = (e.type == EV_SET_URGENCY) ? 4 : 3;
    if (t.size() != expected) {
        String m;
        m.append("event ").append(t[1]).append(" needs ").appendInt(expected - 2).append(" argument(s)");
        addError(c.rep, ln, m);
        return;
    }
    ok = intField(c, ln, t[2], "event target id", e.target) && ok;
    if (e.type == EV_SET_URGENCY) {
        bool uv = intField(c, ln, t[3], "new urgency", e.value);
        if (uv && (e.value < 1 || e.value > 5)) { addError(c.rep, ln, "urgency must be between 1 and 5"); uv = false; }
        ok = ok && uv;
    }
    if (ok) c.sc.events.push_back(e);
}

// ------------------------------------------------------------ sections
void skipToKeyword(Ctx& c) {
    while (c.pos < c.lines.size()) {
        Vector<String> t;
        tokenize(c.lines[c.pos].text, t);
        if (t.size() > 0 && isKeyword(t[0])) return;
        c.pos++;
    }
}

int sectionIndex(const String& kw) {
    if (kw == "LOCATIONS") return SEC_LOC;
    if (kw == "CONNECTIONS") return SEC_CON;
    if (kw == "ROBOTS") return SEC_ROB;
    if (kw == "REQUESTS") return SEC_REQ;
    return SEC_EVT;
}

void parseSection(Ctx& c, const Vector<String>& head) {
    int hln = c.lines[c.pos].no;
    const String& kw = head[0];
    int sec = sectionIndex(kw);
    int n = 0;
    if (head.size() != 2 || !parseInt(head[1], n) || n < 0) {
        String m;
        m.append(kw).append(" header needs a non-negative count: ").append(kw).append(" <count>");
        addError(c.rep, hln, m);
        c.pos++;
        skipToKeyword(c);
        return;
    }
    if (c.seen[sec]) {
        String m;
        m.append("section ").append(kw).append(" appears more than once");
        addError(c.rep, hln, m);
    }
    c.seen[sec] = true;
    c.pos++;

    Vector<String> t;
    int i = 0;
    for (; i < n; i++) {
        if (c.pos >= c.lines.size()) break;
        tokenize(c.lines[c.pos].text, t);
        if (t.size() > 0 && isKeyword(t[0])) break;
        int ln = c.lines[c.pos].no;
        if (sec == SEC_LOC) parseLocation(c, t, ln);
        else if (sec == SEC_CON) parseConnection(c, t, ln);
        else if (sec == SEC_ROB) parseRobot(c, t, ln);
        else if (sec == SEC_REQ) parseRequest(c, t, ln);
        else parseEvent(c, t, ln);
        c.pos++;
    }
    if (i < n) {
        String m;
        m.append("section ").append(kw).append(" declares ").appendInt(n)
         .append(" records but only ").appendInt(i).append(" found");
        addError(c.rep, hln, m);
        return;
    }
    if (c.pos < c.lines.size()) {
        tokenize(c.lines[c.pos].text, t);
        if (t.size() > 0 && !isKeyword(t[0])) {
            String m;
            m.append("section ").append(kw).append(" has more records than its declared count ")
             .appendInt(n);
            addError(c.rep, c.lines[c.pos].no, m);
            skipToKeyword(c);
        }
    }
}

// ------------------------------------------------------------ cross checks
void duplicateError(LoadReport& rep, int line, const char* what, int id, int firstLine) {
    String m;
    m.append("duplicate ").append(what).append(" id ").appendInt(id)
     .append(" (first defined on line ").appendInt(firstLine).append(")");
    addError(rep, line, m);
}

struct ReqByTime {
    bool operator()(const ReqRec& a, const ReqRec& b) const { return a.time < b.time; }
};
struct EventByTime {
    bool operator()(const EventRec& a, const EventRec& b) const { return a.time < b.time; }
};

void validate(Ctx& c) {
    Scenario& sc = c.sc;
    LoadReport& rep = c.rep;
    HashMap locMap, connMap, robMap, reqMap;   // id -> index in the record vector

    if (!c.seen[SEC_LOC]) addError(rep, 0, "missing LOCATIONS section");
    else if (sc.locations.size() == 0) addError(rep, 0, "LOCATIONS section has no valid records");
    if (!c.seen[SEC_CON]) addError(rep, 0, "missing CONNECTIONS section");
    if (!c.seen[SEC_ROB]) addError(rep, 0, "missing ROBOTS section");
    else if (sc.robots.size() == 0) addError(rep, 0, "ROBOTS section has no valid records");

    for (int i = 0; i < sc.locations.size(); i++) {
        const LocRec& r = sc.locations[i];
        int first;
        if (locMap.get(r.id, first)) duplicateError(rep, r.line, "location", r.id, sc.locations[first].line);
        else locMap.put(r.id, i);
    }
    for (int i = 0; i < sc.connections.size(); i++) {
        const ConnRec& r = sc.connections[i];
        int first;
        if (connMap.get(r.id, first)) duplicateError(rep, r.line, "connection", r.id, sc.connections[first].line);
        else connMap.put(r.id, i);
        if (!locMap.contains(r.a)) addIdError(rep, r.line, "connection", r.id, "unknown location in location_a");
        if (!locMap.contains(r.b)) addIdError(rep, r.line, "connection", r.id, "unknown location in location_b");
        if (r.a == r.b) addIdError(rep, r.line, "connection", r.id, "connects a location to itself");
    }
    for (int i = 0; i < sc.robots.size(); i++) {
        const RobotRec& r = sc.robots[i];
        int first;
        if (robMap.get(r.id, first)) duplicateError(rep, r.line, "robot", r.id, sc.robots[first].line);
        else robMap.put(r.id, i);
        if (!locMap.contains(r.start)) addIdError(rep, r.line, "robot", r.id, "unknown start location");
    }
    for (int i = 0; i < sc.requests.size(); i++) {
        const ReqRec& r = sc.requests[i];
        int first;
        if (reqMap.get(r.id, first)) duplicateError(rep, r.line, "request", r.id, sc.requests[first].line);
        else reqMap.put(r.id, i);
        if (!locMap.contains(r.location)) addIdError(rep, r.line, "request", r.id, "unknown location");
    }
    for (int i = 0; i < sc.events.size(); i++) {
        const EventRec& e = sc.events[i];
        int idx;
        if (e.type == EV_BLOCK || e.type == EV_UNBLOCK) {
            if (!connMap.contains(e.target)) addIdError(rep, e.line, "event", e.target, "unknown connection");
        } else if (e.type == EV_STATION_DOWN || e.type == EV_STATION_UP) {
            if (!locMap.get(e.target, idx)) addIdError(rep, e.line, "event", e.target, "unknown location");
            else if (sc.locations[idx].type != LT_CHARGING)
                addIdError(rep, e.line, "event", e.target, "location is not a charging station");
        } else if (e.type == EV_SET_URGENCY) {
            if (!reqMap.contains(e.target)) addIdError(rep, e.line, "event", e.target, "unknown request");
        } else {
            if (!robMap.contains(e.target)) addIdError(rep, e.line, "event", e.target, "unknown robot");
        }
    }

    if (!sc.params.maxStepsGiven) addWarning(rep, 0, String("PARAM max_steps missing, using default 250"));

    // time order: stable sort, with a warning, only if the file was out of order
    bool ordered = true;
    for (int i = 1; i < sc.requests.size(); i++)
        if (sc.requests[i].time < sc.requests[i - 1].time) { ordered = false; break; }
    if (!ordered) {
        mergeSort(sc.requests, ReqByTime());
        addWarning(rep, 0, String("requests were not in time order, stable-sorted by report time"));
    }
    ordered = true;
    for (int i = 1; i < sc.events.size(); i++)
        if (sc.events[i].time < sc.events[i - 1].time) { ordered = false; break; }
    if (!ordered) {
        mergeSort(sc.events, EventByTime());
        addWarning(rep, 0, String("events were not in time order, stable-sorted by time"));
    }
}

}  // namespace

bool loadScenario(const char* path, Scenario& sc, LoadReport& report) {
    sc = Scenario();
    report.errors.clear();
    report.warnings.clear();
    Ctx c(sc, report);
    if (!readLines(path, c)) return false;

    bool sawEnd = false;
    Vector<String> t;
    while (c.pos < c.lines.size()) {
        tokenize(c.lines[c.pos].text, t);
        int ln = c.lines[c.pos].no;
        const String& kw = t[0];
        if (kw == "SCENARIO") {
            if (t.size() >= 2) sc.name = t[1];
            else addError(report, ln, "SCENARIO needs a name");
            c.pos++;
        } else if (kw == "PARAM") {
            parseParam(c, t, ln);
            c.pos++;
        } else if (kw == "END") {
            sawEnd = true;
            c.pos++;
            if (c.pos < c.lines.size())
                addWarning(report, c.lines[c.pos].no, String("text after END is ignored"));
            break;
        } else if (isKeyword(kw)) {
            parseSection(c, t);
        } else {
            String m;
            m.append("unexpected text '").append(kw).append("', expected a section keyword");
            addError(report, ln, m);
            c.pos++;
        }
    }
    if (!sawEnd) addError(report, 0, "missing END line");

    validate(c);
    return report.errors.size() == 0;
}
