/**
 * Unit tests for Assistant State Machine
 * Tests state transitions and event handling logic
 */

#include <unity.h>

#ifdef NATIVE_BUILD
#include "../mocks/Arduino.h"
#endif

// ============================================================================
// State definitions (from main.cpp)
// ============================================================================

enum class AssistantState {
    INIT,
    CONNECTING_WIFI,
    IDLE,
    LISTENING,
    PROCESSING,
    RESPONDING,
    ERROR
};

// ============================================================================
// Mock Event Types
// ============================================================================

enum class Event {
    WIFI_CONNECTED,
    WIFI_FAILED,
    BOOT_BUTTON_PRESSED,
    WAKE_WORD_DETECTED,
    SILENCE_DETECTED,
    PROCESSING_COMPLETE,
    PLAYBACK_COMPLETE,
    ERROR_OCCURRED,
    ERROR_TIMEOUT
};

// ============================================================================
// State Machine Implementation (testable version)
// ============================================================================

class AssistantStateMachine {
public:
    AssistantState currentState = AssistantState::INIT;
    String lastError = "";
    bool wakeWordEnabled = true;

    AssistantState processEvent(Event event) {
        AssistantState previousState = currentState;

        switch (currentState) {
            case AssistantState::INIT:
                // From INIT, only transition to CONNECTING_WIFI
                currentState = AssistantState::CONNECTING_WIFI;
                break;

            case AssistantState::CONNECTING_WIFI:
                if (event == Event::WIFI_CONNECTED) {
                    currentState = AssistantState::IDLE;
                } else if (event == Event::WIFI_FAILED) {
                    currentState = AssistantState::ERROR;
                    lastError = "WiFi connection failed";
                }
                break;

            case AssistantState::IDLE:
                if (event == Event::BOOT_BUTTON_PRESSED ||
                    (event == Event::WAKE_WORD_DETECTED && wakeWordEnabled)) {
                    currentState = AssistantState::LISTENING;
                }
                break;

            case AssistantState::LISTENING:
                if (event == Event::SILENCE_DETECTED ||
                    event == Event::BOOT_BUTTON_PRESSED) {
                    currentState = AssistantState::PROCESSING;
                } else if (event == Event::ERROR_OCCURRED) {
                    currentState = AssistantState::ERROR;
                }
                break;

            case AssistantState::PROCESSING:
                if (event == Event::PROCESSING_COMPLETE) {
                    currentState = AssistantState::RESPONDING;
                } else if (event == Event::ERROR_OCCURRED) {
                    currentState = AssistantState::ERROR;
                }
                break;

            case AssistantState::RESPONDING:
                if (event == Event::PLAYBACK_COMPLETE) {
                    currentState = AssistantState::IDLE;
                } else if (event == Event::BOOT_BUTTON_PRESSED) {
                    // Interrupt playback
                    currentState = AssistantState::IDLE;
                }
                break;

            case AssistantState::ERROR:
                if (event == Event::ERROR_TIMEOUT) {
                    currentState = AssistantState::IDLE;
                }
                break;
        }

        return currentState;
    }

    bool canStartRecording() const {
        return currentState == AssistantState::IDLE;
    }

    bool canInterrupt() const {
        return currentState == AssistantState::RESPONDING;
    }

    bool isActive() const {
        return currentState != AssistantState::INIT &&
               currentState != AssistantState::ERROR &&
               currentState != AssistantState::CONNECTING_WIFI;
    }
};

// ============================================================================
// State Transition Tests
// ============================================================================

void test_initial_state_is_init() {
    AssistantStateMachine sm;
    TEST_ASSERT_EQUAL(AssistantState::INIT, sm.currentState);
}

void test_init_to_connecting() {
    AssistantStateMachine sm;
    sm.processEvent(Event::WIFI_CONNECTED);  // Any event transitions from INIT
    TEST_ASSERT_EQUAL(AssistantState::CONNECTING_WIFI, sm.currentState);
}

void test_connecting_to_idle_on_success() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::CONNECTING_WIFI;

    sm.processEvent(Event::WIFI_CONNECTED);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);
}

void test_connecting_to_error_on_failure() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::CONNECTING_WIFI;

    sm.processEvent(Event::WIFI_FAILED);
    TEST_ASSERT_EQUAL(AssistantState::ERROR, sm.currentState);
    TEST_ASSERT_TRUE(sm.lastError.indexOf("WiFi") >= 0);
}

void test_idle_to_listening_on_button() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::IDLE;

    sm.processEvent(Event::BOOT_BUTTON_PRESSED);
    TEST_ASSERT_EQUAL(AssistantState::LISTENING, sm.currentState);
}

