#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_double.h"
#include "xbase/x_console.h"
#include "xbase/x_runes.h"
#include "xbase/x_printf.h"

namespace xcore
{
	struct printf_writer_t
	{
		printf_writer_t(runes_writer_t* w, bool write_to_console) : m_runes_writer(w), m_write_to_console(write_to_console), m_runes_count(0) {}
		runes_writer_t*		m_runes_writer;
		runez_t<utf32::rune, 64> m_write_to_console_cache;
		bool                m_write_to_console;
		s32                 m_runes_count;

		bool                write(uchar32 c)
		{
			bool result = false;
			if (m_runes_writer != nullptr)
			{
				result |= m_runes_writer->write(c);
			}
			else
			{
				m_runes_count += 1;
			}
			if (m_write_to_console)
			{
				m_write_to_console_cache += c;
			}
			return result;
		}
		
		bool                write(crunes_t const& runes)
		{
			bool result = false;
			if (m_runes_writer != nullptr)
			{
				result |= m_runes_writer->write(runes);
			}
			else
			{
				runes_reader_t reader(runes);
				while (!reader.at_end())
				{
					reader.skip();
					m_runes_count += 1;
				}
			}
			if (m_write_to_console)
			{
				flush();
				console->write(runes);
			}
			return result;
		}

		void flush()
		{
			if (m_write_to_console)
			{
				if (m_write_to_console_cache.size() > 0)
				{
					crunes_t cachestr = m_write_to_console_cache;
					console->write(cachestr);
					m_write_to_console_cache.reset();
				}
			}
		}
	};

//==============================================================================
// DEFINES
//==============================================================================

// --- FLAGS FOR VSPRINTF ---

// NUMERIC VALUES
#define LONGDBL 0x01  // long f64; unimplemented
#define LONGINT 0x02  // long integer
#define QUADINT 0x04  // quad integer
#define SHORTINT 0x08 // short integer
#define NUMBERMASK 0x0F

// OTHER FLAGS
#define ALT 0x10       // alternate form
#define HEXPREFIX 0x20 // add 0x or 0X prefix
#define LADJUST 0x40   // left adjustment
#define ZEROPAD 0x80   // zero (as opposed to blank) pad

// TEXT FLAGS
#define LOWERCASE 0x100 // lower-case, yes/no
#define UPPERCASE 0x200 // upper-case, YES/NO
#define CAMELCASE 0x400 // camel-case, Yes/No

// CONSTANTS
#define SPF_LONG_MAX 0x7FFFFFFF
#define WORKSIZE 128 // space for %c, %[diouxX], %[eEfgG]
#define DEFPREC 6    // number of precision for the real numbers

    //==============================================================================
    // FUNCTIONS
    //==============================================================================

    // pow for f64
    static f64 sPow(f64 x, s32 p)
    {
        if (p == 0)
            return 1.0;
        if (x == 0.0)
            return 0.0;

        if (p < 0)
        {
            p = -p;
            x = 1.0 / x;
        }

        f64 r = 1.0;
        while (true)
        {
            if (p & 1)
                r *= x;

            if ((p >>= 1) == 0)
                return r;

            x *= x;
        }
    }

#if defined(X_BIG_ENDIAN)
    struct ieee_double_shape_type
    {
        struct u32d
        {
            u32 msw;
            u32 lsw;
        };

        union {
            f64  value;
            u32d parts;
        };
    };
#else
    struct ieee_double_shape_type
    {
        struct u32d
        {
            u32 lsw;
            u32 msw;
        };

        union {
            f64  value;
            u32d parts;
        };
    };
#endif

// Get two 32 bit ints from a double.
#define EXTRACT_WORDS(ix0, ix1, d)   \
    do                               \
    {                                \
        ieee_double_shape_type ew_u; \
        ew_u.value = (d);            \
        (ix0)      = ew_u.parts.msw; \
        (ix1)      = ew_u.parts.lsw; \
    } while (0)

// Set a double from two 32 bit ints
#define INSERT_WORDS(d, ix0, ix1)    \
    do                               \
    {                                \
        ieee_double_shape_type iw_u; \
        iw_u.parts.msw = (ix0);      \
        iw_u.parts.lsw = (ix1);      \
        (d)            = iw_u.value; \
    } while (0)

