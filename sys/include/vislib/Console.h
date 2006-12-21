/*
 * Console.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 */

#ifndef VISLIB_CONSOLE_H_INCLUDED
#define VISLIB_CONSOLE_H_INCLUDED
#if (_MSC_VER > 1000)
#pragma once
#endif /* (_MSC_VER > 1000) */


#include "vislib/String.h"
#include "vislib/types.h"


namespace vislib {
namespace sys {


    /**
     * Class wrapping text console functionality and features
     */
    class Console {

    public:

        /** valid color values for console colors */
        enum ColorType {
            BLACK = 0x00,
            DARK_RED = 0x01,
            DARK_GREEN = 0x02,
            DARK_YELLOW = 0x03,
            DARK_BLUE = 0x04,
            DARK_MAGENTA = 0x05,
            DARK_CYAN = 0x06,
            GRAY = 0x07,
            DARK_GRAY = 0x08,
            RED = 0x09,
            GREEN = 0x0A,
            YELLOW = 0x0B,
            BLUE = 0x0C,
            MAGENTA = 0x0D,
            CYAN = 0x0E,
            WHITE = 0x0F,
            UNKNOWN_COLOR = 0x10
        };

        // TODO: documentation
        // TODO: Windows implementation.
        static int Run(const char *command, StringA *outStdOut = NULL, 
            StringA *outStdErr = NULL);

        /**
         * Write formatted text output to the standard output.
         *
         * @param fmt The string to write, possibly containing printf-style
         *            placeholders.
         * @param ... Values for the placeholders.
         */
        // TODO: Should Write throw a system exception in case of an error?
        // TODO: Should Write return a boolean to signal success?
        static void Write(const char *fmt, ...);

        /**
         * Write a line of formatted text output to the standard output. The 
         * linebreak will be added by the method.
         *
         * @param fmt The string to write, possibly containing printf-style
         *            placeholders.
         * @param ... Values for the placeholders.
         */
        static void WriteLine(const char *fmt, ...);

        /**
         * Answer if console is able to set color for output. If this methode
         * returns false, no color manipulation members will have any effect,
         * and all color getter members will return UNKNOWN_COLOR.
         *
         * @return true if the console supports colors, false otherwise
         */
        static bool ColorsAvailable(void);

        /**
         * Answer if color output is enabled.
         * See: "EnableColors" for more information.
         *
         * @return true if color output is enabled, false otherwise.
         */
        static bool ColorsEnabled(void);

        /**
         * Enables or disables color output. If "enable" is false, the color
         * output is disabled and all color manipulation member will have no
         * effect, and all color getter members will return UNKNOWN_COLOR. If
         * "enable" is true, the color output will be enabled if the console 
         * supports color output (see: "ColorsAvailable"). Colors are enabled
         * by default on consoles supporting color output.
         *
         * @param enable Flag whether to enable color output, or not.
         */
        static void EnableColors(bool enable);

        /**
         * Restores the console output colors to the default colors, if color
         * output is available and enabled. If the default colors are not 
         * available at program startup, this member will not set any colors.
         */
        static void RestoreDefaultColors(void);

        /**
         * Sets the foreground/text color of the output to fgcolor, if color
         * output is available and enabled. If fgcolor is UNKNOWN_COLOR, this 
         * member will not change the foreground color. If the console does not
         * support the color represented by the value of fgcolor, the most
         * similar looking color is set. In this situation following calls to
         * "GetForegroundColor" may return values different from fgcolor.
         * 
         * @param fgcolor The new foreground color.
         */
        static void SetForegroundColor(ColorType fgcolor);

        /**
         * Sets the background color of the output to fbcolor, if color output
         * is available and enabled. If bgcolor is UNKNOWN_COLOR, this member 
         * will not change the background color. If the console does not 
         * support the color represented by the value of bgcolor, the most
         * similar looking color is set. In this situation following calls to
         * "GetBackgroundColor" may return values different from bgcolor.
         * 
         * @param bgcolor The new background color.
         */
        static void SetBackgroundColor(ColorType bgcolor);

        /**
         * Answer the current foreground/text color, if color output is 
         * available and enabled. Otherwise UNKNOWN_COLOR is returned. However,
         * it is possible that UNKNOWN_COLOR is returned even if the console 
         * supports colors and the colors are enabled, when the console lacks 
         * the possibility of querying the current colors, or if necessary
         * system calls fail.
         *
         * @return The current foreground color.
         */
        static ColorType GetForegroundColor(void);

        /**
         * Answer the current backgroundt color, if color output is available 
         * and enabled. Otherwise UNKNOWN_COLOR is returned. However, it is 
         * possible that UNKNOWN_COLOR is returned even if the console supports
         * colors and the colors are enabled, when the console lacks the 
         * possibility of querying the current colors, or if necessary system 
         * calls fail.
         *
         * @return The current background color.
         */
        static ColorType GetBackgroundColor(void);

        /**
         * TODO: Document
         */
        static unsigned int GetWidth(void);

        /**
         * TODO: Document
         */
        static unsigned int GetHeight(void);

        /** Dtor. */
        ~Console(void);

    private:

        /** Disallow instances of this class. */
        Console(void);

        /** flag activating the color functions */
        static bool useColors;

        /** default foreground color */
        static ColorType defaultFgcolor;

        /** default background color */
        static ColorType defaultBgcolor;

#ifndef _WIN32

        /** forward declaration of internal helper class */
        class Curser;

        /** internal helper class */
        static Curser curser;
#endif // _WIN32

    };


} /* end namespace sys */
} /* end namespace vislib */

#endif /* VISLIB_CONSOLE_H_INCLUDED */

