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
 * Comprehensive Test Suite for MOS6510 4-Stage Interrupt Recognition
 * 
 * Tests all aspects of the interrupt recognition system including:
 * - 4-stage recognition process for all interrupt types
 * - Hardware node simulation and timing dependencies
 * - NMI edge detection and latching
 * - Interrupt priority resolution
 * - φ2 sampling and asynchronous-to-synchronous conversion
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
        
        // Set initial timing state
        cpu->timing_state = TIMING_T1F;  // Start in fetch state
    }
    
    return cpu;
}

/**
 * Set processor status flags for testing IRQ masking
 */
static void set_cpu_flags(struct mos6510_state_s* cpu, uint8_t flags) {
    if (cpu) {
        cpu->registers[REG_P] = flags;
    }
}

/**
 * Print interrupt recognition state for debugging
 */
static void print_interrupt_state(const interrupt_recognition_t* int_rec) {
    char buffer[512];
    interrupt_recognition_dump(int_rec, buffer, sizeof(buffer));
    printf("    %s\n", buffer);
}

// ===== BASIC FUNCTIONALITY TESTS =====

static void test_interrupt_recognition_init(void) {
    TEST_SECTION("Interrupt Recognition Initialization");
    
    interrupt_recognition_t int_rec;
    interrupt_recognition_init(&int_rec);
    
    // Test initial state
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_IDLE, "NMI stage initialized to IDLE");
    TEST_ASSERT(int_rec.irq_stage == INTERRUPT_STAGE_IDLE, "IRQ stage initialized to IDLE");
    TEST_ASSERT(int_rec.reset_stage == INTERRUPT_STAGE_IDLE, "Reset stage initialized to IDLE");
    
    // Test edge detection initialization
    TEST_ASSERT(int_rec.nmi_last_state == true, "NMI last state initialized high (inactive)");
    TEST_ASSERT(int_rec.nmi_edge_detected == false, "NMI edge detection cleared");
    TEST_ASSERT(int_rec.nmi_edge_latched == false, "NMI edge latch cleared");
    
    // Test hardware nodes initialization
    TEST_ASSERT(int_rec.nodes.nmig_node == false, "~NMIG node initialized inactive");
    TEST_ASSERT(int_rec.nodes.irqp_node == false, "IRQP node initialized inactive");
    TEST_ASSERT(int_rec.nodes.resp_node == false, "RESP node initialized inactive");
    
    // Test BRK substitution initialization
    TEST_ASSERT(int_rec.brk_substitution_ready == false, "BRK substitution not ready initially");
    TEST_ASSERT(int_rec.interrupt_vector_type == VECTOR_NONE, "No vector type initially");
    
    // Test validation
    TEST_ASSERT(interrupt_recognition_validate(&int_rec), "Initial state passes validation");
}

static void test_interrupt_recognition_reset(void) {
    TEST_SECTION("Interrupt Recognition Reset");
    
    interrupt_recognition_t int_rec;
    interrupt_recognition_init(&int_rec);
    
    // Simulate some interrupt activity first
    int_rec.nmi_stage = INTERRUPT_STAGE_PENDING;
    int_rec.irq_stage = INTERRUPT_STAGE_PENDING;
    int_rec.nodes.nmig_node = true;
    int_rec.nodes.irqp_node = true;
    
    // Reset the interrupt recognition system
    interrupt_recognition_reset(&int_rec);
    
    // Test reset behavior
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_IDLE, "NMI stage cleared on reset");
    TEST_ASSERT(int_rec.irq_stage == INTERRUPT_STAGE_IDLE, "IRQ stage cleared on reset");
    TEST_ASSERT(int_rec.reset_stage == INTERRUPT_STAGE_BRK_SUBST, "Reset stage active immediately");
    
    // Test hardware nodes after reset
    TEST_ASSERT(int_rec.nodes.nmig_node == false, "~NMIG node cleared on reset");
    TEST_ASSERT(int_rec.nodes.irqp_node == false, "IRQP node cleared on reset");
    TEST_ASSERT(int_rec.nodes.resp_node == true, "RESP node active on reset");
    TEST_ASSERT(int_rec.nodes.reset_active == true, "Reset active node set");
    
    // Test reset vector setup
    TEST_ASSERT(int_rec.interrupt_vector_type == VECTOR_RESET, "Reset vector type set");
    TEST_ASSERT(int_rec.brk_substitution_ready == true, "BRK substitution ready for reset");
    
    TEST_ASSERT(interrupt_recognition_validate(&int_rec), "Reset state passes validation");
}

