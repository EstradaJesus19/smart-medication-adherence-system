#include <MedicationSystem.h>
#include <Simulation.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <initializer_list>

#ifdef STANDALONE_TEST
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
#define RUN_TEST(fn) do { fn(); std::puts(#fn " PASS"); } while (0)
#else
#include <unity.h>
#define CHECK(expr) TEST_ASSERT_TRUE(expr)
void setUp() {}
void tearDown() {}
#endif

using namespace MedicationSystem;
namespace {
char output[32768]{};
void capture(const char* row) {
    CHECK(std::strlen(output) + std::strlen(row) + 2 < sizeof(output));
    std::strcat(output, row);
    std::strcat(output, "\n");
}
void reset() {
    output[0] = '\0';
    Simulation::begin(capture);
}
unsigned count(const char* event) {
    char needle[80];
    std::snprintf(needle, sizeof(needle), ",%s,", event);
    unsigned found = 0;
    const char* cursor = output;
    while ((cursor = std::strstr(cursor, needle))) {
        ++found;
        cursor += std::strlen(needle);
    }
    return found;
}
const CompartmentStatus& lid(int c) {
    CHECK(getCompartment(c));
    return *getCompartment(c);
}
bool start(int first, int second = 0, uint32_t now = 0, uint32_t scheduled = 100) {
    bool due[COMPARTMENT_COUNT]{};
    due[first - 1] = true;
    if (second) due[second - 1] = true;
    return startSession(due, scheduled, now);
}
void send(const char* text) {
    while (*text) Simulation::feed(*text++);
}
void confirm(int c, uint32_t openTime = 0) {
    lidChanged(c, true, openTime);
    update(openTime + 2000);
}
}

