// Loader tests. Build and run from the project root:
//   g++ -std=c++11 -Wall -Wextra -O2 -Isrc tests/test_loader.cpp src/scenario_loader.cpp -o test_loader && ./test_loader
// Optional argument: folder with the scenario files (default "data").
#include "scenario_loader.h"
using namespace ds;

static int g_checks = 0, g_failed = 0;
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failed++; \
    printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static bool hasMessage(const Vector<String>& list, const char* part) {
    for (int i = 0; i < list.size(); i++)
        if (strstr(list[i].c_str(), part)) return true;
    return false;
}

static void writeFile(const char* path, const String& text) {
    FILE* f = fopen(path, "wb");
    fwrite(text.c_str(), 1, text.length(), f);
    fclose(f);
}

static String replaceOnce(const String& base, const char* from, const char* to) {
    const char* s = base.c_str();
    const char* p = strstr(s, from);
    if (!p) { printf("TEST BUG: '%s' not found in base\n", from); return base; }
    String out;
    for (const char* q = s; q < p; q++) out.append(*q);
    out.append(to);
    out.append(p + strlen(from));
    return out;
}

// ---------------------------------------------------------------- real files
struct Expect { const char* file; int loc, conn, blocked, robots, reqs, events, maxSteps; };

static void testRealFiles(const char* dir) {
    Expect ex[] = {
        { "scenario_1_easy.txt",    14,  22,  2,  5,  12,  6,  80 },
        { "scenario_2_medium.txt",  30,  60,  4,  8,  30, 15, 150 },
        { "scenario_provided.txt",  56, 117,  7, 12,  60, 20, 250 },
        { "scenario_3_hard.txt",    60, 125, 10, 14,  80, 39, 300 },
        { "scenario_4_stress.txt", 130, 280, 22, 26, 140, 79, 520 },
    };
    for (int i = 0; i < 5; i++) {
        String path;
        path.append(dir).append('/').append(ex[i].file);
        Scenario sc;
        LoadReport rep;
        bool ok = loadScenario(path.c_str(), sc, rep);
        if (!ok) for (int k = 0; k < rep.errors.size(); k++) printf("  %s: %s\n", ex[i].file, rep.errors[k].c_str());
        CHECK(ok);
        CHECK(sc.locations.size() == ex[i].loc);
        CHECK(sc.connections.size() == ex[i].conn);
        CHECK(sc.blockedCount() == ex[i].blocked);
        CHECK(sc.robots.size() == ex[i].robots);
        CHECK(sc.requests.size() == ex[i].reqs);
        CHECK(sc.events.size() == ex[i].events);
        CHECK(sc.params.maxSteps == ex[i].maxSteps);
        CHECK(rep.warnings.size() == 0);
        printf("%-24s ok=%d  V=%d E=%d (blocked %d) R=%d Q=%d Ev=%d steps=%d\n", ex[i].file, ok,
               sc.locations.size(), sc.connections.size(), sc.blockedCount(), sc.robots.size(),
               sc.requests.size(), sc.events.size(), sc.params.maxSteps);
    }
    // spot checks against the text of scenario_provided.txt
    String path;
    path.append(dir).append("/scenario_provided.txt");
    Scenario sc;
    LoadReport rep;
    loadScenario(path.c_str(), sc, rep);
    CHECK(sc.locations[5].id == 6 && sc.locations[5].type == LT_CHARGING);
    CHECK(sc.connections[10].id == 11 && sc.connections[10].blocked);
    CHECK(sc.robots[11].capability == CAP_GENERAL && sc.robots[11].initBattery == 88);
    CHECK(sc.requests[1].urgency == 5 && sc.requests[1].category == CAT_SEARCH);
    CHECK(sc.events[2].type == EV_STATION_DOWN && sc.events[2].target == 19);
    CHECK(sc.events[6].type == EV_SET_URGENCY && sc.events[6].value == 5);
}