void test_idle_to_listening_on_wake_word() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::IDLE;
    sm.wakeWordEnabled = true;

    sm.processEvent(Event::WAKE_WORD_DETECTED);
    TEST_ASSERT_EQUAL(AssistantState::LISTENING, sm.currentState);
}

void test_idle_ignores_wake_word_when_disabled() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::IDLE;
    sm.wakeWordEnabled = false;

    sm.processEvent(Event::WAKE_WORD_DETECTED);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);
}

void test_listening_to_processing_on_silence() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::LISTENING;

    sm.processEvent(Event::SILENCE_DETECTED);
    TEST_ASSERT_EQUAL(AssistantState::PROCESSING, sm.currentState);
}

void test_listening_to_processing_on_button() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::LISTENING;

    sm.processEvent(Event::BOOT_BUTTON_PRESSED);
    TEST_ASSERT_EQUAL(AssistantState::PROCESSING, sm.currentState);
}

void test_processing_to_responding() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::PROCESSING;

    sm.processEvent(Event::PROCESSING_COMPLETE);
    TEST_ASSERT_EQUAL(AssistantState::RESPONDING, sm.currentState);
}

void test_processing_to_error() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::PROCESSING;

    sm.processEvent(Event::ERROR_OCCURRED);
    TEST_ASSERT_EQUAL(AssistantState::ERROR, sm.currentState);
}

void test_responding_to_idle_on_complete() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::RESPONDING;

    sm.processEvent(Event::PLAYBACK_COMPLETE);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);
}

void test_responding_interrupt_on_button() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::RESPONDING;

    sm.processEvent(Event::BOOT_BUTTON_PRESSED);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);
}

void test_error_recovery_on_timeout() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::ERROR;

    sm.processEvent(Event::ERROR_TIMEOUT);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);
}

// ============================================================================
// State Query Tests
// ============================================================================

void test_can_start_recording_only_when_idle() {
    AssistantStateMachine sm;

    sm.currentState = AssistantState::INIT;
    TEST_ASSERT_FALSE(sm.canStartRecording());

    sm.currentState = AssistantState::CONNECTING_WIFI;
    TEST_ASSERT_FALSE(sm.canStartRecording());

    sm.currentState = AssistantState::IDLE;
    TEST_ASSERT_TRUE(sm.canStartRecording());

    sm.currentState = AssistantState::LISTENING;
    TEST_ASSERT_FALSE(sm.canStartRecording());

    sm.currentState = AssistantState::PROCESSING;
    TEST_ASSERT_FALSE(sm.canStartRecording());

    sm.currentState = AssistantState::RESPONDING;
    TEST_ASSERT_FALSE(sm.canStartRecording());

    sm.currentState = AssistantState::ERROR;
    TEST_ASSERT_FALSE(sm.canStartRecording());
}

void test_can_interrupt_only_when_responding() {
    AssistantStateMachine sm;

    sm.currentState = AssistantState::IDLE;
    TEST_ASSERT_FALSE(sm.canInterrupt());

    sm.currentState = AssistantState::LISTENING;
    TEST_ASSERT_FALSE(sm.canInterrupt());

    sm.currentState = AssistantState::PROCESSING;
    TEST_ASSERT_FALSE(sm.canInterrupt());

    sm.currentState = AssistantState::RESPONDING;
    TEST_ASSERT_TRUE(sm.canInterrupt());
}

void test_is_active_excludes_init_error_connecting() {
    AssistantStateMachine sm;

    sm.currentState = AssistantState::INIT;
    TEST_ASSERT_FALSE(sm.isActive());

    sm.currentState = AssistantState::CONNECTING_WIFI;
    TEST_ASSERT_FALSE(sm.isActive());

    sm.currentState = AssistantState::ERROR;
    TEST_ASSERT_FALSE(sm.isActive());

    sm.currentState = AssistantState::IDLE;
    TEST_ASSERT_TRUE(sm.isActive());

    sm.currentState = AssistantState::LISTENING;
    TEST_ASSERT_TRUE(sm.isActive());

    sm.currentState = AssistantState::PROCESSING;
    TEST_ASSERT_TRUE(sm.isActive());

    sm.currentState = AssistantState::RESPONDING;
    TEST_ASSERT_TRUE(sm.isActive());
}

// ============================================================================
// Full Flow Integration Tests
// ============================================================================

