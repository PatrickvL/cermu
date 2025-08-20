#include "nmi_skipping.h"
#include "interrupt_recognition.h"
#include "mos6510_state.h"
#include "mos6510_registers.h"
#include "timing_states.h"
#include "cpu_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/**
 * Comprehensive Test Suite for MOS6510 NMI Skipping Conditions
 * 
 * Tests all 4 critical NMI skipping conditions discovered by visual6502 analysis:
 * 1. Lost NMI during IRQ vector fetch
 * 2. Branch instruction masking
 * 3. Critical timing window miss
 * 4. Pipeline-induced delays with SEI/CLI
 */

// ===== TEST FRAMEWORK =====

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("  ✓ %s\n", message); \
        } else { \
            tests_failed++; \
            printf("  ✗ FAIL: %s\n", message); \
        } \
    } while (0)

#define TEST_SECTION(name) \
    printf("\n=== %s ===\n", name)

// ===== TEST HELPER FUNCTIONS =====

/**
 * Create a test CPU state with basic configuration
 */
static struct mos6510_state_s* create_test_cpu(void) {
    cpu_config_t config = {
        .cpu_variant = CPU_6510,
        .supports_illegal_ops = true,
        .has_io_port = true,
        .has_aec_pin = true,
        .address_lines = 16,
        .base_frequency = 1000000,
        .has_irq_pin = true,
        .has_nmi_pin = true,
        .has_reset_pin = true
    };
    
    struct mos6510_state_s* cpu = mos6510_create(&config);
    if (cpu) {
        mos6510_init(cpu, &config);
        mos6510_reset(cpu);
        cpu->timing_state = TIMING_T1F;  // Start in fetch state
    }
    
    return cpu;
}

/**
 * Create a test interrupt recognition system
 */
static interrupt_recognition_t* create_test_interrupt_recognition(void) {
    interrupt_recognition_t* int_rec = malloc(sizeof(interrupt_recognition_t));
    if (int_rec) {
        interrupt_recognition_init(int_rec);
    }
    return int_rec;
}

/**
 * Print NMI skipping state for debugging
 */
static void print_nmi_skipping_state(const nmi_skipping_state_t* skip_state) {
    char buffer[1024];
    nmi_skipping_dump(skip_state, buffer, sizeof(buffer));
    printf("    %s\n", buffer);
}

/**
 * Set CPU instruction register for testing specific opcodes
 */
static void set_cpu_instruction_register(struct mos6510_state_s* cpu, uint8_t opcode) {
    if (cpu) {
        cpu->instruction_register = opcode;
    }
}

/**
 * Set CPU timing state for testing
 */
static void set_cpu_timing_state(struct mos6510_state_s* cpu, uint8_t timing_state) {
    if (cpu) {
        cpu->timing_state = timing_state;
    }
}

// ===== BASIC FUNCTIONALITY TESTS =====

static void test_nmi_skipping_init(void) {
    TEST_SECTION("NMI Skipping Initialization");
    
    nmi_skipping_state_t skip_state;
    nmi_skipping_init(&skip_state);
    
    // Test initial state
    TEST_ASSERT(skip_state.current_condition == NMI_SKIP_NONE, "Current condition initialized to NONE");
    TEST_ASSERT(skip_state.nmi_skip_active == false, "Skip active flag initialized to false");
    
    // Test condition 1 initialization
    TEST_ASSERT(skip_state.irq_vector_fetch_active == false, "IRQ vector fetch not active initially");
    TEST_ASSERT(skip_state.nmi_lost_during_irq_vector == false, "No NMI lost initially");
    
    // Test condition 2 initialization
    TEST_ASSERT(skip_state.branch_t3_to_t1f_sequence == false, "Branch sequence not active initially");
    TEST_ASSERT(skip_state.branch_masking_next_instr == false, "No branch masking initially");
    
    // Test condition 3 initialization
    TEST_ASSERT(skip_state.timing_window_t5_phi1 == false, "T5 φ1 window not active initially");
    TEST_ASSERT(skip_state.timing_window_t1_phi1 == false, "T1 φ1 window not active initially");
    
    // Test condition 4 initialization
    TEST_ASSERT(skip_state.sei_cli_instruction_active == false, "SEI/CLI not active initially");
    TEST_ASSERT(skip_state.interrupt_slip_window == false, "No interrupt slip window initially");
    
    // Test statistics initialization
    TEST_ASSERT(skip_state.condition1_count == 0, "Condition 1 count initialized to 0");
    TEST_ASSERT(skip_state.condition2_count == 0, "Condition 2 count initialized to 0");
    TEST_ASSERT(skip_state.condition3_count == 0, "Condition 3 count initialized to 0");
    TEST_ASSERT(skip_state.condition4_count == 0, "Condition 4 count initialized to 0");
    
    // Test validation
    TEST_ASSERT(nmi_skipping_validate(&skip_state), "Initial state passes validation");
}