// ---------------------------------------------------------------- bad input
static const char* BASE =
    "SCENARIO t\n"
    "PARAM max_steps 10\n"
    "LOCATIONS 3\n"
    "1 A JUNCTION\n"
    "2 B CHARGING\n"
    "3 C BUILDING\n"
    "CONNECTIONS 2\n"
    "1 1 2 3 OPEN\n"
    "2 2 3 4 BLOCKED\n"
    "ROBOTS 1\n"
    "1 1 GENERAL 100 100\n"
    "REQUESTS 1\n"
    "1 0 3 FIRE 3 2\n"
    "EVENTS 2\n"
    "5 BLOCK 1\n"
    "6 STATION_DOWN 2\n"
    "END\n";

static void expectFail(const char* from, const char* to, const char* message) {
    String text = replaceOnce(String(BASE), from, to);
    writeFile("loader_test.tmp", text);
    Scenario sc;
    LoadReport rep;
    bool ok = loadScenario("loader_test.tmp", sc, rep);
    CHECK(!ok);
    bool found = hasMessage(rep.errors, message);
    if (!found) {
        printf("expected message containing '%s' for change '%s' -> '%s'; got:\n", message, from, to);
        for (int i = 0; i < rep.errors.size(); i++) printf("   %s\n", rep.errors[i].c_str());
    }
    CHECK(found);
}