    static const f64 one = 1.0;
    f64              modf(f64 x, f64* iptr)
    {
        s32 i0, i1, _j0;
        u32 i;
        EXTRACT_WORDS(i0, i1, x);
        _j0 = ((i0 >> 20) & 0x7ff) - 0x3ff; /* exponent of x */
        if (_j0 < 20)
        { /* integer part in high x */
            if (_j0 < 0)
            {                                            /* |x|<1 */
                INSERT_WORDS(*iptr, i0 & 0x80000000, 0); /* *iptr = +-0 */
                return x;
            }
            else
            {
                i = (0x000fffff) >> _j0;
                if (((i0 & i) | i1) == 0)
                { /* x is integral */
                    *iptr = x;
                    INSERT_WORDS(x, i0 & 0x80000000, 0); /* return +-0 */
                    return x;
                }
                else
                {
                    INSERT_WORDS(*iptr, i0 & (~i), 0);
                    return x - *iptr;
                }
            }
        }
        else if (_j0 > 51)
        { /* no fraction part */
            *iptr = x * one;
            /* We must handle NaNs separately.  */
            if (_j0 == 0x400 && ((i0 & 0xfffff) | i1))
                return x * one;
            INSERT_WORDS(x, i0 & 0x80000000, 0); /* return +-0 */
            return x;
        }
        else
        { /* fraction part in low x */
            i = ((u32)(0xffffffff)) >> (_j0 - 20);
            if ((i1 & i) == 0)
            { /* x is integral */
                *iptr = x;
                INSERT_WORDS(x, i0 & 0x80000000, 0); /* return +-0 */
                return x;
            }
            else
            {
                INSERT_WORDS(*iptr, i0, i1 & (~i));
                return x - *iptr;
            }
        }
    }