// ===== STAGE 0: φ2 SAMPLING TESTS =====

static void test_stage0_phi2_sampling(void) {
    TEST_SECTION("Stage 0: φ2 Sampling");
    
    interrupt_recognition_t int_rec;
    interrupt_recognition_init(&int_rec);
    
    // Test φ2 sampling with all pins inactive (high, since active low)
    interrupt_stage0_async_to_sync(&int_rec, true, true, true);
    
    TEST_ASSERT(int_rec.nodes.phi2_sample_nmi == false, "NMI sampled inactive (pin high)");
    TEST_ASSERT(int_rec.nodes.phi2_sample_irq == false, "IRQ sampled inactive (pin high)");
    TEST_ASSERT(int_rec.nodes.phi2_sample_reset == false, "Reset sampled inactive (pin high)");
    
    // Test φ2 sampling with all pins active (low, since active low)
    interrupt_stage0_async_to_sync(&int_rec, false, false, false);
    
    TEST_ASSERT(int_rec.nodes.phi2_sample_nmi == true, "NMI sampled active (pin low)");
    TEST_ASSERT(int_rec.nodes.phi2_sample_irq == true, "IRQ sampled active (pin low)");
    TEST_ASSERT(int_rec.nodes.phi2_sample_reset == true, "Reset sampled active (pin low)");
    
    // Test stage advancement from IDLE to ASYNC_SYNC
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_ASYNC_SYNC, "NMI advanced to ASYNC_SYNC");
    TEST_ASSERT(int_rec.irq_stage == INTERRUPT_STAGE_ASYNC_SYNC, "IRQ advanced to ASYNC_SYNC");
    TEST_ASSERT(int_rec.reset_stage == INTERRUPT_STAGE_ASYNC_SYNC, "Reset advanced to ASYNC_SYNC");
}

static void test_nmi_edge_detection(void) {
    TEST_SECTION("NMI Edge Detection");
    
    interrupt_recognition_t int_rec;
    interrupt_recognition_init(&int_rec);
    
    // Test no edge initially (both states high)
    interrupt_recognition_nmi_edge_detect(&int_rec, true);
    TEST_ASSERT(int_rec.nmi_edge_detected == false, "No edge detected when staying high");
    
    // Test falling edge detection (high to low)
    interrupt_recognition_nmi_edge_detect(&int_rec, false);
    TEST_ASSERT(int_rec.nmi_edge_detected == true, "Falling edge detected");
    TEST_ASSERT(int_rec.nmi_last_state == false, "Last state updated to low");
    
    // Test edge latching
    interrupt_recognition_nmi_edge_latch(&int_rec);
    TEST_ASSERT(int_rec.nmi_edge_latched == true, "NMI edge latched");
    TEST_ASSERT(int_rec.nmi_edge_detected == false, "Edge detected flag cleared after latching");
    
    // Test no additional edge while low
    interrupt_recognition_nmi_edge_detect(&int_rec, false);
    TEST_ASSERT(int_rec.nmi_edge_detected == false, "No edge detected when staying low");
    
    // Test rising edge (should not trigger another edge)
    interrupt_recognition_nmi_edge_detect(&int_rec, true);
    TEST_ASSERT(int_rec.nmi_edge_detected == false, "Rising edge does not trigger detection");
    
    // Test edge clearing
    interrupt_recognition_nmi_edge_clear(&int_rec);
    TEST_ASSERT(int_rec.nmi_edge_latched == false, "NMI edge latch cleared");
}

// ===== STAGE 1: EDGE/LEVEL DETECTION TESTS =====