void test_correct_access_waits_for_closure() {
    reset();
    CHECK(start(2));
    confirm(2);
    CHECK(lid(2).latestDose.outcome == DoseOutcome::ACCESS_CONFIRMED);
    CHECK(getState() == SystemState::WAITING_FOR_CLOSURE);
    CHECK(!lid(2).inCurrentSession);
    lidChanged(2, false, 2001);
    CHECK(getState() == SystemState::IDLE);
    CHECK(count("ACCESS_CONFIRMED") == 1);
}
void test_wrong_lid_must_also_close() {
    reset();
    start(2);
    lidChanged(3, true, 0);
    confirm(2);
    CHECK(count("WRONG_COMPARTMENT") == 1);
    lidChanged(2, false, 2001);
    CHECK(getState() == SystemState::WAITING_FOR_CLOSURE);
    lidChanged(3, false, 2002);
    CHECK(getState() == SystemState::IDLE);
}
void test_two_compartments_either_order() {
    for (int first : {1, 3}) {
        reset();
        const int second = first == 1 ? 3 : 1;
        start(1, 3);
        confirm(first);
        CHECK(getState() == SystemState::REMINDER);
        CHECK(lid(first).latestDose.outcome == DoseOutcome::ACCESS_CONFIRMED);
        CHECK(lid(second).latestDose.outcome == DoseOutcome::PENDING);
        lidChanged(first, false, 2001);
        confirm(second, 3000);
        CHECK(count("ACCESS_CONFIRMED") == 2);
        lidChanged(second, false, 5001);
        CHECK(getState() == SystemState::IDLE);
    }
}
void test_simultaneous_openings_independent_timers() {
    reset();
    start(1, 3);
    lidChanged(3, true, 100);
    lidChanged(1, true, 100);
    update(2099);
    CHECK(count("ACCESS_CONFIRMED") == 0);
    update(2100);
    CHECK(count("ACCESS_CONFIRMED") == 2);
    CHECK(getState() == SystemState::WAITING_FOR_CLOSURE);
}
void test_brief_opening_does_not_affect_other_timer() {
    reset();
    start(1, 3);
    lidChanged(1, true, 0);
    lidChanged(3, true, 500);
    lidChanged(1, false, 1000);
    lidChanged(1, true, 1500);
    update(2500);
    CHECK(lid(3).latestDose.outcome == DoseOutcome::ACCESS_CONFIRMED);
    CHECK(lid(1).latestDose.outcome == DoseOutcome::PENDING);
    update(3500);
    CHECK(count("ACCESS_CONFIRMED") == 2);
    CHECK(count("BRIEF_OPENING") == 1);
}
void test_partial_confirmation_then_timeout() {
    reset();
    start(1, 3);
    confirm(1);
    update(60000);
    CHECK(lid(1).latestDose.outcome == DoseOutcome::ACCESS_CONFIRMED);
    CHECK(lid(3).latestDose.outcome == DoseOutcome::MISSED);
    CHECK(count("DOSE_MISSED") == 1);
    CHECK(getState() == SystemState::WAITING_FOR_CLOSURE);
    update(70000);
    CHECK(count("DOSE_MISSED") == 1);
}
void test_no_interaction_misses_and_returns_idle() {
    reset();
    start(1, 3);
    update(60000);
    CHECK(count("DOSE_MISSED") == 2);
    CHECK(getState() == SystemState::IDLE);
    CHECK(count("SESSION_FINISHED") == 1);
}
void test_deadline_wins_and_brief_attempts_do_not_extend_it() {
    reset();
    start(2);
    lidChanged(2, true, 100);
    lidChanged(2, false, 200);
    lidChanged(2, true, 58000);
    update(60000);
    CHECK(count("ACCESS_CONFIRMED") == 0);
    CHECK(count("DOSE_MISSED") == 1);
    CHECK(getState() == SystemState::WAITING_FOR_CLOSURE);
}
void test_close_at_exact_dwell_confirms() {
    reset();
    start(2);
    lidChanged(2, true, 0);
    lidChanged(2, false, 2000);
    CHECK(count("ACCESS_CONFIRMED") == 1);
    CHECK(count("BRIEF_OPENING") == 0);
    CHECK(getState() == SystemState::IDLE);
}
void test_duplicate_open_is_not_repeated_access() {
    reset();
    start(2);
    confirm(2);
    lidChanged(2, true, 2001);
    CHECK(count("LID_OPEN") == 1);
    CHECK(count("REPEATED_ACCESS") == 0);
    lidChanged(2, false, 2002);
    lidChanged(2, true, 2003);
    CHECK(count("REPEATED_ACCESS") == 1);
    CHECK(count("ACCESS_CONFIRMED") == 1);
}
void test_repeat_during_pending_session_has_warning_priority() {
    reset();
    start(1, 3);
    confirm(1);
    lidChanged(1, false, 2001);
    lidChanged(1, true, 2002);
    CHECK(count("REPEATED_ACCESS") == 1);
    CHECK(count("WRONG_COMPARTMENT") == 0);
    CHECK(getState() == SystemState::REMINDER);
    CHECK(lid(3).latestDose.outcome == DoseOutcome::PENDING);
}
void test_repeat_while_other_lid_waits_for_closure() {
    reset();
    start(2);
    lidChanged(3, true, 0);
    confirm(2);
    lidChanged(2, false, 2001);
    lidChanged(2, true, 2002);
    CHECK(count("REPEATED_ACCESS") == 1);
    CHECK(getState() == SystemState::WAITING_FOR_CLOSURE);
}
void test_next_dose_replaces_only_its_own_history() {
    reset();
    start(1, 3, 0, 100);
    lidChanged(1, true, 0);
    lidChanged(3, true, 0);
    update(2000);
    lidChanged(1, false, 2001);
    lidChanged(3, false, 2001);
    CHECK(start(1, 0, 3000, 200));
    CHECK(lid(1).latestDose.scheduledAt == 200);
    CHECK(lid(1).latestDose.outcome == DoseOutcome::PENDING);
    CHECK(lid(3).latestDose.scheduledAt == 100);
    CHECK(lid(3).latestDose.outcome == DoseOutcome::ACCESS_CONFIRMED);
    confirm(1, 3000);
    CHECK(count("ACCESS_CONFIRMED") == 3);
    CHECK(count("REPEATED_ACCESS") == 0);
}
void test_missed_dose_cannot_be_confirmed_late() {
    reset();
    start(2);
    update(60000);
    lidChanged(2, true, 60001);
    update(63000);
    CHECK(count("UNEXPECTED_ACCESS") == 1);
    CHECK(count("REPEATED_ACCESS") == 0);
    CHECK(lid(2).latestDose.outcome == DoseOutcome::MISSED);
}
void test_startup_and_idle_openings_request_closure() {
    reset();
    const bool initial[4] = {false, false, true, false};
    CHECK(initialize(initial, Settings{}));
    CHECK(getState() == SystemState::WAITING_FOR_CLOSURE);
    CHECK(count("LID_OPEN") == 0);
    lidChanged(3, false, 0);
    CHECK(getState() == SystemState::IDLE);
    lidChanged(4, true, 1);
    CHECK(count("UNEXPECTED_ACCESS") == 1);
    CHECK(getState() == SystemState::WAITING_FOR_CLOSURE);
}
void test_already_open_target_requires_reopening() {
    reset();
    lidChanged(2, true, 0);
    CHECK(start(2));
    update(3000);
    CHECK(!lid(2).verifying);
    CHECK(count("ACCESS_CONFIRMED") == 0);
    lidChanged(2, false, 3001);
    confirm(2, 3002);
    CHECK(count("ACCESS_CONFIRMED") == 1);
}
void test_refill_explicit_finish_and_closed_lids_required() {
    reset();
    CHECK(startRefill());
    lidChanged(1, true, 0);
    CHECK(!finishRefill());
    CHECK(getState() == SystemState::REFILL);
    lidChanged(1, false, 100);
    CHECK(getState() == SystemState::REFILL);
    CHECK(finishRefill());
    CHECK(getState() == SystemState::IDLE);
    CHECK(count("UNEXPECTED_ACCESS") == 0);
    CHECK(!finishRefill());
}
void test_refill_suppresses_warnings_but_preserves_history() {
    reset();
    start(2);
    confirm(2);
    lidChanged(2, false, 2001);
    CHECK(startRefill());
    lidChanged(2, true, 2002);
    lidChanged(2, false, 2003);
    CHECK(count("REPEATED_ACCESS") == 0);
    CHECK(lid(2).latestDose.outcome == DoseOutcome::ACCESS_CONFIRMED);
    CHECK(finishRefill());
    lidChanged(2, true, 2004);
    CHECK(count("REPEATED_ACCESS") == 1);
}
void test_due_session_interrupts_refill_with_open_target() {
    reset();
    startRefill();
    lidChanged(2, true, 0);
    CHECK(start(2, 4, 100));
    CHECK(count("REFILL_INTERRUPTED") == 1);
    CHECK(!lid(2).verifying);
    CHECK(!finishRefill());
    lidChanged(2, false, 101);
    confirm(2, 102);
    CHECK(lid(4).latestDose.outcome == DoseOutcome::PENDING);
}
void test_refill_rejected_during_reminder_and_closure() {
    reset();
    start(2);
    CHECK(!startRefill());
    confirm(2);
    CHECK(!startRefill());
    CHECK(count("REFILL_START_REJECTED") == 2);
}
void test_session_validation_and_conflict() {
    reset();
    const bool none[4]{};
    CHECK(!startSession(none, 100, 0));
    CHECK(!startSession(nullptr, 100, 0));
    CHECK(start(1));
    CHECK(!start(3, 0, 100, 200));
    CHECK(lid(1).inCurrentSession);
    CHECK(!lid(3).hasDose);
    CHECK(count("SESSION_CONFLICT") == 1);
    // After the old deadline, its outcome is processed before a new session.
    CHECK(start(3, 0, 60000, 200));
    CHECK(lid(1).latestDose.outcome == DoseOutcome::MISSED);
}
void test_invalid_settings_and_lid_numbers_do_not_corrupt_state() {
    reset();
    start(2);
    const bool closed[4]{};
    CHECK(!initialize(closed, {60000, 0}));
    CHECK(!initialize(closed, {2000, 2000}));
    CHECK(!initialize(closed, {86400001, 2000}));
    lidChanged(0, true, 0);
    lidChanged(5, true, 0);
    CHECK(!getCompartment(0));
    CHECK(getState() == SystemState::REMINDER);
    CHECK(lid(2).inCurrentSession);
}
void test_elapsed_timers_across_rollover() {
    reset();
    const uint32_t begin = UINT32_MAX - 1000;
    CHECK(start(2, 0, begin));
    confirm(2, begin);
    CHECK(count("ACCESS_CONFIRMED") == 1);
    reset();
    CHECK(start(2, 0, begin));
    update(begin + uint32_t{60000});
    CHECK(count("DOSE_MISSED") == 1);
}
void test_serial_workflow_status_and_csv_time() {
    reset();
    send("dose 1 3\r\nopen 3\nopen 1\nadvance 2000\nstatus\n");
    CHECK(count("ACCESS_CONFIRMED") == 2);
    CHECK(std::strstr(output, "2000,ACCESS_CONFIRMED,1,0,simulated,lid_only"));
    CHECK(std::strstr(output, "state=WAITING_FOR_CLOSURE"));
    send("close 1\nclose 3\nrefill start\nopen 2\nclose 2\nrefill finish\n");
    CHECK(getState() == SystemState::IDLE);
    CHECK(count("REFILL_FINISHED") == 1);
}
void test_serial_input_rejection_is_atomic_and_parser_recovers() {
    reset();
    send("dose 1 5\ndose 2 2\ndose 1 2 3 4 1\ndose -1\nopen 0\n");
    send("advance 999999999999999999999\nadvance 600001\nopen 1 extra\n");
    CHECK(getState() == SystemState::IDLE);
    CHECK(count("DOSE_STARTED") == 0);
    CHECK(count("LID_OPEN") == 0);
    for (int i = 0; i < 100; ++i) Simulation::feed('x');
    send("\ndose 2\nstatus\n");
    CHECK(std::strstr(output, "line too long; discarded"));
    CHECK(std::strstr(output, "time_ms=0 state=REMINDER"));
    CHECK(count("DOSE_STARTED") == 1);
}
void test_long_advance_does_not_skip_confirmation() {
    reset();
    send("dose 1 3\nopen 1\nadvance 60000\n");
    CHECK(std::strstr(output, "2000,ACCESS_CONFIRMED,1"));
    CHECK(std::strstr(output, "60000,DOSE_MISSED,3"));
    CHECK(count("DOSE_MISSED") == 1);
}

