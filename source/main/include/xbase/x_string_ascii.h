#ifndef __XBASE_STRING_STD_H__
#define __XBASE_STRING_STD_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

//==============================================================================
// INCLUDES
//==============================================================================
#include "xbase/x_va_list.h"


namespace xcore
{
	namespace ascii
	{
		typedef			uchar					rune;
		typedef			uchar *					prune;
		typedef			const uchar*			pcrune;

		enum ECmpMode { CASE_SENSITIVE, CASE_IGNORE };

		pcrune			len(pcrune str);
		pcrune			len(pcrune str, s32* str_len);
		pcrune			len(pcrune str, pcrune str_eos, s32* str_len);

		prune			copy(prune dest, pcrune dest_end, pcrune src, pcrune src_end);

		pcrune			find(pcrune str, pcrune str_end, pcrune find, pcrune find_end, ECmpMode mode = CASE_SENSITIVE);		/// Return position of first occurrence of <inString> or -1 if not found
		pcrune			find_one_of(pcrune str, pcrune str_end, pcrune set, pcrune set_end);								/// Return position of first occurrence of a character in <inCharSet> after <inPos> or -1 if not found
		
		prune			replace(prune str_begin, pcrune str_end, pcrune str_eos, pcrune replace_str, pcrune replace_end);

		s32  			compare(pcrune str1, pcrune str1_end, pcrune str2, pcrune str2_end, ECmpMode mode = CASE_SENSITIVE);

		prune			concatenate(prune dst, pcrune dst_end, pcrune dst_eos, pcrune src, pcrune src_end);		// Concatenate strings, dst = dst + src

		s32				parse(pcrune str, pcrune str_end, bool& value);
		s32				parse(pcrune str, pcrune str_end, s32& value, s32 base = 10);
		s32				parse(pcrune str, pcrune str_end, u32& value, s32 base = 10);
		s32				parse(pcrune str, pcrune str_end, s64& value, s32 base = 10);
		s32				parse(pcrune str, pcrune str_end, u64& value, s32 base = 10);
		s32				parse(pcrune str, pcrune str_end, f32& value);
		s32				parse(pcrune str, pcrune str_end, f64& value);

		bool			is_decimal(pcrune str, pcrune str_end);
		bool			is_hexadecimal(pcrune str, pcrune str_end, bool with_prefix);
		bool			is_float(pcrune str, pcrune str_end);
		bool			is_GUID(pcrune str, pcrune str_end);

		prune			to_string(prune str, prune str_end, pcrune str_eos, s32 val, s32 base = 10);
		prune			to_string(prune str, prune str_end, pcrune str_eos, u32 val, s32 base = 10);
		prune			to_string(prune str, prune str_end, pcrune str_eos, s64 val, s32 base = 10);
		prune			to_string(prune str, prune str_end, pcrune str_eos, u64 val, s32 base = 10);
		prune			to_string(prune str, prune str_end, pcrune str_eos, f32 val, s32 num_fractional_digits = 2);
		prune			to_string(prune str, prune str_end, pcrune str_eos, f64 val, s32 num_fractional_digits = 2);

		inline bool		is_space(uchar32 c) { return((c == 0x09) || (c == 0x0A) || (c == 0x0D) || (c == ' ')); }
		inline bool		is_upper(uchar32 c) { return((c >= 'A') && (c <= 'Z')); }
		inline bool		is_lower(uchar32 c) { return((c >= 'a') && (c <= 'z')); }
		inline bool		is_alpha(uchar32 c) { return(((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))); }
		inline bool		is_digit(uchar32 c) { return((c >= '0') && (c <= '9')); }
		inline bool		is_number(uchar32 c) { return(((c >= 'A') && (c <= 'F')) || ((c >= 'a') && (c <= 'f')) || ((c >= '0') && (c <= '9'))); }

		inline uchar32	to_upper(uchar32 c) { return((c >= 'a') && (c <= 'z')) ? c + ('A' - 'a') : c; }
		inline uchar32	to_lower(uchar32 c) { return((c >= 'A') && (c <= 'Z')) ? c + ('a' - 'A') : c; }
		inline u32		to_digit(uchar32 c) { return ((c >= '0') && (c <= '9')) ? (c - '0') : c; }
		inline u32		to_number(uchar32 c) { if (is_digit(c)) return to_digit(c); else if (c >= 'A' && c <= 'F') return(c - 'A' + 10); else if (c >= 'a' && c <= 'f') return(c - 'a' + 10); else return(c); }
		inline bool		is_equal(uchar32 a, uchar32 b, ECmpMode mode = CASE_SENSITIVE) { if (mode == CASE_IGNORE) { a = to_lower(a); b = to_lower(b); } return (a == b); }

		bool			is_upper(pcrune str, pcrune str_end);											/// Check if string is all upper case
		bool			is_lower(pcrune str, pcrune str_end);											/// Check if string is all lower case
		bool			is_capitalized(pcrune str, pcrune str_end);										/// Check if string is capitalized ("Shanghai")
		bool			is_delimited(pcrune str, pcrune str_end, rune delimit_left = '\"', rune delimit_right = '\"');		///< Check if string is enclosed between left and right delimiter
		inline bool		is_quoted(pcrune str, pcrune str_end, rune quote = '\"')						{ return is_delimited(str, str_end, quote, quote); }

		prune			to_upper(prune str, pcrune str_end);
		prune			to_lower(prune str, pcrune str_end);

		bool			starts_with(pcrune str, pcrune str_end, uchar32 start_char);
		bool			starts_with(pcrune str, pcrune str_end, pcrune start_str, pcrune start_str_end);

		bool			ends_with(pcrune str, pcrune str_end, uchar32 end_char);
		bool			ends_with(pcrune srcstr, pcrune srcstr_end, pcrune endstr, pcrune endstr_end);

		uchar32			first_char(pcrune str, pcrune str_end);
		uchar32			last_char(pcrune str, pcrune str_end);
		
		s32				cprintf(pcrune format_str, pcrune format_str_end, X_VA_ARGS_16_DEF);
		s32				sprintf(prune dst_str, pcrune dst_str_end, pcrune format_str, pcrune format_str_end, X_VA_ARGS_16_DEF);

		s32				vcprintf(pcrune format_str, pcrune format_str_end, const x_va_list& args);
		s32				vsprintf(prune dst_str, pcrune dst_str_end, pcrune format_str, pcrune format_str_end, const x_va_list& args);

		s32				printf(pcrune format_str, pcrune format_str_end, X_VA_ARGS_16_DEF);
		s32				printf(pcrune format_str, pcrune format_str_end, const x_va_list& args);
		s32				printf(pcrune str, pcrune str_end);

		s32				sscanf(pcrune str, pcrune str_end, pcrune fmt_str, pcrune fmt_str_end, X_VA_R_ARGS_16_DEF);
		s32				vsscanf(pcrune str, pcrune str_end, pcrune fmt_str, pcrune fmt_str_end, const x_va_r_list& vr_args);

	}; ///< end of ascii namespace

	//==============================================================================
	// END xCore namespace
	//==============================================================================
};

#endif    ///< __X_STRING_STD_H__