static void test_stage1_edge_level_detection(void) {
    TEST_SECTION("Stage 1: Edge/Level Detection");
    
    interrupt_recognition_t int_rec;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    interrupt_recognition_init(&int_rec);
    
    // Set up NMI in ASYNC_SYNC stage with edge detected
    int_rec.nmi_stage = INTERRUPT_STAGE_ASYNC_SYNC;
    int_rec.nmi_edge_detected = true;
    
    // Set up IRQ in ASYNC_SYNC stage with IRQ not masked
    int_rec.irq_stage = INTERRUPT_STAGE_ASYNC_SYNC;
    int_rec.nodes.phi2_sample_irq = true;
    set_cpu_flags(cpu, 0x00);  // Clear I flag (IRQ not masked)
    
    // Set up Reset in ASYNC_SYNC stage
    int_rec.reset_stage = INTERRUPT_STAGE_ASYNC_SYNC;
    int_rec.nodes.phi2_sample_reset = true;
    
    // Execute stage 1
    interrupt_stage1_edge_level_detect(&int_rec, cpu);
    
    // Test NMI stage 1 behavior
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_EDGE_LEVEL, "NMI advanced to EDGE_LEVEL");
    TEST_ASSERT(int_rec.nodes.nmig_node == true, "~NMIG node activated");
    TEST_ASSERT(int_rec.nmi_edge_latched == true, "NMI edge latched during stage 1");
    
    // Test IRQ stage 1 behavior
    TEST_ASSERT(int_rec.irq_stage == INTERRUPT_STAGE_EDGE_LEVEL, "IRQ advanced to EDGE_LEVEL");
    TEST_ASSERT(int_rec.nodes.irqp_node == true, "IRQP node activated");
    
    // Test Reset stage 1 behavior
    TEST_ASSERT(int_rec.reset_stage == INTERRUPT_STAGE_EDGE_LEVEL, "Reset advanced to EDGE_LEVEL");
    TEST_ASSERT(int_rec.nodes.resp_node == true, "RESP node activated");
    
    mos6510_destroy(cpu);
}

static void test_irq_masking(void) {
    TEST_SECTION("IRQ Masking");
    
    interrupt_recognition_t int_rec;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    interrupt_recognition_init(&int_rec);
    
    // Set up IRQ in ASYNC_SYNC stage with IRQ masked (I flag set)
    int_rec.irq_stage = INTERRUPT_STAGE_ASYNC_SYNC;
    int_rec.nodes.phi2_sample_irq = true;
    set_cpu_flags(cpu, 0x04);  // Set I flag (IRQ masked)
    
    // Execute stage 1
    interrupt_stage1_edge_level_detect(&int_rec, cpu);
    
    // Test that IRQ is blocked by I flag
    TEST_ASSERT(int_rec.irq_stage == INTERRUPT_STAGE_IDLE, "Masked IRQ returns to IDLE");
    TEST_ASSERT(int_rec.nodes.irqp_node == false, "IRQP node not activated when masked");
    
    // Test IRQ masking check function
    TEST_ASSERT(interrupt_recognition_irq_masked(cpu) == true, "IRQ masking detected");
    
    // Clear I flag and test again
    set_cpu_flags(cpu, 0x00);
    TEST_ASSERT(interrupt_recognition_irq_masked(cpu) == false, "IRQ not masked when I flag clear");
    
    mos6510_destroy(cpu);
}

// ===== STAGE 2: PENDING TO ACTIVE TESTS =====

