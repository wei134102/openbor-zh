/*
 * OpenBOR - https://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c)  OpenBOR Team
 */

#ifndef MENU_H
#define MENU_H

void Menu();

#if WII
void wii_apply_hd_videomode_policy(int *mode, int *hres, int *vres);
#endif

#endif