static void test_nmi_skipping_reset(void) {
    TEST_SECTION("NMI Skipping Reset");
    
    nmi_skipping_state_t skip_state;
    nmi_skipping_init(&skip_state);
    
    // Set up some skip conditions first
    skip_state.current_condition = NMI_SKIP_BRANCH_MASKING;
    skip_state.nmi_skip_active = true;
    skip_state.branch_masking_next_instr = true;
    skip_state.condition2_count = 5;
    
    // Reset the skipping system
    nmi_skipping_reset(&skip_state);
    
    // Test reset behavior
    TEST_ASSERT(skip_state.current_condition == NMI_SKIP_NONE, "Current condition cleared on reset");
    TEST_ASSERT(skip_state.nmi_skip_active == false, "Skip active flag cleared on reset");
    TEST_ASSERT(skip_state.branch_masking_next_instr == false, "Branch masking cleared on reset");
    
    TEST_ASSERT(nmi_skipping_validate(&skip_state), "Reset state passes validation");
}

// ===== CONDITION 1: LOST NMI DURING IRQ VECTOR FETCH TESTS =====

static void test_condition1_irq_vector_fetch_detection(void) {
    TEST_SECTION("Condition 1: IRQ Vector Fetch Detection");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    interrupt_recognition_t* int_rec = create_test_interrupt_recognition();
    
    nmi_skipping_init(&skip_state);
    
    // Set up IRQ vector fetch state
    interrupt_recognition_reset(int_rec);
    int_rec->irq_stage = INTERRUPT_STAGE_BRK_SUBST;
    int_rec->interrupt_vector_type = VECTOR_IRQ_BRK;
    int_rec->brk_substitution_ready = true;
    set_cpu_timing_state(cpu, TIMING_VEC);  // Vector fetch timing state
    
    // Test IRQ vector fetch detection
    bool irq_fetch_active = nmi_skipping_irq_vector_fetch_active(cpu, int_rec);
    TEST_ASSERT(irq_fetch_active == true, "IRQ vector fetch detected when conditions met");
    
    // Test condition 1 update
    nmi_skipping_condition1_update(&skip_state, cpu, int_rec, true);  // NMI inactive
    TEST_ASSERT(skip_state.irq_vector_fetch_active == true, "IRQ vector fetch state tracked");
    TEST_ASSERT(skip_state.irq_vector_fetch_cycles == 3, "IRQ vector fetch cycles set to 3");
    
    // Simulate NMI going active for less than 3 cycles during IRQ vector fetch
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, false);  // NMI active (low)
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, false);  // Still active
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, true);   // NMI inactive (high) - less than 3 cycles
    
    TEST_ASSERT(skip_state.nmi_lost_during_irq_vector == true, "NMI loss during IRQ vector fetch detected");
    
    mos6510_destroy(cpu);
    free(int_rec);
}

static void test_condition1_nmi_loss_timing(void) {
    TEST_SECTION("Condition 1: NMI Loss Timing");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    interrupt_recognition_t* int_rec = create_test_interrupt_recognition();
    
    nmi_skipping_init(&skip_state);
    skip_state.irq_vector_fetch_active = true;  // Simulate IRQ vector fetch
    
    // Test NMI active for exactly 3 cycles (should NOT be lost)
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, false);  // Cycle 1
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, false);  // Cycle 2  
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, false);  // Cycle 3
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, true);   // NMI goes inactive
    
    TEST_ASSERT(skip_state.nmi_lost_during_irq_vector == false, "NMI NOT lost when active for 3+ cycles");
    
    // Reset and test NMI active for less than 3 cycles (should be lost)
    skip_state.nmi_lost_during_irq_vector = false;
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, false);  // Cycle 1
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, false);  // Cycle 2
    nmi_skipping_detect_nmi_loss_during_irq(&skip_state, true);   // NMI goes inactive (only 2 cycles)
    
    TEST_ASSERT(skip_state.nmi_lost_during_irq_vector == true, "NMI lost when active for less than 3 cycles");
    
    mos6510_destroy(cpu);
    free(int_rec);
}