static void test_stage2_timing_dependencies(void) {
    TEST_SECTION("Stage 2: Timing Dependencies");
    
    interrupt_recognition_t int_rec;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    interrupt_recognition_init(&int_rec);
    
    // Set up interrupts in EDGE_LEVEL stage
    int_rec.nmi_stage = INTERRUPT_STAGE_EDGE_LEVEL;
    int_rec.nmi_edge_latched = true;
    int_rec.irq_stage = INTERRUPT_STAGE_EDGE_LEVEL;
    
    // Test normal instruction timing (T0 φ2 trigger)
    interrupt_recognition_set_timing_mode(&int_rec, false, false);  // Not branch, no page cross
    cpu->timing_state = TIMING_T0;
    
    interrupt_stage2_pending_to_active(&int_rec, cpu);
    
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_PENDING, "NMI advanced to PENDING on T0");
    TEST_ASSERT(int_rec.irq_stage == INTERRUPT_STAGE_PENDING, "IRQ advanced to PENDING on T0");
    TEST_ASSERT(int_rec.nodes.intg_node == true, "INTG node activated for stage 2");
    
    // Reset for branch instruction test
    interrupt_recognition_init(&int_rec);
    int_rec.nmi_stage = INTERRUPT_STAGE_EDGE_LEVEL;
    int_rec.nmi_edge_latched = true;
    
    // Test branch instruction timing (T2 φ2 trigger)
    interrupt_recognition_set_timing_mode(&int_rec, true, false);   // Branch, no page cross
    cpu->timing_state = TIMING_T2;
    
    interrupt_stage2_pending_to_active(&int_rec, cpu);
    
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_PENDING, "NMI advanced to PENDING on T2 for branch");
    
    // Test that T0 doesn't trigger for branch instructions
    cpu->timing_state = TIMING_T0;
    int_rec.nmi_stage = INTERRUPT_STAGE_EDGE_LEVEL;  // Reset to test
    
    interrupt_stage2_pending_to_active(&int_rec, cpu);
    
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_EDGE_LEVEL, "NMI not advanced on T0 for branch");
    
    mos6510_destroy(cpu);
}

// ===== STAGE 3: BRK SUBSTITUTION TESTS =====

static void test_stage3_brk_substitution(void) {
    TEST_SECTION("Stage 3: BRK Substitution");
    
    interrupt_recognition_t int_rec;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    interrupt_recognition_init(&int_rec);
    
    // Set up interrupts in PENDING stage
    int_rec.nmi_stage = INTERRUPT_STAGE_PENDING;
    int_rec.irq_stage = INTERRUPT_STAGE_PENDING;
    int_rec.reset_stage = INTERRUPT_STAGE_PENDING;
    
    // Set CPU in fetch phase (T1F with SYNC)
    cpu->timing_state = TIMING_T1F;
    
    // Execute stage 3
    interrupt_stage3_brk_substitution(&int_rec, cpu);
    
    // Test that highest priority interrupt (Reset) is selected
    TEST_ASSERT(int_rec.reset_stage == INTERRUPT_STAGE_BRK_SUBST, "Reset advanced to BRK_SUBST");
    TEST_ASSERT(int_rec.nodes.reset_active == true, "Reset active node set");
    TEST_ASSERT(int_rec.interrupt_vector_type == VECTOR_RESET, "Reset vector type selected");
    TEST_ASSERT(int_rec.brk_substitution_ready == true, "BRK substitution ready");
    
    // Test that lower priority interrupts remain in PENDING
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_PENDING, "NMI remains in PENDING");
    TEST_ASSERT(int_rec.irq_stage == INTERRUPT_STAGE_PENDING, "IRQ remains in PENDING");
    
    mos6510_destroy(cpu);
}

static void test_interrupt_priority(void) {
    TEST_SECTION("Interrupt Priority Resolution");
    
    interrupt_recognition_t int_rec;
    
    interrupt_recognition_init(&int_rec);
    
    // Test priority with all interrupts pending
    int_rec.reset_stage = INTERRUPT_STAGE_PENDING;
    int_rec.nmi_stage = INTERRUPT_STAGE_PENDING;
    int_rec.irq_stage = INTERRUPT_STAGE_PENDING;
    
    interrupt_vector_t highest = interrupt_recognition_get_highest_priority(&int_rec);
    TEST_ASSERT(highest == VECTOR_RESET, "Reset has highest priority");
    
    // Test priority with only NMI and IRQ
    int_rec.reset_stage = INTERRUPT_STAGE_IDLE;
    
    highest = interrupt_recognition_get_highest_priority(&int_rec);
    TEST_ASSERT(highest == VECTOR_NMI, "NMI has higher priority than IRQ");
    
    // Test priority with only IRQ
    int_rec.nmi_stage = INTERRUPT_STAGE_IDLE;
    
    highest = interrupt_recognition_get_highest_priority(&int_rec);
    TEST_ASSERT(highest == VECTOR_IRQ_BRK, "IRQ selected when only IRQ pending");
    
    // Test priority with no interrupts
    int_rec.irq_stage = INTERRUPT_STAGE_IDLE;
    
    highest = interrupt_recognition_get_highest_priority(&int_rec);
    TEST_ASSERT(highest == VECTOR_NONE, "No priority when no interrupts pending");
}