    /**
     *------------------------------------------------------------------------------
     * writer  - Where the output is written to
     * fpnum   - number which is going to be converted
     * cvt     - what type of conversion needs
     * width   - total width of the output number
     * prec    - how many digits of precision
     *------------------------------------------------------------------------------
     */
    static void dtoa(runes_writer_t* writer, f64 fpnum, char cvt, s32 width, s32 prec)
    {
        static const f64 powTable[] = {1, 10, 10e1, 10e2, 10e3, 10e4, 10e5, 10e6, 10e7, 10e8, 10e9, 10e10, 10e11, 10e12, 10e13, 10e14, 10e15, 10e16, 10e17, 10e18, 10e19, 10e20, 10e21, 10e22, 10e23};

        ASSERT(writer);
        ASSERT(prec >= 0);

        char  fwork[WORKSIZE];
        char* fw = fwork;

        // setup integer part
        char  iwork[WORKSIZE];
        char* iworkend = &iwork[sizeof(iwork) - 1];
        char* iw       = iworkend;
        *iw            = '\0';

        // setup exponent part
        char  ework[16];
        char* eworkend = &ework[sizeof(ework) - 1];
        ;
        char* ew = eworkend;
        *ew      = '\0';

        if (xf64::isInfinite(fpnum))
        {
            if (fpnum < 0)
                writer->write('-');
			crunes_t str("Inf", 3);
            writer->write(str);
            return;
        }

        if (xf64::isNAN(fpnum))
        {
            crunes_t str("NaN", 3);
            writer->write(str);
            return;
        }

        // grab sign & make non-negative
        s32 is_neg = fpnum < 0;
        if (is_neg)
            fpnum = -fpnum;

        // precision matters

        // can't have more prec than supported
        if (prec > WORKSIZE - 2)
            prec = WORKSIZE - 2;

        f64 powprec;
        if (prec == 6)
        {
            powprec = 1.0e6;
        }
        else
        {
            if (prec > 23)
            {
                powprec = sPow(10.0, prec);
            }
            else
            {
                powprec = powTable[prec];
            }
        }
        f64 rounder = 0.5 / powprec;

        s32 f_fmt = cvt == 'f' || ((cvt == 'g') && (fpnum == 0.0 || (fpnum >= 1e-4 && fpnum < powprec)));

        s32 iwidth = 0;
        s32 fwidth = 0;
        s32 ewidth = 0;

        if (f_fmt) // fixed format
        {
            f64 ipart;
            f64 fpart = modf(fpnum, &ipart);

            // convert fractional part
            if (fpart >= rounder || cvt != 'g')
            {
                f64 ifpart;
                s32 i;
                f64 ffpart;

                fpart += rounder;
                if (fpart >= 1.0)
                {
                    ipart += 1.0;
                    fpart -= 1.0;
                }

                ffpart = fpart;

                bool anySignificantInteger = xFALSE;
                for (i = 0; i < prec; ++i)
                {
                    ffpart = modf(ffpart * 10.0, &ifpart);
                    *fw++  = (char)('0' + (s32)(ifpart));

                    // This was added to keep prec number of significant digits
                    // When the user is looking for higher precision we will do this
                    if (prec > DEFPREC)
                    {
                        if (anySignificantInteger == xFALSE && ifpart == '0')
                            prec++;
                        else
                            anySignificantInteger = xTRUE;
                    }

                    ++fwidth;
                }

                if (cvt == 'g') // inhibit trailing zeroes if g-fmt
                {
                    char* p;
                    for (p = fw - 1; p >= fwork && *p == '0'; --p)
                    {
                        *p = 0;
                        --fwidth;
                    }
                }
            }

            // convert integer part
            if (ipart == 0.0)
            {
                // Removed this because I want to always see the zero I don't like things
                // that look like ".324234" I like "0.324234" to make sure we don't give
                // up precision I incremented the space for the number.
                prec++;
                // if (cvt != 'g' || fwidth < prec || fwidth < width)
                {
                    *--iw = '0';
                    ++iwidth;
                }
            }
            else
            {
                if (ipart <= (f64)(SPF_LONG_MAX)) // a useful speedup
                {
                    s32 li = (s32)(ipart);
                    while (li != 0)
                    {
                        *--iw = (char)('0' + (li % 10));
                        li    = li / 10;
                        ++iwidth;
                        ASSERT(iwidth < WORKSIZE);
                    }
                }
                else // the slow way
                {
                    while (ipart > 0.5)
                    {
                        f64 ff = modf(ipart / 10.0, &ipart);
                        ff     = (ff + 0.05) * 10.0;
                        *--iw  = (char)('0' + (s32)(ff));
                        ++iwidth;
                        ASSERT(iwidth < WORKSIZE);
                    }
                }
            }

            // g-fmt: kill part of frac if prec/width exceeded
            if (cvt == 'g')
            {
                s32 m = prec;
                s32 adj;

                if (m < width)
                    m = width;

                adj = iwidth + fwidth - m;
                if (adj > fwidth)
                    adj = fwidth;

                if (adj > 0)
                {
                    char* f;
                    for (f = &fwork[fwidth - 1]; f >= fwork && adj > 0; --adj, --f)
                    {
                        char ch = *f;

                        --fwidth;
                        *f = 0;

                        if (ch > '5') // properly round: unavoidable propagation
                        {
                            char* p;
                            s32   carry = 1;

                            for (p = f - 1; p >= fwork && carry; --p)
                            {
                                ++*p;

                                if (*p > '9')
                                    *p = '0';
                                else
                                    carry = 0;
                            }

                            if (carry)
                            {
                                for (p = iworkend - 1; p >= iw && carry; --p)
                                {
                                    ++*p;
                                    if (*p > '9')
                                        *p = '0';
                                    else
                                        carry = 0;
                                }

                                if (carry)
                                {
                                    *--iw = '1';
                                    ++iwidth;
                                    --adj;
                                    ASSERT(iwidth < WORKSIZE);
                                }
                            }
                        }
                    }

                    // kill any additional trailing zeros
                    {
                        char* p;
                        for (p = f; p >= fwork && *p == '0'; --p)
                        {
                            *p = 0;
                            --fwidth;
                        }
                    }
                }
            }
        }
        else // e-fmt
        {
            // normalize
            s32 exp = 0;

            while (fpnum >= 10.0)
            {
                fpnum *= 0.1;
                ++exp;
            }

            f64 almost_one = 1.0 - rounder;

            while (fpnum > 0.0 && fpnum < almost_one)
            {
                fpnum *= 10.0;
                --exp;
            }

            if (cvt == 'g') // used up one digit for s32 part...
            {
                --prec;
                powprec /= 10.0;
                rounder = 0.5 / powprec;
            }

            f64 ipart  = 0;
            f64 fpart  = modf(fpnum, &ipart);
            f64 ffpart = 0;
            f64 ifpart = 0;
            s32 i;
            // convert fractional part -- almost same as above
            if (fpart >= rounder || cvt != 'g')
            {
                fpart += rounder;

                if (fpart >= 1.0)
                {
                    fpart -= 1.0;
                    ipart += 1.0;

                    if (ipart >= 10.0)
                    {
                        ++exp;
                        ipart /= 10.0;
                        fpart /= 10.0;
                    }
                }

                ffpart = fpart;

                for (i = 0; i < prec; ++i)
                {
                    ffpart = modf(ffpart * 10.0, &ifpart);
                    *fw++  = (char)('0' + (s32)(ifpart));
                    ++fwidth;
                    ASSERT(fwidth < WORKSIZE);
                }

                if (cvt == 'g') // inhibit trailing zeroes if g-fmt
                {
                    char* p;
                    for (p = fw - 1; p >= fwork && *p == '0'; --p)
                    {
                        *p = 0;
                        --fwidth;
                        ASSERT(fwidth >= 0);
                    }
                }
            }

            // convert exponent
            bool const eneg = exp < 0;
            if (eneg)
                exp = -exp;

            while (exp > 0)
            {
                *--ew = (char)('0' + (exp % 10));
                exp /= 10;
                ++ewidth;
                ASSERT(ewidth < 16);
            }

            while (ewidth <= 2) // ensure at least 2 zeroes
            {
                *--ew = '0';
                ++ewidth;
                ASSERT(ewidth < 16);
            }

            *--ew = eneg ? '-' : '+';
            *--ew = 'e';

            ewidth += 2;

            // convert the one-digit integer part
            *--iw = (char)('0' + (s32)(ipart));
            ++iwidth;
        }

        // arrange everything in returned string
        s32 const showdot  = cvt != 'g' || fwidth > 0;
        s32 const fmtwidth = is_neg + iwidth + showdot + fwidth + ewidth;
        s32 const pad      = width - fmtwidth;

        for (s32 i = 0; pad > 0 && i < pad; ++i)
            writer->write(' ');

        if (is_neg)
        {
            writer->write('-');
        }
		crunes_t workstr(iw, iwidth);
        writer->write(workstr);
        if (showdot)
        {
            writer->write('.'); 
			crunes_t fworkstr(fwork, fwidth);
			writer->write(fworkstr);
        }
		crunes_t eworkstr(ew, ewidth);
		writer->write(eworkstr);
    }