// ===== CONDITION 2: BRANCH INSTRUCTION MASKING TESTS =====

static void test_condition2_branch_sequence_detection(void) {
    TEST_SECTION("Condition 2: Branch Sequence Detection");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    nmi_skipping_init(&skip_state);
    
    // Set up T3→T1F transition (branch sequence)
    skip_state.last_timing_state = TIMING_T3;
    set_cpu_timing_state(cpu, TIMING_T1F);
    
    // Test branch sequence detection
    bool branch_sequence = nmi_skipping_detect_branch_t3_to_t1f(&skip_state, cpu);
    TEST_ASSERT(branch_sequence == true, "T3→T1F branch sequence detected");
    
    // Test condition 2 update
    nmi_skipping_condition2_update(&skip_state, cpu, false);  // NMI pin state
    TEST_ASSERT(skip_state.branch_t3_to_t1f_sequence == true, "Branch T3→T1F sequence tracked");
    TEST_ASSERT(skip_state.branch_masking_next_instr == true, "Next instruction marked for masking");
    TEST_ASSERT(skip_state.branch_masking_cycles == 2, "Masking cycles set to 2");
    
    // Test that next instruction is masked
    TEST_ASSERT(nmi_skipping_next_instruction_masked(&skip_state) == true, "Next instruction is masked");
    
    mos6510_destroy(cpu);
}

static void test_condition2_masking_duration(void) {
    TEST_SECTION("Condition 2: Masking Duration");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    nmi_skipping_init(&skip_state);
    
    // Set up branch masking
    skip_state.branch_masking_next_instr = true;
    skip_state.branch_masking_cycles = 2;
    
    // Test masking countdown
    nmi_skipping_condition2_update(&skip_state, cpu, false);
    TEST_ASSERT(skip_state.branch_masking_cycles == 1, "Masking cycles decremented");
    TEST_ASSERT(skip_state.branch_masking_next_instr == true, "Still masked after 1 cycle");
    
    nmi_skipping_condition2_update(&skip_state, cpu, false);
    TEST_ASSERT(skip_state.branch_masking_cycles == 0, "Masking cycles reached 0");
    TEST_ASSERT(skip_state.branch_masking_next_instr == false, "Masking cleared after countdown");
    
    mos6510_destroy(cpu);
}

// ===== CONDITION 3: CRITICAL TIMING WINDOW TESTS =====

static void test_condition3_timing_windows(void) {
    TEST_SECTION("Condition 3: Timing Windows");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    nmi_skipping_init(&skip_state);
    
    // Test T5 φ1 window detection
    set_cpu_timing_state(cpu, TIMING_T5);
    nmi_skipping_detect_t5_phi1_window(&skip_state, cpu, true);  // φ1 phase
    TEST_ASSERT(skip_state.timing_window_t5_phi1 == true, "T5 φ1 window detected");
    
    // Test T1 φ1 window detection
    set_cpu_timing_state(cpu, TIMING_T1F);
    nmi_skipping_detect_t1_phi1_window(&skip_state, cpu, true, true);  // φ1 phase, NMI inactive
    TEST_ASSERT(skip_state.timing_window_t1_phi1 == true, "T1 φ1 window detected");
    
    mos6510_destroy(cpu);
}

static void test_condition3_nmi_window_miss(void) {
    TEST_SECTION("Condition 3: NMI Window Miss");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    nmi_skipping_init(&skip_state);
    
    // Simulate NMI going down at T5 φ1
    skip_state.timing_window_t5_phi1 = true;
    nmi_skipping_condition3_update(&skip_state, cpu, false, true);  // NMI active, φ1 phase
    TEST_ASSERT(skip_state.nmi_down_at_t5_phi1 == true, "NMI down at T5 φ1 tracked");
    
    // Simulate NMI going up before T1 φ1
    skip_state.nmi_down_at_t5_phi1 = true;  // Ensure this is set
    set_cpu_timing_state(cpu, TIMING_T1F);
    nmi_skipping_detect_t1_phi1_window(&skip_state, cpu, true, true);  // T1 φ1, NMI inactive
    TEST_ASSERT(skip_state.nmi_up_before_t1_phi1 == true, "NMI up before T1 φ1 detected");
    
    // Test complete condition 3 update
    nmi_skipping_condition3_update(&skip_state, cpu, true, true);  // NMI inactive, φ1 phase
    TEST_ASSERT(skip_state.critical_timing_cycles > 0, "Critical timing cycles tracked");
    
    mos6510_destroy(cpu);
}