// ===== VECTOR HANDLING TESTS =====

static void test_vector_handling(void) {
    TEST_SECTION("Vector Handling");
    
    interrupt_recognition_t int_rec;
    
    interrupt_recognition_init(&int_rec);
    
    // Test vector address functions
    uint16_t reset_addr = interrupt_recognition_get_vector_address(VECTOR_RESET);
    uint16_t nmi_addr = interrupt_recognition_get_vector_address(VECTOR_NMI);
    uint16_t irq_addr = interrupt_recognition_get_vector_address(VECTOR_IRQ_BRK);
    
    TEST_ASSERT(reset_addr == 0xFFFC, "Reset vector address correct");
    TEST_ASSERT(nmi_addr == 0xFFFA, "NMI vector address correct");
    TEST_ASSERT(irq_addr == 0xFFFE, "IRQ/BRK vector address correct");
    
    // Test vector ready detection
    int_rec.brk_substitution_ready = false;
    TEST_ASSERT(interrupt_recognition_vector_ready(&int_rec) == false, "Vector not ready initially");
    
    int_rec.brk_substitution_ready = true;
    int_rec.interrupt_vector_type = VECTOR_NMI;
    TEST_ASSERT(interrupt_recognition_vector_ready(&int_rec) == true, "Vector ready when BRK substitution active");
    TEST_ASSERT(interrupt_recognition_get_vector_type(&int_rec) == VECTOR_NMI, "Correct vector type returned");
    
    // Test vector clearing
    interrupt_recognition_clear_interrupt(&int_rec, VECTOR_NMI);
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_IDLE, "NMI stage cleared after vector fetch");
    TEST_ASSERT(int_rec.brk_substitution_ready == false, "BRK substitution cleared");
    TEST_ASSERT(int_rec.interrupt_vector_type == VECTOR_NONE, "Vector type cleared");
    TEST_ASSERT(int_rec.nmi_edge_latched == false, "NMI edge latch cleared");
}

// ===== HARDWARE NODE SIMULATION TESTS =====

static void test_hardware_node_simulation(void) {
    TEST_SECTION("Hardware Node Simulation");
    
    interrupt_recognition_t int_rec;
    
    interrupt_recognition_init(&int_rec);
    
    // Test node updates based on stages
    int_rec.nmi_stage = INTERRUPT_STAGE_EDGE_LEVEL;
    int_rec.irq_stage = INTERRUPT_STAGE_PENDING;
    int_rec.reset_stage = INTERRUPT_STAGE_BRK_SUBST;
    
    interrupt_nodes_update(&int_rec.nodes, &int_rec);
    
    TEST_ASSERT(int_rec.nodes.nmig_node == true, "~NMIG node active for NMI stage 1+");
    TEST_ASSERT(int_rec.nodes.irqp_node == true, "IRQP node active for IRQ stage 1+");
    TEST_ASSERT(int_rec.nodes.resp_node == true, "RESP node active for Reset stage 1+");
    TEST_ASSERT(int_rec.nodes.intg_node == true, "INTG node active for stage 2+ interrupts");
    TEST_ASSERT(int_rec.nodes.resg_node == true, "RESG node active for Reset stage 2+");
    
    // Test node clearing when stages return to idle
    int_rec.nmi_stage = INTERRUPT_STAGE_IDLE;
    int_rec.irq_stage = INTERRUPT_STAGE_IDLE;
    int_rec.reset_stage = INTERRUPT_STAGE_IDLE;
    
    interrupt_nodes_update(&int_rec.nodes, &int_rec);
    
    TEST_ASSERT(int_rec.nodes.nmig_node == false, "~NMIG node inactive when NMI idle");
    TEST_ASSERT(int_rec.nodes.irqp_node == false, "IRQP node inactive when IRQ idle");
    TEST_ASSERT(int_rec.nodes.resp_node == false, "RESP node inactive when Reset idle");
    TEST_ASSERT(int_rec.nodes.intg_node == false, "INTG node inactive when no stage 2 interrupts");
    TEST_ASSERT(int_rec.nodes.resg_node == false, "RESG node inactive when Reset not stage 2+");
}