static void testBadInput() {
    // the base file itself must be valid
    writeFile("loader_test.tmp", String(BASE));
    Scenario sc;
    LoadReport rep;
    CHECK(loadScenario("loader_test.tmp", sc, rep));
    CHECK(rep.errors.size() == 0 && rep.warnings.size() == 0);

    expectFail("LOCATIONS 3", "LOCATIONS 4", "declares 4 records");
    expectFail("LOCATIONS 3", "LOCATIONS 2", "more records than its declared count");
    expectFail("CONNECTIONS 2", "CONNECTIONS 3", "declares 3 records but only 2 found");
    expectFail("3 C BUILDING", "2 C BUILDING", "duplicate location id 2");
    expectFail("2 2 3 4 BLOCKED", "1 2 3 4 BLOCKED", "duplicate connection id 1");
    expectFail("END\n", "", "missing END");
    expectFail("1 1 2 3 OPEN", "1 1 9 3 OPEN", "unknown location");
    expectFail("1 1 2 3 OPEN", "1 1 1 3 OPEN", "connects a location to itself");
    expectFail("1 1 2 3 OPEN", "1 1 2 0 OPEN", "cost must be greater than 0");
    expectFail("1 1 2 3 OPEN", "1 1 2 3 SHUT", "unknown connection status");
    expectFail("1 0 3 FIRE 3 2", "1 0 3 FIRE 6 2", "urgency must be between 1 and 5");
    expectFail("1 0 3 FIRE 3 2", "1 0 9 FIRE 3 2", "unknown location");
    expectFail("1 0 3 FIRE 3 2", "1 0 3 RESCUE 3 2", "unknown category");
    expectFail("1 1 GENERAL 100 100", "1 1 GENERAL 100 120", "initial battery is above max battery");
    expectFail("1 1 GENERAL 100 100", "1 1 GADGET 100 100", "unknown capability");
    expectFail("1 1 GENERAL 100 100", "1 7 GENERAL 100 100", "unknown start location");
    expectFail("5 BLOCK 1", "5 BLOCK 77", "unknown connection");
    expectFail("6 STATION_DOWN 2", "6 STATION_DOWN 3", "not a charging station");
    expectFail("6 STATION_DOWN 2", "6 STATION_DOWN 9", "unknown location");
    expectFail("5 BLOCK 1", "5 SET_URGENCY 99 3", "unknown request");
    expectFail("5 BLOCK 1", "5 ROBOT_OFFLINE 4", "unknown robot");
    expectFail("5 BLOCK 1", "five BLOCK 1", "not a valid number");
    expectFail("5 BLOCK 1", "5 EXPLODE 1", "unknown event type");
    expectFail("5 BLOCK 1", "5 SET_URGENCY 1", "needs 2 argument");
    expectFail("PARAM max_steps 10", "PARAM max_steps -4", "max_steps must be between");
    expectFail("PARAM max_steps 10", "PARAM max_steps ten", "not a valid number");
    expectFail("ROBOTS 1\n1 1 GENERAL 100 100\n", "", "missing ROBOTS section");
    expectFail("1 A JUNCTION\n", "1 A JUNCTION\nstray words here\n", "has more records than its declared count");

    // a file with several problems reports all of them, not just the first
    String multi = replaceOnce(String(BASE), "1 1 2 3 OPEN", "1 1 9 3 OPEN");
    multi = replaceOnce(multi, "1 0 3 FIRE 3 2", "1 0 3 FIRE 9 2");
    writeFile("loader_test.tmp", multi);
    CHECK(!loadScenario("loader_test.tmp", sc, rep));
    CHECK(rep.errors.size() >= 2);

    // out-of-order events are sorted, with a warning, and stay stable
    String swapped = replaceOnce(String(BASE), "5 BLOCK 1\n6 STATION_DOWN 2", "6 STATION_DOWN 2\n5 BLOCK 1");
    writeFile("loader_test.tmp", swapped);
    CHECK(loadScenario("loader_test.tmp", sc, rep));
    CHECK(rep.warnings.size() == 1);
    CHECK(sc.events[0].time == 5 && sc.events[1].time == 6);

    // events at the same time keep their file order
    String same = replaceOnce(String(BASE), "6 STATION_DOWN 2", "5 STATION_DOWN 2");
    writeFile("loader_test.tmp", same);
    CHECK(loadScenario("loader_test.tmp", sc, rep));
    CHECK(sc.events[0].type == EV_BLOCK && sc.events[1].type == EV_STATION_DOWN);

    // comments, blank lines, tabs and Windows line endings are accepted
    String crlf;
    for (const char* p = BASE; *p; p++) { if (*p == '\n') crlf.append('\r'); crlf.append(*p); }
    crlf = replaceOnce(crlf, "ROBOTS 1", "# a comment\r\n\r\nROBOTS\t1");
    writeFile("loader_test.tmp", crlf);
    CHECK(loadScenario("loader_test.tmp", sc, rep));
    CHECK(sc.robots.size() == 1);

    // optional PARAM keys: defaults when absent, values when present, warning when unknown
    CHECK(loadScenario("loader_test.tmp", sc, rep));
    CHECK(sc.params.requestTimeout == 150 && sc.params.rerouteWait == 5 && sc.params.lowBatteryPct == 40);
    String ext = replaceOnce(String(BASE), "PARAM max_steps 10\n",
                             "PARAM max_steps 10\nPARAM request_timeout 99\nPARAM reroute_wait 2\n"
                             "PARAM low_battery_pct 25\nPARAM colour red\n");
    writeFile("loader_test.tmp", ext);
    CHECK(loadScenario("loader_test.tmp", sc, rep));
    CHECK(sc.params.requestTimeout == 99 && sc.params.rerouteWait == 2 && sc.params.lowBatteryPct == 25);
    CHECK(hasMessage(rep.warnings, "unknown PARAM key"));

    // file problems: missing file, empty file, junk
    CHECK(!loadScenario("no_such_file.txt", sc, rep));
    CHECK(hasMessage(rep.errors, "cannot open file"));
    writeFile("loader_test.tmp", String(""));
    CHECK(!loadScenario("loader_test.tmp", sc, rep));
    CHECK(hasMessage(rep.errors, "missing LOCATIONS section"));
    writeFile("loader_test.tmp", String("this is not a scenario\n12345\nLOCATIONS\nROBOTS x\n\x01\x02 zzz\n"));
    CHECK(!loadScenario("loader_test.tmp", sc, rep));
    CHECK(rep.errors.size() > 0);

    remove("loader_test.tmp");
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : "data";
    testRealFiles(dir);
    testBadInput();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