// ===== CONDITION 4: PIPELINE-INDUCED DELAYS TESTS =====

static void test_condition4_sei_cli_detection(void) {
    TEST_SECTION("Condition 4: SEI/CLI Detection");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    nmi_skipping_init(&skip_state);
    
    // Test SEI instruction detection
    set_cpu_instruction_register(cpu, 0x78);  // SEI opcode
    bool sei_detected = nmi_skipping_detect_sei_cli_instruction(cpu);
    TEST_ASSERT(sei_detected == true, "SEI instruction detected");
    
    // Test CLI instruction detection
    set_cpu_instruction_register(cpu, 0x58);  // CLI opcode
    bool cli_detected = nmi_skipping_detect_sei_cli_instruction(cpu);
    TEST_ASSERT(cli_detected == true, "CLI instruction detected");
    
    // Test non-SEI/CLI instruction
    set_cpu_instruction_register(cpu, 0xEA);  // NOP opcode
    bool nop_detected = nmi_skipping_detect_sei_cli_instruction(cpu);
    TEST_ASSERT(nop_detected == false, "Non-SEI/CLI instruction not detected");
    
    mos6510_destroy(cpu);
}

static void test_condition4_pipeline_delay(void) {
    TEST_SECTION("Condition 4: Pipeline Delay");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    nmi_skipping_init(&skip_state);
    
    // Simulate SEI instruction execution
    set_cpu_instruction_register(cpu, 0x78);  // SEI opcode
    nmi_skipping_condition4_update(&skip_state, cpu, false);  // NMI active
    
    TEST_ASSERT(skip_state.sei_cli_instruction_active == true, "SEI instruction active");
    TEST_ASSERT(skip_state.status_register_delay == true, "Status register delay active");
    TEST_ASSERT(skip_state.pipeline_delay_cycles == 2, "Pipeline delay cycles set to 2");
    
    // Test interrupt slip window detection
    nmi_skipping_detect_interrupt_slip_window(&skip_state, false);  // NMI active during delay
    TEST_ASSERT(skip_state.interrupt_slip_window == true, "Interrupt slip window detected");
    
    // Test pipeline delay countdown
    nmi_skipping_condition4_update(&skip_state, cpu, false);
    TEST_ASSERT(skip_state.pipeline_delay_cycles == 1, "Pipeline delay cycles decremented");
    
    nmi_skipping_condition4_update(&skip_state, cpu, false);
    TEST_ASSERT(skip_state.pipeline_delay_cycles == 0, "Pipeline delay cycles reached 0");
    TEST_ASSERT(skip_state.sei_cli_instruction_active == false, "SEI instruction no longer active");
    TEST_ASSERT(skip_state.interrupt_slip_window == false, "Interrupt slip window cleared");
    
    mos6510_destroy(cpu);
}

// ===== COMPREHENSIVE INTEGRATION TESTS =====

static void test_multiple_conditions_priority(void) {
    TEST_SECTION("Multiple Conditions Priority");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    interrupt_recognition_t* int_rec = create_test_interrupt_recognition();
    
    nmi_skipping_init(&skip_state);
    
    // Set up multiple conditions simultaneously
    skip_state.nmi_lost_during_irq_vector = true;    // Condition 1
    skip_state.branch_masking_next_instr = true;     // Condition 2
    skip_state.interrupt_slip_window = true;         // Condition 4
    
    // Update should prioritize Condition 1 (IRQ Vector Fetch)
    nmi_skipping_update(&skip_state, cpu, int_rec, false, false);
    
    TEST_ASSERT(nmi_skipping_get_condition(&skip_state) == NMI_SKIP_IRQ_VECTOR_FETCH, 
                "Condition 1 has highest priority");
    TEST_ASSERT(nmi_skipping_should_skip_nmi(&skip_state) == true, 
                "NMI should be skipped with active condition");
    
    // Clear Condition 1, should fall back to Condition 4
    skip_state.nmi_lost_during_irq_vector = false;
    nmi_skipping_update(&skip_state, cpu, int_rec, false, false);
    
    TEST_ASSERT(nmi_skipping_get_condition(&skip_state) == NMI_SKIP_PIPELINE_DELAY,
                "Condition 4 active after Condition 1 cleared");
    
    mos6510_destroy(cpu);
    free(int_rec);
}