    /**
     *------------------------------------------------------------------------------
     * Convert an unsigned long to ASCII for printf purposes, returning
     * a pointer to the first character of the string representation.
     * Octal numbers can be forced to have a leading zero; hex numbers
     * use the given digits.
     *------------------------------------------------------------------------------
     * val      - numeric value to be converted
     * writer   - writer for the destination string
     * base     - base of the number (10, 8, 16)
     * octzero  - flag to add to the oct numbers the 0 in front
     * xdigs    - hexadecimal string array of number 0,1,2,3,4,5,9,A,or a, ..etc
     *------------------------------------------------------------------------------
     */
#define to_digit(c) ((c) - '0')
#define is_digit(c) ((u32)to_digit(c) >= 0 && ((u32)to_digit(c) <= 9))
#define to_char(n) ((u32)((n) + '0'))

	static void ReverseCharStr(char* str, char* end)
	{
		// Reverse work buffer
		char* head = str;
		char* tail = end - 1;
		while (head < tail)
		{
			char t = *head;
			*head = *tail;
			*tail = t;
			head += 1;
			tail -= 1;
		}
	}

    static void ULtoA(u32 val, runes_writer_t* writer, s32 base, bool octzero, char const* xdigs)
    {
        uchar32 c;
        s32     sval;

        ASSERT(writer);

		char  work[WORKSIZE];
		char* w = &work[WORKSIZE];

        switch (base)
        {
            case 10:
                // On many machines, unsigned arithmetic is harder than
                // signed arithmetic, so we do at most one unsigned mod and
                // divide; this is sufficient to reduce the range of
                // the incoming value to where signed arithmetic works.

                if (val > SPF_LONG_MAX)
                {
                    c = to_char(val % 10);
					w -= 1;
					*w = c;
                    sval = (s32)(val / 10);
                }
                else
                {
                    sval = (s32)val;
                }

                do
                {
                    c = to_char(sval % 10);
					w -= 1;
					*w = c;
					sval /= 10;
                } while (sval != 0);

                break;

            case 8:
                do
                {
                    c = to_char(val & 7);
					w -= 1;
					*w = c;
                    val >>= 3;
                } while (val);
				if (octzero && c != '0')
				{
					*w-- = '0';
				}
                break;

            case 16:
                do
                {
					w -= 1;
					*w = xdigs[val & 15];
                    val >>= 4;
                } while (val);

                break;

            default:
                /* oops */
                break;
        }

		//ReverseCharStr(w, &work[WORKSIZE]);
		crunes_t workstr(w, &work[WORKSIZE]);
		writer->write(workstr);
    }

    /**
     * Same as above but for u64
     */
    static void UQtoA(u64 val, runes_writer_t* writer, s32 base, bool octzero, char const* xdigs)
    {
        uchar32 c;
        s64     sval;

		char  work[WORKSIZE];
		char* w = &work[WORKSIZE];

        switch (base)
        {
            case 10:
                // On many machines, unsigned arithmetic is harder than
                // signed arithmetic, so we do at most one unsigned mod and
                // divide; this is sufficient to reduce the range of
                // the incoming value to where signed arithmetic works.

                if (val > ((~(u64)0) >> 1))
                {
                    c = to_char(val % 10);
					w -= 1;
					*w = c;
                    sval = (s64)(val / 10);
                }
                else
                {
                    sval = (s64)val;
                }

                do
                {
                    c = to_char(sval % 10);
					w -= 1;
					*w = c;
					sval /= 10;
                } while (sval != 0);

                break;

            case 8:
                do
                {
                    c = to_char(val & 7);
					w -= 1;
					*w = c;
					val >>= 3;
                } while (val);

				if (octzero && c != '0')
				{
					c = '0';
					w -= 1;
					*w = c;
				}

                break;

            case 16:
                do
                {
                    c = xdigs[val & 15];
					w -= 1;
					*w = c;
					val >>= 4;
                } while (val);

                break;

            default:
                /* oops */
                break;
        }
	
		crunes_t workstr(w, &work[WORKSIZE]);
		writer->write(workstr);
	}

    static s32 boolToStr(u32 _boolean, runes_writer_t* writer, bool yesNo, xcore::s32 flags)
    {
        const char* t[] = {"false", "true", "FALSE", "TRUE", "False", "True", "no", "yes", "NO", "YES", "No", "Yes"};
        s32         i   = yesNo ? 6 : 0;
        if (flags & CAMELCASE)
            i += 4;
        else if (flags & UPPERCASE)
            i += 2;

        i += (_boolean == 0) ? 0 : 1;

        s32           len = 0;
        ascii::pcrune src = (ascii::pcrune)t[i];
        while (*src != '\0')
        {
            uchar32 c = *src++;
            writer->write(c);
            len += 1;
        }
        return len;
    }