void test_complete_voice_interaction_flow() {
    AssistantStateMachine sm;

    // Boot sequence
    TEST_ASSERT_EQUAL(AssistantState::INIT, sm.currentState);
    sm.processEvent(Event::WIFI_CONNECTED);  // Triggers INIT -> CONNECTING
    sm.processEvent(Event::WIFI_CONNECTED);  // CONNECTING -> IDLE
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);

    // Voice interaction
    sm.processEvent(Event::BOOT_BUTTON_PRESSED);
    TEST_ASSERT_EQUAL(AssistantState::LISTENING, sm.currentState);

    sm.processEvent(Event::SILENCE_DETECTED);
    TEST_ASSERT_EQUAL(AssistantState::PROCESSING, sm.currentState);

    sm.processEvent(Event::PROCESSING_COMPLETE);
    TEST_ASSERT_EQUAL(AssistantState::RESPONDING, sm.currentState);

    sm.processEvent(Event::PLAYBACK_COMPLETE);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);
}

void test_wake_word_flow() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::IDLE;
    sm.wakeWordEnabled = true;

    // Wake word activation
    sm.processEvent(Event::WAKE_WORD_DETECTED);
    TEST_ASSERT_EQUAL(AssistantState::LISTENING, sm.currentState);

    sm.processEvent(Event::SILENCE_DETECTED);
    TEST_ASSERT_EQUAL(AssistantState::PROCESSING, sm.currentState);

    sm.processEvent(Event::PROCESSING_COMPLETE);
    TEST_ASSERT_EQUAL(AssistantState::RESPONDING, sm.currentState);

    sm.processEvent(Event::PLAYBACK_COMPLETE);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);
}

void test_error_recovery_flow() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::PROCESSING;

    // Error during processing
    sm.processEvent(Event::ERROR_OCCURRED);
    TEST_ASSERT_EQUAL(AssistantState::ERROR, sm.currentState);

    // Auto-recover after timeout
    sm.processEvent(Event::ERROR_TIMEOUT);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);

    // Should be able to interact again
    TEST_ASSERT_TRUE(sm.canStartRecording());
}

void test_interrupt_during_playback() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::RESPONDING;

    // User presses button to interrupt
    sm.processEvent(Event::BOOT_BUTTON_PRESSED);
    TEST_ASSERT_EQUAL(AssistantState::IDLE, sm.currentState);

    // Should be ready for new interaction
    TEST_ASSERT_TRUE(sm.canStartRecording());
}

// ============================================================================
// Edge Cases
// ============================================================================

void test_multiple_button_presses_during_idle() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::IDLE;

    // First press starts listening
    sm.processEvent(Event::BOOT_BUTTON_PRESSED);
    TEST_ASSERT_EQUAL(AssistantState::LISTENING, sm.currentState);
}

void test_events_ignored_in_wrong_state() {
    AssistantStateMachine sm;
    sm.currentState = AssistantState::PROCESSING;

    // Wake word should be ignored during processing
    sm.processEvent(Event::WAKE_WORD_DETECTED);
    TEST_ASSERT_EQUAL(AssistantState::PROCESSING, sm.currentState);

    // Button should be ignored during processing
    sm.processEvent(Event::BOOT_BUTTON_PRESSED);
    TEST_ASSERT_EQUAL(AssistantState::PROCESSING, sm.currentState);
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // State transition tests
    RUN_TEST(test_initial_state_is_init);
    RUN_TEST(test_init_to_connecting);
    RUN_TEST(test_connecting_to_idle_on_success);
    RUN_TEST(test_connecting_to_error_on_failure);
    RUN_TEST(test_idle_to_listening_on_button);
    RUN_TEST(test_idle_to_listening_on_wake_word);
    RUN_TEST(test_idle_ignores_wake_word_when_disabled);
    RUN_TEST(test_listening_to_processing_on_silence);
    RUN_TEST(test_listening_to_processing_on_button);
    RUN_TEST(test_processing_to_responding);
    RUN_TEST(test_processing_to_error);
    RUN_TEST(test_responding_to_idle_on_complete);
    RUN_TEST(test_responding_interrupt_on_button);
    RUN_TEST(test_error_recovery_on_timeout);

    // State query tests
    RUN_TEST(test_can_start_recording_only_when_idle);
    RUN_TEST(test_can_interrupt_only_when_responding);
    RUN_TEST(test_is_active_excludes_init_error_connecting);

    // Integration tests
    RUN_TEST(test_complete_voice_interaction_flow);
    RUN_TEST(test_wake_word_flow);
    RUN_TEST(test_error_recovery_flow);
    RUN_TEST(test_interrupt_during_playback);

    // Edge cases
    RUN_TEST(test_multiple_button_presses_during_idle);
    RUN_TEST(test_events_ignored_in_wrong_state);

    return UNITY_END();
}
