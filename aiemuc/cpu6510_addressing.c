#include "cpu6510.h"

// ============================================================================
// ADDRESSING MODES - All modes organized alphabetically
// ============================================================================

// Absolute addressing
static inline void addr_abs_read(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, abs_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, abs_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, abs_data_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
}

static inline void addr_abs_write(cpu6510_state_t* cpu_dev, uint8_t value) {
    CPU_READY_OR_STALL(cpu_dev, abs_w_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, abs_w_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, value, abs_w_data_wait);
}

// Absolute,X addressing
static inline void addr_absx_read(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, absx_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, absx_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (cpu_dev->hi << 8) | cpu_dev->lo;
    cpu_dev->addr_abs = base + cpu_dev->x;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->addr_abs & 0xFF00)) {
        // Page crossed - dummy read from base address
        CPU_READY_OR_STALL(cpu_dev, absx_dummy_wait);
        (void)cpu_read_cycle(cpu_dev, base);
    }
    CPU_READY_OR_STALL(cpu_dev, absx_data_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
}

static inline void addr_absx_write(cpu6510_state_t* cpu_dev, uint8_t value) {
    CPU_READY_OR_STALL(cpu_dev, absx_w_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, absx_w_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (cpu_dev->hi << 8) | cpu_dev->lo;
    cpu_dev->addr_abs = base + cpu_dev->x;
    // Always dummy read for writes
    CPU_READY_OR_STALL(cpu_dev, absx_w_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, value, absx_w_data_wait);
}

// Absolute,Y addressing
static inline void addr_absy_read(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, absy_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, absy_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (cpu_dev->hi << 8) | cpu_dev->lo;
    cpu_dev->addr_abs = base + cpu_dev->y;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->addr_abs & 0xFF00)) {
        // Page crossed - dummy read from base address
        CPU_READY_OR_STALL(cpu_dev, absy_dummy_wait);
        (void)cpu_read_cycle(cpu_dev, base);
    }
    CPU_READY_OR_STALL(cpu_dev, absy_data_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
}

static inline void addr_absy_write(cpu6510_state_t* cpu_dev, uint8_t value) {
    CPU_READY_OR_STALL(cpu_dev, absy_w_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, absy_w_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (cpu_dev->hi << 8) | cpu_dev->lo;
    cpu_dev->addr_abs = base + cpu_dev->y;
    // Always dummy read for writes
    CPU_READY_OR_STALL(cpu_dev, absy_w_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, value, absy_w_data_wait);
}

// Accumulator addressing (implied)
static inline void addr_acc(cpu6510_state_t* cpu_dev) {
    // Dummy read from PC
    CPU_READY_OR_STALL(cpu_dev, acc_dummy_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc);
    cpu_dev->fetched = cpu_dev->a;
}

// Immediate addressing
static inline void addr_imm(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, imm_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->fetched = cpu_data;
}

// Implied addressing
static inline void addr_imp(cpu6510_state_t* cpu_dev) {
    // Dummy read from PC
    CPU_READY_OR_STALL(cpu_dev, imp_dummy_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc);
}

// Indirect addressing (JMP only)
static inline void addr_ind(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, ind_lo_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, ind_hi_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->hi = cpu_data;
    uint16_t ptr = (cpu_dev->hi << 8) | cpu_dev->lo;
    
    CPU_READY_OR_STALL(cpu_dev, ind_ptr_lo_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, ptr);
    cpu_dev->lo = cpu_data;
    // Handle page boundary bug - increment only low byte
    CPU_READY_OR_STALL(cpu_dev, ind_ptr_hi_wait);
    cpu_data = cpu_read_cycle(cpu_dev, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
}

// Zero page addressing
static inline void addr_zp(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zp_addr_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, zp_data_wait);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->fetched = cpu_data;
}

static inline void addr_zp_write(cpu6510_state_t* cpu_dev, uint8_t value) {
    CPU_READY_OR_STALL(cpu_dev, zp_w_addr_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = cpu_data;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, value, zp_w_data_wait);
}

// Zero page,X addressing
static inline void addr_zpx(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zpx_addr_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint8_t base = cpu_data;
    // Dummy read for extra cycle
    CPU_READY_OR_STALL(cpu_dev, zpx_dummy_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, base);
    cpu_dev->addr_abs = (base + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, zpx_data_wait);
    cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->fetched = cpu_data;
}

static inline void addr_zpx_write(cpu6510_state_t* cpu_dev, uint8_t value) {
    CPU_READY_OR_STALL(cpu_dev, zpx_w_addr_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint8_t base = cpu_data;
    // Dummy read for extra cycle
    CPU_READY_OR_STALL(cpu_dev, zpx_w_dummy_wait);
    cpu_data = cpu_read_cycle(cpu_dev, base);
    cpu_dev->addr_abs = (base + cpu_dev->x) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, value, zpx_w_data_wait);
}

// Zero page,Y addressing
static inline void addr_zpy(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zpy_addr_wait);
    uint8_t base = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    // Dummy read for extra cycle
    CPU_READY_OR_STALL(cpu_dev, zpy_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base);
    cpu_dev->addr_abs = (base + cpu_dev->y) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, zpy_data_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
}

static inline void addr_zpy_write(cpu6510_state_t* cpu_dev, uint8_t value) {
    CPU_READY_OR_STALL(cpu_dev, zpy_w_addr_wait);
    uint8_t base = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    // Dummy read for extra cycle
    CPU_READY_OR_STALL(cpu_dev, zpy_w_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base);
    cpu_dev->addr_abs = (base + cpu_dev->y) & 0xFF;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, value, zpy_w_data_wait);
}