// ===== COMPREHENSIVE INTEGRATION TESTS =====

static void test_complete_nmi_sequence(void) {
    TEST_SECTION("Complete NMI Sequence");
    
    interrupt_recognition_t int_rec;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    interrupt_recognition_init(&int_rec);
    
    // Simulate complete NMI sequence
    printf("    Simulating complete NMI recognition sequence...\n");
    
    // Step 1: φ2 sampling with NMI pin going active
    cpu->timing_state = TIMING_T1F;  // Fetch state
    interrupt_recognition_set_timing_mode(&int_rec, false, false);  // Normal instruction timing
    
    interrupt_recognition_update(&int_rec, cpu, false, true, true);  // NMI pin active (low)
    
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_EDGE_LEVEL, "NMI progressed through stages 0 and 1");
    TEST_ASSERT(int_rec.nmi_edge_latched == true, "NMI edge latched");
    TEST_ASSERT(int_rec.nodes.nmig_node == true, "~NMIG node active");
    
    // Step 2: Stage 2 timing trigger (T0 φ2)
    cpu->timing_state = TIMING_T0;
    interrupt_recognition_update(&int_rec, cpu, false, true, true);
    
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_PENDING, "NMI advanced to PENDING on T0");
    TEST_ASSERT(int_rec.nodes.intg_node == true, "INTG node active");
    
    // Step 3: BRK substitution during fetch
    cpu->timing_state = TIMING_T1F;
    interrupt_recognition_update(&int_rec, cpu, false, true, true);
    
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_BRK_SUBST, "NMI ready for BRK substitution");
    TEST_ASSERT(interrupt_recognition_vector_ready(&int_rec) == true, "Vector ready for fetch");
    TEST_ASSERT(interrupt_recognition_get_vector_type(&int_rec) == VECTOR_NMI, "NMI vector selected");
    
    // Step 4: Vector fetch and cleanup
    interrupt_recognition_clear_interrupt(&int_rec, VECTOR_NMI);
    
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_IDLE, "NMI sequence completed");
    TEST_ASSERT(int_rec.nmi_edge_latched == false, "NMI edge latch cleared");
    TEST_ASSERT(interrupt_recognition_vector_ready(&int_rec) == false, "No vector ready after clearing");
    
    printf("    ✓ Complete NMI sequence executed successfully\n");
    
    mos6510_destroy(cpu);
}

static void test_multiple_interrupts_priority(void) {
    TEST_SECTION("Multiple Interrupts Priority");
    
    interrupt_recognition_t int_rec;
    struct mos6510_state_s* cpu = create_test_cpu();
    
    interrupt_recognition_init(&int_rec);
    set_cpu_flags(cpu, 0x00);  // IRQ not masked
    
    // Simulate all interrupts occurring simultaneously
    cpu->timing_state = TIMING_T1F;
    interrupt_recognition_set_timing_mode(&int_rec, false, false);
    
    // All interrupts active
    interrupt_recognition_update(&int_rec, cpu, false, false, false);  // All pins active
    
    // Allow progression to PENDING stage
    cpu->timing_state = TIMING_T0;
    interrupt_recognition_update(&int_rec, cpu, false, false, false);
    
    // Return to fetch for BRK substitution
    cpu->timing_state = TIMING_T1F;
    interrupt_recognition_update(&int_rec, cpu, false, false, false);
    
    // Test that Reset wins priority
    TEST_ASSERT(interrupt_recognition_get_vector_type(&int_rec) == VECTOR_RESET, "Reset has highest priority");
    TEST_ASSERT(int_rec.reset_stage == INTERRUPT_STAGE_BRK_SUBST, "Reset ready for BRK substitution");
    TEST_ASSERT(int_rec.nmi_stage == INTERRUPT_STAGE_PENDING, "NMI remains pending");
    TEST_ASSERT(int_rec.irq_stage == INTERRUPT_STAGE_PENDING, "IRQ remains pending");
    
    // Clear Reset and test NMI priority over IRQ
    interrupt_recognition_clear_interrupt(&int_rec, VECTOR_RESET);
    
    // Update to allow next interrupt
    interrupt_recognition_update(&int_rec, cpu, false, false, true);  // Reset pin inactive
    
    TEST_ASSERT(interrupt_recognition_get_vector_type(&int_rec) == VECTOR_NMI, "NMI has priority over IRQ");
    
    printf("    ✓ Interrupt priority resolution working correctly\n");
    
    mos6510_destroy(cpu);
}

