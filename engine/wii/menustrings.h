/*
 * OpenBOR Wii menu strings (GBK encoded).
 * Nintendont-style: direct Chinese literals for launcher menu only.
 */

#ifndef MENUSTRINGS_H
#define MENUSTRINGS_H

/* Paks folder has no modules */
#define MENU_STR_NO_MODS        "Paks\xCE\xC4\xBC\xFE\xBC\xD0\xD6\xD0\xCE\xDE\xC4\xA3\xD7\xE9!"

/* Game list header: current index / total count */
#define MENU_STR_GAME_COUNT     "\xB9\xB2 %d/%d \xB8\xF6\xD3\xCE\xCF\xB7"

/* High-resolution mod memory warning (first launch per pak) */
#define MENU_STR_HD_TITLE       "\xB8\xDF\xB7\xD6\xB1\xE6\xC2\xCA\xC4\xA3\xD7\xE9"
#define MENU_STR_HD_INFO        "\xC4\xA3\xD7\xE9 %dx%d \xD5\xBC\xD3\xC3\xC4\xDA\xB4\xE6\xBD\xCF\xB4\xF3"
#define MENU_STR_HD_YES         "A: \xBD\xB5\xD6\xC1 320x240 (\xBD\xA8\xD2\xE9)"
#define MENU_STR_HD_NO          "B: \xB1\xA3\xB3\xD6\xB5\xB1\xC7\xB0\xB7\xD6\xB1\xE6\xC2\xCA"

/* Footer hints: "%s: ..." — key name inserted by printText */
#define MENU_STR_START_GAME     "%s: \xBF\xAA\xCA\xBC\xD3\xCE\xCF\xB7"
#define MENU_STR_BGM_PLAYER     "%s: \xD2\xF4\xC0\xD6\xB2\xA5\xB7\xC5"
#define MENU_STR_VIEW_LOGS      "%s: \xB2\xE9\xBF\xB4\xC8\xD5\xD6\xBE"
#define MENU_STR_QUIT_GAME      "%s: \xCD\xCB\xB3\xF6\xD3\xCE\xCF\xB7"

/* Log viewer */
#define MENU_STR_QUIT_HINT      "\xCD\xCB\xB3\xF6 : 1/B"
#define MENU_STR_LOG_NOT_FOUND  "\xCE\xB4\xD5\xD2\xB5\xBD\xC8\xD5\xD6\xBE: OpenBorLog.txt"
#define MENU_STR_SCRIPT_LOG_NF  "\xCE\xB4\xD5\xD2\xB5\xBD\xC8\xD5\xD6\xBE: ScriptLog.txt"

#endif