    static void PadBuffer(printf_writer_t* writer, s32 howMany, char with)
    {
        for (s32 i = 0; i < howMany; i++)
        {
            if (writer->write(with) == false)
                break;
        }
    }

    ///< DOM-IGNORE-END

    /**
     *------------------------------------------------------------------------------
     * Summary:
     *     write a string formatted output into a buffer.
     * Arguments:
     *        buffer                - Storage location for output.
     *      maxChars            - Maximum number of characters to store
     *        pFormatStr            - String containing the formating specification.
     *      Args                - Pointer to list of arguments OR the actual arguments in case of x_sprintf
     * Returns:
     *        return the number of characters written, not including the terminating null character.
     * Description:
     *      These functions formats and stores a series of characters and values in buffer.
     *      Each argument (if any) is converted and output according to the corresponding format
     *      specification in format. The format consists of ordinary characters and has the same
     *      form and function as the format argument for x_printf. A null character is appended after
     *      the last character written. If copying occurs between strings that overlap, the behavior
     *      is undefined.
     *
     *<P><B>Type Field Characters</B>
     *<TABLE>
     *      Character   Output format
     *      =========   --------------------------------------------------------
     *      %%            a percent sign
     *      %b,%B,%#b     a boolean as true/false (%b=lower-case true/false, %B=upper-case TRUE/FALSE, %#b/%B#=capital-case True/False)
     *      %c            a character with the given number
     *      %s            a string
     *      %d            a signed integer, in decimal
     *      %u            an unsigned integer, in decimal
     *      %o            an unsigned integer, in octal
     *      %x            an unsigned integer, in hexadecimal (lower case)
     *      %e            a floating-point number, in scientific notation
     *      %f            a floating-point number, in fixed decimal notation
     *      %g            a floating-point number, in %e or %f notation
     *      %X            like %x, but using upper-case letters
     *      %E            like %e, but using an upper-case "E"
     *      %G            like %g, but with an upper-case "E" (if applicable)
     *      %p            a pointer (outputs the pointer value's address in hexadecimal)
     *      %n            special: *stores* the number of characters output so far into the next variable in the parameter list
     *      %y,%Y,%#y     a boolean as yes/no (%y=lower-case yes/no, %Y=upper-case YES/NO, %#y/%#Y=capital-case Yes/No)
     *</TABLE>
     *
     *   <B>size flags</B>
     *<TABLE>
     *     Character    Output
     *     =========    ------------------------------------------------------------
     *         h        interpret integer as s16 or u16
     *         l        interpret integer as s32 or u32
     *        q, L      interpret integer as s64 or u64
     *         L        interpret floating point as higher precision adds a few extra floating point numbers
     *</TABLE>
     *
     * Examples:
     *<CODE>
     *      x_printf("%Lf %Lf %Lf", Vec.X, Vec.Y, Vec.Z);   * Note that we use the higher precision floating point representation
     *      x_printf("%Ld" (s64)123);                       * This is a way to print an s64 bit number
     *      x_printf("%d" 123);                             * Nothing special here printing an 32bit integer
     *      x_printf("%f" 123.0);                           * Nothing special here printing a floating point number
     *      x_printf("%+010.4f", 123.456);                  * The printf works like you will expect
     *</CODE>
     *
     * See Also:
     *     x_printf x_printfxy
     *------------------------------------------------------------------------------
     */

