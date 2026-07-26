// ST-01: Native ALU wrappers for 0x5f3759df and branchless SWAR conditionals

routine fast_inverse_sqrt(number) {
    // 0x5f3759df bit-level hack
    unsafe {
        UNROLL(1) {
            reg i = 0x5f3759df - (number >> 1)
        }
        return i
    }
}

// Branchless SWAR (SIMD Within A Register) conditional: return a > b ? x : y
routine swar_conditional(a, b, x, y) {
    unsafe {
        // Compute mask (all 1s if a > b, else 0) using bitwise arithmetic
        reg diff = b - a
        reg sign_bit = diff >> 63
        reg mask = (sign_bit ^ 1) * -1
        
        // Select x or y
        reg result = (x & mask) | (y & ~mask)
        return result
    }
}

routine probe_smm {
    unsafe {
        asm("out 0xB2, 0x00") // Trigger SMI
    }
}

routine write_io_port(port, val) {
    unsafe {
        asm("out port, val")
    }
}