static void test_condition_statistics(void) {
    TEST_SECTION("Condition Statistics");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    interrupt_recognition_t* int_rec = create_test_interrupt_recognition();
    
    nmi_skipping_init(&skip_state);
    
    // Trigger each condition multiple times
    for (int i = 0; i < 3; i++) {
        // Condition 1
        skip_state.nmi_lost_during_irq_vector = true;
        nmi_skipping_update(&skip_state, cpu, int_rec, false, false);
        nmi_skipping_clear_condition(&skip_state);
        
        // Condition 2  
        skip_state.branch_masking_next_instr = true;
        nmi_skipping_update(&skip_state, cpu, int_rec, false, false);
        nmi_skipping_clear_condition(&skip_state);
    }
    
    // Check statistics
    uint32_t c1, c2, c3, c4, total_skipped, total_delayed;
    nmi_skipping_get_statistics(&skip_state, &c1, &c2, &c3, &c4, &total_skipped, &total_delayed);
    
    TEST_ASSERT(c1 == 3, "Condition 1 count correct");
    TEST_ASSERT(c2 == 3, "Condition 2 count correct");
    TEST_ASSERT(total_skipped == 6, "Total skipped count correct");
    
    // Test helper functions
    uint32_t total_events = nmi_skipping_get_total_events(&skip_state);
    TEST_ASSERT(total_events == 6, "Total events count correct");
    
    mos6510_destroy(cpu);
    free(int_rec);
}

static void test_complete_nmi_skipping_sequence(void) {
    TEST_SECTION("Complete NMI Skipping Sequence");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    interrupt_recognition_t* int_rec = create_test_interrupt_recognition();
    
    nmi_skipping_init(&skip_state);
    
    printf("    Simulating complete NMI skipping detection sequence...\n");
    
    // Step 1: Normal operation (no skipping)
    nmi_skipping_update(&skip_state, cpu, int_rec, true, true);  // Both pins inactive
    TEST_ASSERT(nmi_skipping_should_skip_nmi(&skip_state) == false, "No skipping in normal operation");
    
    // Step 2: Trigger Condition 2 (Branch masking)
    skip_state.last_timing_state = TIMING_T3;
    set_cpu_timing_state(cpu, TIMING_T1F);
    nmi_skipping_update(&skip_state, cpu, int_rec, false, true);  // NMI active
    
    TEST_ASSERT(nmi_skipping_get_condition(&skip_state) == NMI_SKIP_BRANCH_MASKING,
                "Branch masking condition detected");
    TEST_ASSERT(nmi_skipping_should_skip_nmi(&skip_state) == true, "NMI should be skipped");
    
    // Step 3: Clear condition and verify cleanup
    nmi_skipping_clear_condition(&skip_state);
    TEST_ASSERT(nmi_skipping_get_condition(&skip_state) == NMI_SKIP_NONE, "Condition cleared");
    TEST_ASSERT(nmi_skipping_should_skip_nmi(&skip_state) == false, "NMI skipping deactivated");
    
    printf("    ✓ Complete NMI skipping sequence executed successfully\n");
    
    mos6510_destroy(cpu);
    free(int_rec);
}

// ===== VALIDATION AND DEBUG TESTS =====