    void VSPrintf_internal(printf_writer_t* writer, runes_reader_t* reader, runes_writer_t* buffer, const va_list_t& args)
    {
        ASSERT(reader != NULL);
        ASSERT(writer != NULL);
        ASSERT(buffer != NULL);

        uchar32     ch;        ///< character
        s32         n;         ///< handy integer (short term usage)
		runes_writer_t* cp = NULL; ///< handy char pointer (short term usage)
        s32         flags;     ///< flags as above
        s32         width;     ///< width from format (%8d), or 0
        s32         prec;      ///< precision from format (%.3d), or -1
        char        sign;      ///< sign prefix (' ', '+', '-', or \0)
        f64         _double;   ///< f64 precision arguments %[eEfgG]
        s32         base;      ///< base for [diouxX] conversion
        s32         dprec;     ///< a copy of prec if [diouxX], 0 otherwise
        s32         realsz;    ///< field size expanded by dprec, sign, etc
        s32         size;      ///< size of converted field or string

        /// Initialize variables
        char const* xdigs    = NULL; // digits for [xX] conversion
        u32         ulval    = 0;    // integer arguments %[diouxX]
        u64         uqval    = 0;    // %q integers
        s32         argindex = 0;

        //CharBufferToCharWriter copy_to_writer(writer);

        /// Scan the format for conversions (`%' character).
        for (;;)
        {
            while (true)
            {
                ch = reader->read();
                if ((ch == uchar32('\0')) || (ch == uchar32('%'))) // Find the first "interesting symbol"
                    break;
                writer->write(ch); // Write all the characters before the "interesting symbol"
            }

            // are we done?
            if (ch == '\0')
                goto done;

            // reset the temp buffer
            buffer->reset();

            // get ready for formating the info
            flags = 0;
            dprec = 0;
            width = 0;
            prec  = -1;
            size  = 0;
            sign  = '\0';

        rflag:
            ch = reader->read();

        reswitch:
            switch (ch)
            {
                case ' ':

                    // ``If the space and + flags both appear, the space
                    // flag will be ignored.''
                    //  -- ANSI X3J11
                    if (!sign)
                        sign = ' ';
                    goto rflag;

                case '#':
                    flags |= ALT;
                    flags |= CAMELCASE;
                    goto rflag;

                case '*':
                    // ``A negative field width argument is taken as a
                    // - flag followed by a positive field width.''
                    // -- ANSI X3J11
                    // They don't exclude field widths read from args.
                    {
                        const s32 w = (s32)args[argindex++];
                        if ((width = w) >= 0)
                            goto rflag;
                        width = -width;
                    }
                /////////>>>>>>>>>>>>>>>>>> FALLTHROUGH <<<<<<<<<<<<<<//////////
                case '-': flags |= LADJUST; goto rflag;

                case '+': sign = '+'; goto rflag;

                case '.':
                    ch = reader->read();
                    if (ch == '*')
                    {
                        n    = (s32)args[argindex++];
                        prec = n < 0 ? -1 : n;
                        goto rflag;
                    }

                    n = 0;
                    while (is_digit(ch))
                    {
                        n  = 10 * n + to_digit(ch);
                        ch = reader->read();
                    }

                    prec = n < 0 ? -1 : n;
                    goto reswitch;

                case '0':
                    // ``Note that 0 is taken as a flag, not as the
                    // beginning of a field width.''
                    // -- ANSI X3J11
                    flags |= ZEROPAD;
                    goto rflag;

                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    n = 0;
                    do
                    {
                        n  = 10 * n + to_digit(ch);
                        ch = reader->read();
                        if (!is_digit(ch))
                            break;
                    } while (true);

                    width = n;
                    goto reswitch;

                case 'L':
                    flags |= LONGDBL;

                    /////////>>>>>>>>>>>>>>>>>> FALLTHROUGH <<<<<<<<<<<<<<//////////
                case 'q': flags |= QUADINT; goto rflag;

                case 'h': flags |= SHORTINT; goto rflag;

                case 'l': flags |= LONGINT; goto rflag;

                case 'c':
                    buffer->write((uchar32)args[argindex++]);
                    size = 1;
                    sign = '\0';
                    break;

                case 'D':
                    flags |= LONGINT;
                    /////////>>>>>>>>>>>>>>>>>> FALLTHROUGH <<<<<<<<<<<<<<//////////

                case 'd':
                case 'i':
                    if (flags & QUADINT || args[argindex].isInt64() || args[argindex].isUInt64())
                    {
                        flags |= QUADINT;
                        s64 temp = (s64)args[argindex++];
                        if (temp < 0)
                        {
                            temp = -temp;
                            sign = '-';
                        }
                        uqval = (u64)temp;
                    }
                    else
                    {
                        s32 temp;
                        if (flags & SHORTINT)
                            temp = (s16)args[argindex++];
                        else
                            temp = (s32)args[argindex++];

                        if (temp < 0)
                        {
                            temp = -temp;
                            sign = '-';
                        }
                        ulval = (u32)(temp);
                    }

                    base = 10;
                    goto number;

                case 'g':
                case 'G':
                    if (prec == 0)
                        prec = 1;
                case 'e':
                case 'E':
                case 'f':
                    /////////>>>>>>>>>>>>>>>>>> FALLTHROUGH <<<<<<<<<<<<<<//////////

                    if (sign == '+')
                    {
                        writer->write(sign);
                        width--;
                    }

                    if (prec == -1)
                        prec = DEFPREC;

                    _double = (f64)args[argindex++];
                    if (flags & LONGDBL)
                    { // add additional precision when we say long f64
                        prec += 4;
                    }

                    if (flags & LADJUST)
                        width = -width;

                    // right-adjusting zero padding
                    {
                        dtoa(buffer, _double, (char)ch, width, prec);
						size = (s32)buffer->count();
                    }

                    // check whether we have to pad or not
                    if (flags & ZEROPAD)
                    {
						runes_t bufstr = buffer->get_current();
						runes_reader_t bufreader(bufstr);
						runes_writer_t bufwriter(bufstr);
						while (!bufreader.at_end())
						{
							uchar32 c = bufreader.read();
							if (c != ' ')
								break;
							bufwriter.write('0');
						}
                    }

                    // Copy 'buffer' to 'writer'
					{
						runes_t bufferstr = buffer->get_current();
						writer->write(bufferstr);
					}

                    if (flags & LADJUST)
                        PadBuffer(writer, -width - size, ' ');

                    continue;

                    ///////////////////// FLOATING_POINT //////////////////////
                case 'n':

                    // not supported

                    // no output
                    continue;

                    //////////////////////////////// THIS IS NOT ANSI STANDARD
                    // case 'O':
                    //    flags |= LONGINT;
                    //    /////////>>>>>>>>>>>>>>>>>> FALLTHROUGH <<<<<<<<<<<<<<//////////

                case 'o':
                    if (flags & QUADINT)
                        uqval = (u64)args[argindex++];
                    else if (flags & SHORTINT)
                        ulval = (u16)args[argindex++];
                    else
                        ulval = (u32)args[argindex++];

                    base = 8;
                    goto nosign;

                case 'p':
                    // "The argument shall be a pointer to void.  The
                    // value of the pointer is converted to a sequence
                    // of printable characters, in an implementation-
                    // defined manner."
                    // -- ANSI X3J11
#ifdef TARGET_PC
                    uqval = (u64)args[argindex++];
                    base  = 16;
                    xdigs = "0123456789abcdef";
                    flags |= QUADINT; // | HEXPREFIX; Not prefixes
                    ch = 'x';

                    // make sure that the precision is at 8
                    if (prec < 0)
                        prec = 8;
#else
                    ulval = (u32)args[argindex++];
                    base  = 16;
                    xdigs = "0123456789abcdef";
                    flags = (flags & ~QUADINT); // | HEXPREFIX; Not prefixes
                    ch    = 'x';

                    // make sure that the precision is at 8
                    if (prec < 0)
                        prec = 8;
#endif
                    goto nosign;

                case 's':
                    if (args[argindex].isPCRunes())
                    {
                        crunes_t src = (crunes_t)args[argindex];
						buffer->write(src);
                    }

                    size = (s32)buffer->count();
                    sign = '\0';
                    break;

                case 'B':
                    ulval = ((bool)args[argindex++]) ? 1 : 0;
                    size  = boolToStr(ulval, buffer, false, ((flags & ALT) != 0) ? CAMELCASE : UPPERCASE);
                    break;

                case 'b':
                    ulval = ((bool)args[argindex++]) ? 1 : 0;
                    size  = boolToStr(ulval, buffer, false, ((flags & ALT) != 0) ? CAMELCASE : 0);
                    break;

                case 'Y':
                    ulval = ((bool)args[argindex++]) ? 1 : 0;
                    size  = boolToStr(ulval, buffer, true, ((flags & ALT) != 0) ? CAMELCASE : UPPERCASE);
                    break;

                case 'y':
                    ulval = ((bool)args[argindex++]) ? 1 : 0;
                    size  = boolToStr(ulval, buffer, true, ((flags & ALT) != 0) ? CAMELCASE : 0);
                    break;

                case 'U':
                    flags |= LONGINT;
                    /////////>>>>>>>>>>>>>>>>>> FALLTHROUGH <<<<<<<<<<<<<<//////////

                case 'u':
                    if (flags & QUADINT)
                        uqval = (u64)args[argindex++];
                    else
                        ulval = (u32)args[argindex++];

                    base = 10;
                    goto nosign;

                case 'X': xdigs = "0123456789ABCDEF"; goto hex;

                case 'x':
                    xdigs = "0123456789abcdef";
                hex:
                    if (flags & QUADINT || args[argindex].type() == va_t::TYPE_INT64 || args[argindex].type() == va_t::TYPE_UINT64)
                    {
                        flags |= QUADINT;
                        uqval = (u64)args[argindex++];
                    }
                    else
                        ulval = (u32)args[argindex++];

                    base = 16;

                    // leading 0x/X only if non-zero
                    if (flags & ALT && (flags & QUADINT ? uqval != 0 : ulval != 0))
                        flags |= HEXPREFIX;

                    // unsigned conversions
                nosign:
                    sign = '\0';

                    // ``... diouXx conversions ... if a precision is
                    // specified, the 0 flag will be ignored.''
                    //  -- ANSI X3J11

                number:
                    if ((dprec = prec) >= 0)
                        flags &= ~ZEROPAD;

                    // ``The result of converting a zero value with an
                    // explicit precision of zero is no characters.''
                    // -- ANSI X3J11
					{
						if (flags & QUADINT)
						{
							if (uqval != 0 || prec != 0)
							{
								UQtoA(uqval, buffer, base, bool((flags & ALT) != 0), xdigs);
								size = (s32)buffer->count();
							}
						}
						else
						{
							if (ulval != 0 || prec != 0)
							{
								ULtoA(ulval, buffer, base, bool((flags & ALT) != 0), xdigs);
								size = (s32)buffer->count();
							}
						}
					}
                    break;

                default: // "%?" prints ?, unless ? is NUL
                    if (ch == '\0')
                        goto done;

                    // pretend it was %c with argument ch
                    buffer->write(ch);
                    size = 1;
                    sign = '\0';

                    break;
            }

            /**
             * All reasonable formats wind up here.  at this point, buffer
             * contains a string which (if not flags&LADJUST) should be
             * padded out to `width' places.  If flags&ZEROPAD, it should
             * first be prefixed by any sign or other prefix; otherwise,
             * it should be blank padded before the prefix is emitted.
             * After any left-hand padding and prefixing, emit zeroes
             * required by a decimal [diouxX] precision, then print the
             * string proper, then emit zeroes required by any leftover
             * floating precision; finally, if LADJUST, pad with blanks.
             */

            /**
             * Compute actual size, so we know how much to pad.
             * size excludes decimal prec; realsz includes it.
             */
            realsz = dprec > size ? dprec : size;

            if (sign)
                realsz++;
            else if (flags & HEXPREFIX)
                realsz += 2;

            // right-adjusting blank padding
            if ((flags & (LADJUST | ZEROPAD)) == 0)
                PadBuffer(writer, width - realsz, ' ');

            // prefix
            if (sign)
            {
				writer->write(sign);
            }
            else
            {
                if (flags & HEXPREFIX)
                {
					writer->write('0');
					writer->write(ch);
                }
            }

            // right-adjusting zero padding
            if ((flags & (LADJUST | ZEROPAD)) == ZEROPAD)
                PadBuffer(writer, width - realsz, '0');

            // leading zeroes from decimal precision
            PadBuffer(writer, dprec - size, '0');

            // write the integer number
			crunes_t bufferstr = buffer->get_current();
			writer->write(bufferstr);

            // left-adjusting padding (always blank)
            if (flags & LADJUST)
                PadBuffer(writer, width - realsz, ' ');
        }

    done:
        return;
    }

