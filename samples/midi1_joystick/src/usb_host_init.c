/*
 * Copyright 2024-2026 NXP
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file usb_host_init.c
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date May 15, 2026
 *
 * This file provides manual initialization for USB Host controllers
 * on NXP MCX series boards. The standard board support in Zephyr
 * often gates these clocks unless Device mode is explicitly enabled.
 */

#include <zephyr/init.h>
#include <zephyr/device.h>
#include <fsl_clock.h>
#include <soc.h>

#ifdef CONFIG_SOC_FAMILY_MCXN
#include <fsl_spc.h>
#define BOARD_XTAL0_CLK_HZ 24000000U
#endif

static int usb_host_clock_init(void)
{
#ifdef CONFIG_SOC_FAMILY_MCXN
	/* MCXN947 High-Speed (EHCI) Initialization */
	
	/* Active mode voltage for OverDrive mode is required for HS USB */
	SPC0->ACTIVE_VDELAY = 0x0500;
	SPC0->ACTIVE_CFG &= ~SPC_ACTIVE_CFG_CORELDO_VDD_DS_MASK;
	SPC0->ACTIVE_CFG |= SPC_ACTIVE_CFG_DCDC_VDD_LVL(0x3) | SPC_ACTIVE_CFG_CORELDO_VDD_LVL(0x3) |
			    SPC_ACTIVE_CFG_SYSLDO_VDD_DS_MASK | SPC_ACTIVE_CFG_DCDC_VDD_DS(0x2u);
	while (SPC0->SC & SPC_SC_BUSY_MASK) { };

	if (0u == (SCG0->LDOCSR & SCG_LDOCSR_LDOEN_MASK)) {
		SCG0->TRIM_LOCK = 0x5a5a0001U;
		SCG0->LDOCSR |= SCG_LDOCSR_LDOEN_MASK;
		while (0U == (SCG0->LDOCSR & SCG_LDOCSR_VOUT_OK_MASK)) { };
	}

	SYSCON->AHBCLKCTRLSET[2] |= SYSCON_AHBCLKCTRL2_USB_HS_MASK |
				    SYSCON_AHBCLKCTRL2_USB_HS_PHY_MASK;

	SCG0->SOSCCFG &= ~(SCG_SOSCCFG_RANGE_MASK | SCG_SOSCCFG_EREFS_MASK);
	SCG0->SOSCCFG = (1U << SCG_SOSCCFG_RANGE_SHIFT) | (1U << SCG_SOSCCFG_EREFS_SHIFT);
	SCG0->SOSCCSR |= SCG_SOSCCSR_SOSCEN_MASK;
	while (!(SCG0->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK)) { };

	SYSCON->CLOCK_CTRL |= SYSCON_CLOCK_CTRL_CLKIN_ENA_MASK |
			      SYSCON_CLOCK_CTRL_CLKIN_ENA_FM_USBH_LPT_MASK;

	CLOCK_EnableClock(kCLOCK_UsbHs);
	CLOCK_EnableClock(kCLOCK_UsbHsPhy);
	CLOCK_EnableUsbhsPhyPllClock(kCLOCK_Usbphy480M, BOARD_XTAL0_CLK_HZ);
	CLOCK_EnableUsbhsClock();
#endif

	return 0;
}

/* Run before USB driver init (POST_KERNEL, 50) */
SYS_INIT(usb_host_clock_init, POST_KERNEL, 40);