static void test_validation_and_debugging(void) {
    TEST_SECTION("Validation and Debugging");
    
    nmi_skipping_state_t skip_state;
    
    nmi_skipping_init(&skip_state);
    
    // Test validation of valid state
    TEST_ASSERT(nmi_skipping_validate(&skip_state) == true, "Valid initial state passes validation");
    
    // Test helper functions
    TEST_ASSERT(nmi_skipping_any_condition_active(&skip_state) == false, "No conditions active initially");
    TEST_ASSERT(nmi_skipping_condition_active(&skip_state, NMI_SKIP_NONE) == false, "NONE condition check");
    
    // Set up a condition and test helpers
    skip_state.current_condition = NMI_SKIP_BRANCH_MASKING;
    skip_state.branch_masking_next_instr = true;
    
    TEST_ASSERT(nmi_skipping_any_condition_active(&skip_state) == true, "Active condition detected");
    TEST_ASSERT(nmi_skipping_condition_active(&skip_state, NMI_SKIP_BRANCH_MASKING) == true, 
                "Specific condition detected");
    
    // Test condition name functions
    TEST_ASSERT(strcmp(nmi_skip_condition_name(NMI_SKIP_NONE), "NONE") == 0, "NONE condition name");
    TEST_ASSERT(strcmp(nmi_skip_condition_name(NMI_SKIP_BRANCH_MASKING), "BRANCH_MASKING") == 0, 
                "BRANCH_MASKING condition name");
    
    // Test debugging dump
    char buffer[1024];
    nmi_skipping_dump(&skip_state, buffer, sizeof(buffer));
    TEST_ASSERT(strlen(buffer) > 0, "Debug dump generates output");
    printf("    Debug dump: %s\n", buffer);
    
    // Test condition description
    char desc[256];
    nmi_skipping_get_condition_description(&skip_state, desc, sizeof(desc));
    TEST_ASSERT(strlen(desc) > 0, "Condition description generated");
    printf("    Condition description: %s\n", desc);
}

// ===== EDGE CASE TESTS =====

static void test_edge_cases(void) {
    TEST_SECTION("Edge Cases");
    
    nmi_skipping_state_t skip_state;
    struct mos6510_state_s* cpu = create_test_cpu();
    interrupt_recognition_t* int_rec = create_test_interrupt_recognition();
    
    nmi_skipping_init(&skip_state);
    
    // Test NULL parameter handling
    nmi_skipping_update(NULL, cpu, int_rec, false, false);  // Should not crash
    nmi_skipping_update(&skip_state, NULL, int_rec, false, false);  // Should not crash
    nmi_skipping_update(&skip_state, cpu, NULL, false, false);  // Should not crash
    
    TEST_ASSERT(true, "NULL parameter handling works");
    
    // Test timing state transitions
    bool transition = nmi_skipping_timing_transition_detected(&skip_state, cpu, TIMING_T3, TIMING_T1F);
    TEST_ASSERT(transition == false, "Timing transition detection works with no transition");
    
    skip_state.last_timing_state = TIMING_T3;
    set_cpu_timing_state(cpu, TIMING_T1F);
    transition = nmi_skipping_timing_transition_detected(&skip_state, cpu, TIMING_T3, TIMING_T1F);
    TEST_ASSERT(transition == true, "Timing transition detection works with valid transition");
    
    // Test timing state checking
    bool in_state = nmi_skipping_in_timing_state(cpu, TIMING_T1F);
    TEST_ASSERT(in_state == true, "Timing state check works");
    
    mos6510_destroy(cpu);
    free(int_rec);
}

// ===== MAIN TEST RUNNER =====

int main(void) {
    printf("MOS6510 NMI Skipping Conditions Test Suite\n");
    printf("==========================================\n");
    
    // Basic functionality tests
    test_nmi_skipping_init();
    test_nmi_skipping_reset();
    
    // Condition 1: Lost NMI during IRQ vector fetch
    test_condition1_irq_vector_fetch_detection();
    test_condition1_nmi_loss_timing();
    
    // Condition 2: Branch instruction masking
    test_condition2_branch_sequence_detection();
    test_condition2_masking_duration();
    
    // Condition 3: Critical timing window miss
    test_condition3_timing_windows();
    test_condition3_nmi_window_miss();
    
    // Condition 4: Pipeline-induced delays
    test_condition4_sei_cli_detection();
    test_condition4_pipeline_delay();
    
    // Comprehensive integration tests
    test_multiple_conditions_priority();
    test_condition_statistics();
    test_complete_nmi_skipping_sequence();
    
    // Validation and debugging tests
    test_validation_and_debugging();
    test_edge_cases();
    
    // Print test summary
    printf("\n==========================================\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Success rate: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    if (tests_failed == 0) {
        printf("\n🎉 All tests passed! NMI Skipping Conditions system is working correctly.\n");
        printf("\nAll 4 critical NMI skipping conditions are properly implemented:\n");
        printf("  ✓ Condition 1: Lost NMI during IRQ vector fetch\n");
        printf("  ✓ Condition 2: Branch instruction masking\n");
        printf("  ✓ Condition 3: Critical timing window miss\n");
        printf("  ✓ Condition 4: Pipeline-induced delays with SEI/CLI\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Please review the implementation.\n");
        return 1;
    }
}