/*
 * Copyright 2024 NXP
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file usb_host_init.c
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date May 15, 2026
 *
 * This file provides the necessary initialization for the High-Speed USB
 * Host controller on the FRDM-MCXN947. The standard board support in Zephyr
 * currently only performs this initialization when Device mode is enabled.
 */

#include <zephyr/init.h>
#include <zephyr/device.h>
#include <fsl_clock.h>
#include <fsl_spc.h>
#include <soc.h>

#define BOARD_XTAL0_CLK_HZ 24000000U

static int usb_host_clock_init(void)
{
	/* This logic is adapted from zephyr/boards/nxp/frdm_mcxn947/board.c
	 * It ensures that High-Speed USB clocks and power are enabled for Host mode.
	 */

	/* Active mode voltage for OverDrive mode is required for HS USB */
	SPC0->ACTIVE_VDELAY = 0x0500;
	/* Change the power DCDC to 1.8v (By default, DCDC is 1.8V), CORELDO to 1.1v (By default,
	 * CORELDO is 1.0V)
	 */
	SPC0->ACTIVE_CFG &= ~SPC_ACTIVE_CFG_CORELDO_VDD_DS_MASK;
	SPC0->ACTIVE_CFG |= SPC_ACTIVE_CFG_DCDC_VDD_LVL(0x3) | SPC_ACTIVE_CFG_CORELDO_VDD_LVL(0x3) |
			    SPC_ACTIVE_CFG_SYSLDO_VDD_DS_MASK | SPC_ACTIVE_CFG_DCDC_VDD_DS(0x2u);
	/* Wait until it is done */
	while (SPC0->SC & SPC_SC_BUSY_MASK) {
	};

	if (0u == (SCG0->LDOCSR & SCG_LDOCSR_LDOEN_MASK)) {
		SCG0->TRIM_LOCK = 0x5a5a0001U;
		SCG0->LDOCSR |= SCG_LDOCSR_LDOEN_MASK;
		/* wait LDO ready */
		while (0U == (SCG0->LDOCSR & SCG_LDOCSR_VOUT_OK_MASK)) {
		};
	}

	/* Enable USB HS and PHY clocks in AHB block */
	SYSCON->AHBCLKCTRLSET[2] |= SYSCON_AHBCLKCTRL2_USB_HS_MASK |
				    SYSCON_AHBCLKCTRL2_USB_HS_PHY_MASK;

	/* Setup System Oscillator for USB PHY */
	SCG0->SOSCCFG &= ~(SCG_SOSCCFG_RANGE_MASK | SCG_SOSCCFG_EREFS_MASK);
	/* xtal = 20 ~ 30MHz (FRDM-MCXN947 has 24MHz) */
	SCG0->SOSCCFG = (1U << SCG_SOSCCFG_RANGE_SHIFT) | (1U << SCG_SOSCCFG_EREFS_SHIFT);
	SCG0->SOSCCSR |= SCG_SOSCCSR_SOSCEN_MASK;
	while (1) {
		if (SCG0->SOSCCSR & SCG_SOSCCSR_SOSCVLD_MASK) {
			break;
		}
	}

	/* Enable clocks in SYSCON */
	SYSCON->CLOCK_CTRL |= SYSCON_CLOCK_CTRL_CLKIN_ENA_MASK |
			      SYSCON_CLOCK_CTRL_CLKIN_ENA_FM_USBH_LPT_MASK;

	/* Enable USB PHY PLL and USB clock */
	CLOCK_EnableClock(kCLOCK_UsbHs);
	CLOCK_EnableClock(kCLOCK_UsbHsPhy);
	CLOCK_EnableUsbhsPhyPllClock(kCLOCK_Usbphy480M, BOARD_XTAL0_CLK_HZ);
	CLOCK_EnableUsbhsClock();

	return 0;
}

/* Run before USB driver init (POST_KERNEL, 50) */
SYS_INIT(usb_host_clock_init, POST_KERNEL, 40);
