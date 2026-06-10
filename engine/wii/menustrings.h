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
