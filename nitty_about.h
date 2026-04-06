/*
 * NiTTY fork attribution for About dialogs (shared by Windows and Unix).
 */

#ifndef NITTY_ABOUT_H
#define NITTY_ABOUT_H

#define NITTY_HOME_URL "https://github.com/iShark5060/NiTTY"

/*
 * Shown under the application name, before version and buildinfo.
 * Windows edit controls: use \251 for the copyright symbol (ANSI).
 */
#define NITTY_ABOUT_FORK_PARAGRAPH_WIN                                       \
    "NiTTY \251 Lutz Schwemer Panchez <shark@shark5060.net>\r\n"           \
    NITTY_HOME_URL "\r\n\r\n"                                              \
    "Based on PuTTY.\r\n\r\n"

/*
 * GTK label: UTF-8 sequence for U+00A9 ©
 */
#define NITTY_ABOUT_FORK_PARAGRAPH_UNIX                                      \
    "NiTTY \302\251 Lutz Schwemer Panchez <shark@shark5060.net>\n"         \
    NITTY_HOME_URL "\n\n"                                                  \
    "Based on PuTTY.\n\n"

#endif /* NITTY_ABOUT_H */