int main() {
#ifndef STANDALONE_TEST
    UNITY_BEGIN();
#endif
    RUN_TEST(test_correct_access_waits_for_closure);
    RUN_TEST(test_wrong_lid_must_also_close);
    RUN_TEST(test_two_compartments_either_order);
    RUN_TEST(test_simultaneous_openings_independent_timers);
    RUN_TEST(test_brief_opening_does_not_affect_other_timer);
    RUN_TEST(test_partial_confirmation_then_timeout);
    RUN_TEST(test_no_interaction_misses_and_returns_idle);
    RUN_TEST(test_deadline_wins_and_brief_attempts_do_not_extend_it);
    RUN_TEST(test_close_at_exact_dwell_confirms);
    RUN_TEST(test_duplicate_open_is_not_repeated_access);
    RUN_TEST(test_repeat_during_pending_session_has_warning_priority);
    RUN_TEST(test_repeat_while_other_lid_waits_for_closure);
    RUN_TEST(test_next_dose_replaces_only_its_own_history);
    RUN_TEST(test_missed_dose_cannot_be_confirmed_late);
    RUN_TEST(test_startup_and_idle_openings_request_closure);
    RUN_TEST(test_already_open_target_requires_reopening);
    RUN_TEST(test_refill_explicit_finish_and_closed_lids_required);
    RUN_TEST(test_refill_suppresses_warnings_but_preserves_history);
    RUN_TEST(test_due_session_interrupts_refill_with_open_target);
    RUN_TEST(test_refill_rejected_during_reminder_and_closure);
    RUN_TEST(test_session_validation_and_conflict);
    RUN_TEST(test_invalid_settings_and_lid_numbers_do_not_corrupt_state);
    RUN_TEST(test_elapsed_timers_across_rollover);
    RUN_TEST(test_serial_workflow_status_and_csv_time);
    RUN_TEST(test_serial_input_rejection_is_atomic_and_parser_recovers);
    RUN_TEST(test_long_advance_does_not_skip_confirmation);
#ifndef STANDALONE_TEST
    return UNITY_END();
#else
    return 0;
#endif
}
