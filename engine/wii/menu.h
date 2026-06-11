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
void wii_apply_hd_videomode_policy(int *mode, short *hres, short *vres);
void wii_hd_scale_videomode_down(int *videoMode);
extern int wii_hd_video_downgraded;
extern short wii_render_hres;
extern short wii_render_vres;
extern float wii_render_scale_h;
extern float wii_render_scale_v;
#endif

#endif

