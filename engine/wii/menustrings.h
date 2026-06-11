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

/* High-resolution mod render buffer picker (shown every launch) */
#define MENU_STR_HD_TITLE       "\xB8\xDF\xB7\xD6\xB1\xE6\xC2\xCA\xC4\xA3\xD7\xE9"
#define MENU_STR_HD_INFO        "\xC4\xA3\xD7\xE9\xC2\xDF\xBC\xAD %dx%d \xC7\xEB\xD1\xA1\xBD\xE7\xC3\xE6\xB7\xD6\xB1\xE6\xC2\xCA"
#define MENU_STR_HD_HINT        "\xC9\xCF\xCF\xC2\xD1\xA1\xD4\xF1   A \xC8\xB7\xC8\xCF\xC6\xF4\xB6\xAF"
#define MENU_STR_HD_OPT_FULL    "%dx%d  \xD4\xAD\xCA\xBC(\xC8\xAB\xC4\xDA\xB4\xE6)"
#define MENU_STR_HD_OPT_RENDER  "%dx%d  \xBD\xE7\xC3\xE6(\xCA\xA1\xC4\xDA\xB4\xE6)"

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
