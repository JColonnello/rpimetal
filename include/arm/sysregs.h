#ifndef _SYSREGS_H
#define _SYSREGS_H

// ***************************************
// SCTLR_EL1, System Control Register (EL1), Page 2654 of AArch64-Reference-Manual.
// ***************************************

#define SCTLR_RESERVED (3 << 28) | (3 << 22) | (1 << 20) | (1 << 11)
#define SCTLR_EE_LITTLE_ENDIAN (0 << 25)
#define SCTLR_EOE_LITTLE_ENDIAN (0 << 24)
#define SCTLR_I_CACHE_DISABLED (0 << 12)
#define SCTLR_D_CACHE_DISABLED (0 << 2)
#define SCTLR_I_CACHE_ENABLED (1 << 12)
#define SCTLR_D_CACHE_ENABLED (1 << 2)
#define SCTLR_MMU_DISABLED (0 << 0)
#define SCTLR_MMU_ENABLED (1 << 0)

#define SCTLR_VALUE_MMU_DISABLED \
	(SCTLR_RESERVED | SCTLR_EE_LITTLE_ENDIAN | SCTLR_I_CACHE_DISABLED | SCTLR_D_CACHE_DISABLED | SCTLR_MMU_DISABLED)
#define SCTLR_VALUE_MMU_ENABLE (SCTLR_MMU_ENABLED)

// ***************************************
// HCR_EL2, Hypervisor Configuration Register (EL2), Page 2487 of AArch64-Reference-Manual.
// ***************************************

#define HCR_RW (1 << 31)
#define HCR_VALUE HCR_RW

// ***************************************
// SCR_EL3, Secure Configuration Register (EL3), Page 2648 of AArch64-Reference-Manual.
// ***************************************

#define SCR_RESERVED (3 << 4)
#define SCR_RW (1 << 10)
#define SCR_NS (1 << 0)
#define SCR_VALUE (SCR_RESERVED | SCR_RW | SCR_NS)

// ***************************************
// SPSR_EL3, Saved Program Status Register (EL3) Page 389 of AArch64-Reference-Manual.
// ***************************************

#define SPSR_MASK_ALL (7 << 6)
#define SPSR_EL1h (5 << 0)
#define SPSR_VALUE (SPSR_MASK_ALL | SPSR_EL1h)

// ***************************************
// CPACR_EL1, Architectural Feature Access Control Register (EL1) Section G8.2.32 of AArch64-Reference-Manual.
// ***************************************

#define CPACR_cp11 (0b11 << 20)
#define CPACR_VALUE CPACR_cp11

// ***************************************
// MAIR_EL1
// ***************************************

#define MAIR_nGnRnE 0b00
#define MAIR_DEVICE_ATTR (MAIR_nGnRnE << 2)
#define MAIR_NON_TRANSIENT (1 << 3)
#define MAIR_WRITE_BACK (1 << 2)
#define MAIR_NO_CACHE 0b0100
#define MAIR_OUTER_POLICY (MAIR_NO_CACHE)
#define MAIR_INNER_POLICY (MAIR_NO_CACHE)
#define MAIR_NORMAL_ATTR ((MAIR_OUTER_POLICY << 4) | (MAIR_INNER_POLICY))
#define MAIR_VALUE ((MAIR_DEVICE_ATTR << 8) | (MAIR_NORMAL_ATTR))

// ***************************************
// TCR_EL1
// ***************************************

#define TCR_T0SZ 33
#define TCR_IRGN0 (0b00 << 8)
#define TCR_ORGN0 (0b00 << 10)
#define TCR_SH0 (0b11 << 12)
#define TCR_TG0 (0b00 << 14)
#define TCR_HA (1 << 39)
#define TCR_VALUE ((TCR_TG0) | (TCR_SH0) | (TCR_ORGN0) | (TCR_IRGN0) | (TCR_T0SZ))

#endif