    s32 cprintf(crunes_t const& format, X_VA_ARGS_16)
    {
        va_list_t                                   args(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16);
		return vcprintf(format, args);
    }

    s32 vcprintf(crunes_t const& format, const va_list_t& args)
    {
		s32 len = 0;
		if (format.m_type == utf32::TYPE)
		{
			runez_t<utf32::rune, WORKSIZE> scratchbuffer;
			runes_writer_t scratch(scratchbuffer);
			runes_reader_t runesreader(format);
			printf_writer_t writer(nullptr, false);
			VSPrintf_internal(&writer, &runesreader, &scratch, args);
			len = writer.m_runes_count;
		}
		else if (format.m_type == ascii::TYPE)
		{
			runez_t<ascii::rune, WORKSIZE> scratchbuffer;
			runes_writer_t scratch(scratchbuffer);
			runes_reader_t reader(format);
			printf_writer_t writer(nullptr, false);
			VSPrintf_internal(&writer, &reader, &scratch, args);
			len = writer.m_runes_count;
		}
        return len;
    }

    void sprintf(runes_t& str, crunes_t const& format, X_VA_ARGS_16)
    {
        va_list_t                                   args(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16);
		vsprintf(str, format, args);
    }

    void vsprintf(runes_t& str, crunes_t const& format, const va_list_t& args)
    {
		if (format.m_type == utf32::TYPE)
		{
			runez_t<utf32::rune, WORKSIZE> scratchbuffer;
			runes_writer_t scratch(scratchbuffer);
			runes_reader_t runesreader(format);
			runes_writer_t runeswriter(str);
			printf_writer_t writer(&runeswriter, false);
			VSPrintf_internal(&writer, &runesreader, &scratch, args);
			str = writer.m_runes_writer->get_current();
		}
		else if (format.m_type == ascii::TYPE)
		{
			runez_t<utf32::rune, WORKSIZE> scratchbuffer;
			runes_writer_t scratch(scratchbuffer);
			runes_reader_t runesreader(format);
			runes_writer_t runeswriter(str);
			printf_writer_t writer(&runeswriter, false);
			VSPrintf_internal(&writer, &runesreader, &scratch, args);
			str = writer.m_runes_writer->get_current();
		}
	}

    void printf(crunes_t const& str)
	{
		switch (str.m_type)
		{
			case ascii::TYPE: return console->write(str.m_runes.m_ascii);
			case utf32::TYPE: return console->write(str.m_runes.m_utf32);
		}
	}

    void printf(crunes_t const& format, X_VA_ARGS_16)
    {
        va_list_t                                   args(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16);
		printf(format, args);
    }

    void printf(crunes_t const& format, const va_list_t& args)
    {
		s32 const                                   cache_size = 128;

		if (format.m_type == utf32::TYPE)
		{
			runez_t<utf32::rune, WORKSIZE> scratchbuffer;
			runes_writer_t scratch(scratchbuffer);
			runes_reader_t runesreader(format);
			printf_writer_t writer(nullptr, true);
			VSPrintf_internal(&writer, &runesreader, &scratch, args);
		}
		else if (format.m_type == ascii::TYPE)
		{
			runez_t<ascii::rune, WORKSIZE> scratchbuffer;
			runes_writer_t scratch(scratchbuffer);
			runes_reader_t runesreader(format);
			printf_writer_t writer(nullptr, true);
			VSPrintf_internal(&writer, &runesreader, &scratch, args);
		}
	}


}; // namespace xcore