// (Zero page,X) - Indexed Indirect addressing
static inline void addr_zpx_ind(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_base_wait);
    uint8_t base = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    // Dummy read for extra cycle
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base);
    uint8_t zp_addr = (base + cpu_dev->x) & 0xFF;
    
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_data_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
}

static inline void addr_zpx_ind_write(cpu6510_state_t* cpu_dev, uint8_t value) {
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_w_base_wait);
    uint8_t base = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_w_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base); // Dummy read for extra cycle
    uint8_t zp_addr = (base + cpu_dev->x) & 0xFF;
    
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_w_lo_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, zp_addr);
    cpu_dev->lo = cpu_data;
    CPU_READY_OR_STALL(cpu_dev, zpx_ind_w_hi_wait);
    cpu_data = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    cpu_dev->hi = cpu_data;
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, value, zpx_ind_w_data_wait);
}

// (Zero page),Y - Indirect Indexed addressing
static inline void addr_zp_ind_y(cpu6510_state_t* cpu_dev) {
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_base_wait);
    uint8_t zp_addr = cpu_read_cycle(cpu_dev, cpu_dev->pc++);

    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    uint16_t base = (cpu_dev->hi << 8) | cpu_dev->lo;
    cpu_dev->addr_abs = base + cpu_dev->y;
    
    // Check for page crossing
    if ((base & 0xFF00) != (cpu_dev->addr_abs & 0xFF00)) {
        // Page crossed - dummy read from base address
        CPU_READY_OR_STALL(cpu_dev, zp_ind_y_dummy_wait);
        (void)cpu_read_cycle(cpu_dev, base);
    }
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_data_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    cpu_dev->fetched = cpu_data;
}

static inline void addr_zp_ind_y_write(cpu6510_state_t* cpu_dev, uint8_t value) {
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_w_base_wait);
    uint8_t cpu_data = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint8_t zp_addr = cpu_data;
    
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_w_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, zp_addr);
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_w_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, (zp_addr + 1) & 0xFF);
    uint16_t base = (cpu_dev->hi << 8) | cpu_dev->lo;
    cpu_dev->addr_abs = base + cpu_dev->y;
    
    // Always dummy read for writes
    CPU_READY_OR_STALL(cpu_dev, zp_ind_y_w_dummy_wait);
    (void)cpu_read_cycle(cpu_dev, base);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, value, zp_ind_y_w_data_wait);
}

// ============================================================================
// READ-MODIFY-WRITE ADDRESSING MODES
// ============================================================================

// RMW Zero page
static inline void addr_rmw_zp(cpu6510_state_t* cpu_dev, uint8_t (*operation)(uint8_t)) {
    CPU_READY_OR_STALL(cpu_dev, rmw_zp_addr_wait);
    cpu_dev->addr_abs = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, rmw_zp_read_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    // Dummy write
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->fetched, rmw_zp_dummy_wait);
    cpu_dev->fetched = operation(cpu_dev->fetched);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->fetched, rmw_zp_write_wait);
}

// RMW Zero page,X
static inline void addr_rmw_zpx(cpu6510_state_t* cpu_dev, uint8_t (*operation)(uint8_t)) {
    CPU_READY_OR_STALL(cpu_dev, rmw_zpx_addr_wait);
    uint8_t base = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, rmw_zpx_dummy1_wait);
    (void)cpu_read_cycle(cpu_dev, base);
    cpu_dev->addr_abs = (base + cpu_dev->x) & 0xFF;
    CPU_READY_OR_STALL(cpu_dev, rmw_zpx_read_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    // Dummy write
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->fetched, rmw_zpx_dummy2_wait);
    cpu_dev->fetched = operation(cpu_dev->fetched);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->fetched, rmw_zpx_write_wait);
}

// RMW Absolute
static inline void addr_rmw_abs(cpu6510_state_t* cpu_dev, uint8_t (*operation)(uint8_t)) {
    CPU_READY_OR_STALL(cpu_dev, rmw_abs_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, rmw_abs_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    cpu_dev->addr_abs = (cpu_dev->hi << 8) | cpu_dev->lo;
    CPU_READY_OR_STALL(cpu_dev, rmw_abs_read_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    // Dummy write
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->fetched, rmw_abs_dummy_wait);
    cpu_dev->fetched = operation(cpu_dev->fetched);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->fetched, rmw_abs_write_wait);
}

// RMW Absolute,X
static inline void addr_rmw_absx(cpu6510_state_t* cpu_dev, uint8_t (*operation)(uint8_t)) {
    CPU_READY_OR_STALL(cpu_dev, rmw_absx_lo_wait);
    cpu_dev->lo = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    CPU_READY_OR_STALL(cpu_dev, rmw_absx_hi_wait);
    cpu_dev->hi = cpu_read_cycle(cpu_dev, cpu_dev->pc++);
    uint16_t base = (cpu_dev->hi << 8) | cpu_dev->lo;
    cpu_dev->addr_abs = base + cpu_dev->x;
    // Always dummy read for RMW
    CPU_READY_OR_STALL(cpu_dev, rmw_absx_dummy1_wait);
    (void)cpu_read_cycle(cpu_dev, base);
    CPU_READY_OR_STALL(cpu_dev, rmw_absx_read_wait);
    cpu_dev->fetched = cpu_read_cycle(cpu_dev, cpu_dev->addr_abs);
    // Dummy write
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->fetched, rmw_absx_dummy2_wait);
    cpu_dev->fetched = operation(cpu_dev->fetched);
    WAIT_READY_THEN_WRITE(cpu_dev, cpu_dev->addr_abs, cpu_dev->fetched, rmw_absx_write_wait);
}