// ===== VALIDATION AND DEBUG TESTS =====

static void test_validation_and_debugging(void) {
    TEST_SECTION("Validation and Debugging");
    
    interrupt_recognition_t int_rec;
    
    interrupt_recognition_init(&int_rec);
    
    // Test validation of valid state
    TEST_ASSERT(interrupt_recognition_validate(&int_rec) == true, "Valid initial state passes validation");
    
    // Test helper functions
    TEST_ASSERT(interrupt_recognition_active(&int_rec) == false, "No interrupt activity initially");
    TEST_ASSERT(interrupt_recognition_nmi_edge_latched(&int_rec) == false, "No NMI edge latched initially");
    
    // Set up some interrupt activity
    int_rec.nmi_stage = INTERRUPT_STAGE_PENDING;
    int_rec.nmi_edge_latched = true;
    int_rec.nodes.nmig_node = true;
    
    TEST_ASSERT(interrupt_recognition_active(&int_rec) == true, "Interrupt activity detected");
    TEST_ASSERT(interrupt_recognition_nmi_edge_latched(&int_rec) == true, "NMI edge latch detected");
    
    // Test stage name functions
    TEST_ASSERT(strcmp(interrupt_stage_name(INTERRUPT_STAGE_IDLE), "IDLE") == 0, "IDLE stage name correct");
    TEST_ASSERT(strcmp(interrupt_stage_name(INTERRUPT_STAGE_PENDING), "PENDING") == 0, "PENDING stage name correct");
    TEST_ASSERT(strcmp(interrupt_vector_name(VECTOR_NMI), "NMI") == 0, "NMI vector name correct");
    
    // Test debugging dump
    char buffer[1024];
    interrupt_recognition_dump(&int_rec, buffer, sizeof(buffer));
    TEST_ASSERT(strlen(buffer) > 0, "Debug dump generates output");
    printf("    Debug dump: %s\n", buffer);
    
    // Test statistics
    uint32_t nmi_cycles, irq_cycles, total_cycles;
    int_rec.nmi_recognition_cycles = 10;
    int_rec.irq_recognition_cycles = 5;
    
    interrupt_recognition_get_stats(&int_rec, &nmi_cycles, &irq_cycles, NULL, &total_cycles);
    TEST_ASSERT(nmi_cycles == 10, "NMI cycle count correct");
    TEST_ASSERT(irq_cycles == 5, "IRQ cycle count correct");
    TEST_ASSERT(total_cycles == 15, "Total cycle count correct");
}

// ===== MAIN TEST RUNNER =====

int main(void) {
    printf("MOS6510 4-Stage Interrupt Recognition Test Suite\n");
    printf("==================================================\n");
    
    // Basic functionality tests
    test_interrupt_recognition_init();
    test_interrupt_recognition_reset();
    
    // Stage 0 tests
    test_stage0_phi2_sampling();
    test_nmi_edge_detection();
    
    // Stage 1 tests
    test_stage1_edge_level_detection();
    test_irq_masking();
    
    // Stage 2 tests
    test_stage2_timing_dependencies();
    
    // Stage 3 tests
    test_stage3_brk_substitution();
    test_interrupt_priority();
    
    // Vector handling tests
    test_vector_handling();
    
    // Hardware node simulation tests
    test_hardware_node_simulation();
    
    // Comprehensive integration tests
    test_complete_nmi_sequence();
    test_multiple_interrupts_priority();
    
    // Validation and debugging tests
    test_validation_and_debugging();
    
    // Print test summary
    printf("\n==================================================\n");
    printf("Test Summary:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_failed);
    printf("  Success rate: %.1f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    if (tests_failed == 0) {
        printf("\n🎉 All tests passed! 4-Stage Interrupt Recognition system is working correctly.\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Please review the implementation.\n");
        return 1;
    }
}