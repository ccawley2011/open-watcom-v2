/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  This file defines the InfoBench specific functions.
*
****************************************************************************/

#include "whpcvt.h"
#include <stdio.h>
#include <assert.h>

#include "clibext.h"


#define MAX_LISTS       20

typedef enum {
    LPOSTFIX_NONE,
    LPOSTFIX_TERM,
} line_postfix;

// We use the escape char a lot...
#define STR_ESCAPE              "\x1B"
#define CHR_ESCAPE              (char)'\x1B'

// these are for the map_char_ib function
typedef enum {
    MAP_NONE,
    MAP_REMOVE,
    MAP_ESCAPE
} map_char_type;

// internal hyperlink symbol. Gets filtered out at output time
#define CHR_TEMP_HLINK          (char)'\x7F'

// this symbol separates the hyper-link label and topic. Other hyper-link
// related symbols are in whpcvt.h
#define CHR_HLINK               (char)'\xE0'
#define CHR_HLINK_BREAK         (char)'\xE8'

// Some characters we use for graphics
#define CHR_BULLET              (char)'\x07'
#define BOX_VBAR                (char)'\xB3'
#define BOX_HBAR                CH_BOX_HBAR
#define BOX_CORNER_TOP_LEFT     (char)'\xDA'
#define BOX_CORNER_TOP_RIGHT    (char)'\xBF'
#define BOX_CORNER_BOTOM_LEFT   (char)'\xC0'
#define BOX_CORNER_BOTOM_RIGHT  (char)'\xD9'

// InfoBench style codes
#define STR_BOLD_ON             STR_ESCAPE "b"
#define STR_UNDERLINE_ON        STR_ESCAPE "u"
#define STR_BOLD_OFF            STR_ESCAPE "p"
#define STR_UNDERLINE_OFF       STR_ESCAPE "w"

// labels for speacial header buttons
#define HB_UP                   " Up "
#define HB_PREV                 " <<<< "
#define HB_NEXT                 " >>>> "
#define HB_CONTENTS             " Contents "
#define HB_INDEX                " Index "
#define HB_KEYWORDS             " Keywords "

// Some stuff for tab examples:
#define MAX_TABS                100     // up to 100 tab stops

typedef enum {
    LIST_SPACE_COMPACT,
    LIST_SPACE_STANDARD,
} list_space;

typedef enum {
    LIST_TYPE_NONE,
    LIST_TYPE_UNORDERED,
    LIST_TYPE_ORDERED,
    LIST_TYPE_SIMPLE,
    LIST_TYPE_DEFN
} list_type;

typedef struct {
    list_type           type;
    int                 number;
    int                 prev_indent;
    list_space          compact;
} list_def;

static unsigned         Tab_list[MAX_TABS];
static int              tabs_num = 0;

// turns off bold and underline
static char Reset_Style[] = STR_BOLD_OFF STR_UNDERLINE_OFF;

static list_def         Lists[MAX_LISTS] = {
    { LIST_TYPE_NONE,   0,      0,  LIST_SPACE_COMPACT },       // list base
};
static int              List_level = 0;
static list_def         *Curr_list = &Lists[0];

static bool             Blank_line = false;
static int              Curr_indent = 0;
static bool             Eat_blanks = false;

// The following are for word-wrapping and indentation support
static int              Cursor_X = 0;   // column number
static int              R_Chars = 0;    // visible chars since Wrap_Safe
static size_t           Wrap_Safe = 0;  // index of break candidate
static int              NL_Group = 0;   // Number of contiguous newlines

static bool             Box_Mode = false;

static line_postfix     Line_postfix = LPOSTFIX_NONE;

static void warning( char *msg, unsigned int line )
/*************************************************/
{
    printf( "*** WARNING: %s on line %d.\n", msg, line );
}

static void set_compact( char *line )
/***********************************/
{
    ++line;
    if( *line == 'c' ) {
        /* compact list */
        Curr_list->compact = LIST_SPACE_COMPACT;
    } else {
        Curr_list->compact = LIST_SPACE_STANDARD;
    }
}

// this function will change all of the spaces in a string into non-breaking
// spaces (character 0xFF ). It's currently only used for the labels on
// hyper-links to ensure that they do not get broken across lines as
// this is not allowed by the InfoBench grammar.
static void to_nobreak( char *str )
/*********************************/
{
    if( str != NULL ) {
        for( ; *str != '\0'; ++str ) {
            if( *str == ' ' ) {
                *str = CH_SPACE_NOBREAK;
            }
        }
    }
}

/* This function will return CHR_REMOVE if the character is not allowed in
 * InfoBench, null if the character is okay. Any other values are prefixes
 * used to escape the character.
 */
static map_char_type map_char_ib( char ch )
/*****************************************/
{
    map_char_type   map_type;

    switch( ch ) {
    // The following characters should be preceded by an ESC character
    // Some could also be preceded by themselves, but this leads to
    // ambiguity. ie: what does <<< mean?
    case '}':
    case '{':
    case '<':
    case '>':
    case '"':
        map_type = MAP_ESCAPE;
        break;
    // The following characters are special to InfoBench, and there is no
    // way to represent them with the current grammar
    case CHR_HLINK:
    case CHR_HLINK_BREAK:
    case CHR_ESCAPE:
        map_type = MAP_REMOVE;
        break;
    default:
        map_type = MAP_NONE;
        break;
    }
    return( map_type );
}

/* This function will do proper escaping or character substitution for
 * characters special to InfoBench and add them to the section text.
 */
static size_t trans_add_char_ib( char ch, section_def *section, size_t *size )
/****************************************************************************/
{
    size_t          len = 0;
    map_char_type   map_type;

    map_type = map_char_ib( ch );
    if( map_type == MAP_REMOVE ) {
        return( 0 );
    } else if( map_type == MAP_ESCAPE ) {
        len += trans_add_char( CHR_ESCAPE, section, size );
    }
    len += trans_add_char( ch, section, size );
    return( len );
}

/* This function will do proper escaping or character substitution for
 * characters special to InfoBench and send them to the output stream.
 */
static void str_out_ib( FILE *f, const char *str )
/************************************************/
{
    map_char_type   map_type;

    if( str != NULL ) {
        for( ; *str != '\0'; ++str ) {
            map_type = map_char_ib( *str );
            if( map_type == MAP_REMOVE )
                continue;
            if( map_type == MAP_ESCAPE ) {
                whp_fprintf( f, "%c", CHR_ESCAPE );
            }
            whp_fwrite( str, 1, 1, f );
        }
    }
}

// The following two functions handle word-wrapping
static size_t trans_add_char_wrap( char ch, section_def *section, size_t *size )
/******************************************************************************/
{
    int                 ctr;            // misc. counter
    size_t              wrap_amount;    // amount of wrapped text
    int                 shift;          // amount we need to shift text
    int                 delta;          // amount of whitespace we ignore
    int                 indent;         // actual indent
    size_t              len = 0;        // number of chars we just added

    // the "1" is because a character is allowed to appear on the right margin
    // the minus part is so that we leave enough room for the box chars
    #define MY_MARGIN ( Right_Margin + 1 - ( Box_Mode ? 2 : 0 ) )

    // find out the real indentation
    indent = ( Curr_indent < 0 ) ? 0 : Curr_indent;

    // add character
    if( ch == '\n' ) {
        NL_Group++;
        if( Box_Mode ) {
            // Add left side if necessary:
            for( ; Cursor_X < indent; Cursor_X++ ) {
                trans_add_char( ' ', section, size );
            }
            if( Cursor_X == indent ) {
                trans_add_char( BOX_VBAR, section, size );
                Cursor_X++;
            }

            // Now add right side:
            // the "- 1" is for the BOX_VBAR
            for( ; Cursor_X < Right_Margin - 1; Cursor_X++ ) {
                trans_add_char( ' ', section, size );
            }
            // Now add vertical bar if room (there should be...)
            if( Cursor_X == Right_Margin - 1 ) {
                trans_add_char( BOX_VBAR, section, size );
            }
        }
        R_Chars = 0;
        Cursor_X = 0;
        Wrap_Safe = section->section_size;
    } else {
        // We assume all other characters will advance one char by default
        R_Chars++;
        Cursor_X++;
    }

    // now we actually add the character to our buffer
    len = trans_add_char_ib( ch, section, size );

    // adjust the nearest safe break point if the char we got was a space and
    // is not preceded by a space
    if( ch == ' ' && section->section_size > 2 && section->section_text[section->section_size - 2] != ' ' ) {
        Wrap_Safe = section->section_size;
        R_Chars = 0;
    }

    // If a bunch of spaces are pushing us over the edge then we'll rip all
    // of them off except one. Only happens in extreme cases. (ie: wdbg.whp)
    if( Cursor_X >= MY_MARGIN && ch == ' ' ) {
        while( section->section_size > 2 && section->section_text[section->section_size - 2] == ' ' ) {
            section->section_size--;
            Cursor_X--;
        }
    }

    // if this assertion fails then the wrapping code will break. Hopefully
    // this cannot happen...
    assert( Cursor_X <= MY_MARGIN );

    // if we need to wrap...
    if( Cursor_X == MY_MARGIN ) {
        // find out how many characters we need to move to next line
        if( R_Chars == MY_MARGIN - indent ) {
            // if the current word won't even fit on the next line then we
            // break it at the margin, so just wrap the current char
            R_Chars = 1;
            wrap_amount = len;
            Wrap_Safe = section->section_size - wrap_amount;
        } else {
            wrap_amount = section->section_size - Wrap_Safe;
        }

        // By this point we know exactly how many chars we need to move to
        // the next line. Now to figure out how to move them:
        // calculate shift
        shift = indent + 1;         // for the indent spaces and for the newline char

        // if we're in box mode we need some extra space for the
        // spaces and box drawing chars
        if( Box_Mode ) {
            shift += R_Chars + 3;   // trailing spaces to add plus 2 vertical bars and 1 space
        } else {
            // remove existing trailing spaces when not in box mode
            delta = 0;
            while( 1 + delta < Wrap_Safe && section->section_text[Wrap_Safe - 1 - delta] == ' ' ) {
                delta++;
            }
            shift -= delta;
        }

        // figure out which way the text has to move, and tweak the section
        if( shift > 0 ) {
            // add some padding so we can shift the chars over
            for( ctr = 0; ctr < shift; ctr++ ) {
                trans_add_char( 'X', section, size );
            }
        } else {
            // we have to decrease the section size accordingly
            section->section_size += shift;
        }

        // shift the chars over
        memmove( section->section_text + Wrap_Safe + shift, section->section_text + Wrap_Safe, wrap_amount );

        // fill "vacated" region with spaces (if there was one)
        if( shift > 0 ) {
            memset( section->section_text + Wrap_Safe, ' ', shift );
            // if shift is negative then the region we'll be working with
            // should already be filled with spaces.
        }

        // add newline before text we just shifted to break the line
        ctr = Wrap_Safe + shift - indent - 1;
        if( Box_Mode ) {
            // when in box mode we've added two chars for the vertical bar
            // and a leading space. Need to make sure the newline goes
            // before them...
            ctr -= 2;
        }
        *(section->section_text + ctr) = '\n';

        // if we're in Box_Mode then we also add the vertical bars
        if( Box_Mode ) {
            *(section->section_text + ctr - 1) = BOX_VBAR;
            *(section->section_text + Wrap_Safe + shift - 2) = BOX_VBAR;
        }

        // reset cursor x position
        Cursor_X = indent + R_Chars + ( Box_Mode ? 2 : 0 );

        // Set next safe wrappable point to beginning of text on this line
        Wrap_Safe += indent + 1;

        if( wrap_amount == len ) {
            R_Chars = 1;
        }
    }
    return( len );
}

static size_t trans_add_str_wrap( const char *str, section_def *section, size_t *size )
/*************************************************************************************/
{
    size_t      len = 0;

    for( ; *str != '\0'; ++str ) {
        len += trans_add_char_wrap( *str, section, size );
    }
    return( len );
}

static void new_list( char chtype )
/*********************************/
{
    list_type   type;

    ++List_level;
    if( List_level == MAX_LISTS ) {
        error( ERR_MAX_LISTS, true );
    }
    Curr_list = &Lists[List_level];
    switch( chtype ) {
    case CH_LIST_START:
        type = LIST_TYPE_UNORDERED;
        break;
    case CH_DLIST_START:
        type = LIST_TYPE_DEFN;
        break;
    case CH_OLIST_START:
        type = LIST_TYPE_ORDERED;
        break;
    case CH_SLIST_START:
        type = LIST_TYPE_SIMPLE;
        break;
    default:
        type = LIST_TYPE_NONE;
        break;
    }
    Curr_list->type = type;
    Curr_list->number = 1;
    Curr_list->prev_indent = Curr_indent;
    Curr_list->compact = LIST_SPACE_STANDARD;
}

static void pop_list( void )
/**************************/
{
    Curr_indent = Curr_list->prev_indent;
    --List_level;
    Curr_list = &Lists[List_level];
}

static void read_tabs( char *tab_line )
/*************************************/
{
    char        *ptr;
    unsigned    tabcol;

    Tab_xmp_char = *tab_line;
    tabs_num = 0;
    tabcol = 0;
    for( ptr = strtok( tab_line + 1, " " ); ptr != NULL; ptr = strtok( NULL, " " ) ) {
        if( *ptr == '+' ) {
            tabcol += atoi( ptr + 1 );
        } else {
            tabcol = atoi( ptr );
        }
        Tab_list[tabs_num++] = tabcol;
    }
}

static size_t tab_align( size_t ch_len, section_def *section, size_t *size )
/**************************************************************************/
{
    int         i;
    size_t      len;
    size_t      j;

    // find the tab we should use
    len = 1;
    for( i = 0; i < tabs_num; i++ ) {
        if( Tab_list[i] > ch_len ) {
            len = Tab_list[i] - ch_len;
            break;
        }
    }
    for( j = len; j > 0; j-- ) {
        trans_add_char_wrap( ' ', section, size );
    }
    return( len );
}

void ib_topic_init( void )
/************************/
{
}

size_t ib_trans_line( section_def *section, size_t size )
/*******************************************************/
{
    char                *ptr;
    char                *end;
    char                ch;
    char                *ctx_name;
    char                *ctx_text;
    char                buf[100];
    int                 indent = 0;
    int                 ctr;
    char                *file_name;
    size_t              len;

    // check for special pre-processing stuff first
    ptr = Line_buf;
    ch = *ptr;

    // We start at a new line...
    Wrap_Safe = section->section_size;
    Cursor_X = 0;
    R_Chars = 0;

    if( Blank_line && ( ch != CH_LIST_ITEM || Curr_list->compact != LIST_SPACE_COMPACT ) ) {
        Blank_line = false;
    }
    switch( ch ) {
    case CH_TABXMP:     // Tabbed-example
        if( *skip_blank( ptr + 1 ) == '\0' ) {
            Tab_xmp = false;
        } else {
            read_tabs( ptr + 1 );
            Tab_xmp = true;
        }
        return( size );
    case CH_BOX_ON:     // Box-mode start
        // indent properly
        for( ctr = 0; ctr < Curr_indent; ctr++ ) {
            trans_add_char( ' ', section, &size);
        }
        // draw the top line of the box
        trans_add_char( BOX_CORNER_TOP_LEFT, section, &size );
        for( ctr = 0; ctr < Right_Margin - Curr_indent - 2; ctr++ ) {
            trans_add_char( BOX_HBAR, section, &size );
        }
        trans_add_char( BOX_CORNER_TOP_RIGHT, section, &size );
        trans_add_char_wrap( '\n', section, &size);
        Box_Mode = true;
        return( size );
    case CH_BOX_OFF:    // Box-mode end
        for( ctr = 0; ctr < Curr_indent; ctr++ ) {
            trans_add_char( ' ', section, &size);
        }
        trans_add_char( BOX_CORNER_BOTOM_LEFT, section, &size );
        for( ctr = 0; ctr < Right_Margin - Curr_indent - 2; ctr++ ) {
            trans_add_char( BOX_HBAR, section, &size );
        }
        trans_add_char( BOX_CORNER_BOTOM_RIGHT, section, &size );
        Box_Mode = false;
        trans_add_char_wrap( '\n', section, &size );
        return( size );
    case CH_LIST_START:
    case CH_DLIST_START:
    case CH_OLIST_START:
    case CH_SLIST_START:
        indent = Text_Indent;
        if( ch == CH_SLIST_START ) {
            /* nested simple lists, with no pre-indent. Force an indent */
            if( Curr_list->type != LIST_TYPE_SIMPLE ) {
                indent = 0;
            }
        }
        new_list( ch );
        set_compact( ptr );
        Curr_indent += indent;
        if( ch == CH_DLIST_START ) {
            ptr = skip_blank( ptr + 1 );
            if( *ptr != '\0' ) {
                /* due to a weakness in GML, the definition term must be
                   allowed on the same line as the definition tag. So
                   if its there, continue */
                break;
            }
        }
        return( size );
    case CH_LIST_END:
    case CH_DLIST_END:
    case CH_OLIST_END:
    case CH_SLIST_END:
        pop_list();
        return( size );
    case CH_DLIST_TERM:
        Curr_indent -= Text_Indent;
        break;
    case CH_DLIST_DESC:
        Curr_indent += Text_Indent;
        if( *skip_blank( ptr + 1 ) == '\0' ) {
            /* no description on this line. Ignore it so that no
               blank line gets generated */
            return( size );
        }
        break;
    case CH_CTX_KW:
        ptr = whole_keyword_line( ptr );
        if( ptr == NULL ) {
            return( size );
        }
        break;
    }

    // skip preceding blank lines
    if( *skip_blank( ptr ) == '\0' && Curr_ctx->empty ) {
        return( size );
    }

    // remove '\n' on the end
    if( Blank_line ) {
        --section->section_size;
    }

    // indent properly if the first char is not white-space
    if( ch != '\0' && ch != ' ' && ch != '\t') {
        ctr = ( ch == CH_LIST_ITEM && !Box_Mode && Curr_list->type != LIST_TYPE_SIMPLE ) ? Text_Indent : 0;
        for( ; ctr < Curr_indent; ctr++ ) {
            trans_add_char_wrap( ' ', section, &size);
        }

        if( Box_Mode ) {
            trans_add_char_wrap( BOX_VBAR, section, &size);
            trans_add_char_wrap( ' ', section, &size);
        }
    }

    Blank_line = true;
    for( ;; ) {
        ch = *ptr;
        if( ch != '\0' && ch != ' ' && ch != '\t' ) {
            Blank_line = false;
        }
        if( ch == '\0' ) {
            // this just shuts off bolding after a def. list term
            if( Line_postfix == LPOSTFIX_TERM ) {
                Line_postfix = LPOSTFIX_NONE;
                trans_add_str( STR_BOLD_OFF, section, &size );
            }
            trans_add_char_wrap( '\n', section, &size );
            break;
        } else if( ch == CH_HLINK || ch == CH_DFN || ch == CH_FLINK ) {
            Curr_ctx->empty = false;
            if( ch == CH_FLINK ) {
                file_name = strchr( ptr + 1, ch );
                if( file_name == NULL ) {
                    error( ERR_BAD_LINK_DFN, true );
                }
                *file_name = '\0';
            } else {
                file_name = ptr;
            }
            ctx_name = strchr( file_name + 1, ch );
            if( ctx_name == NULL ) {
                error( ERR_BAD_LINK_DFN, true );
            }
            *ctx_name = '\0';

            ctx_text = strchr( ctx_name + 1, ch );
            if( ctx_text == NULL ) {
                error( ERR_BAD_LINK_DFN, true );
            }
            *ctx_text = '\0';

            ctx_text = ctx_name + 1;
            ctx_name = file_name + 1;
            file_name = ptr + 1;
            if( ch != CH_FLINK ) {
                add_link( ctx_name );
            }

            ptr = ctx_text + strlen( ctx_text ) + 1;

            // Definition pop-up's are converted to hyper-links in InfoBench
            trans_add_char( CHR_TEMP_HLINK , section, &size );

            // Add line number to hyperlink so we can give meaningful errors
            trans_add_str( itoa( Line_num, buf, 10 ), section, &size );
            trans_add_char( CHR_TEMP_HLINK, section, &size );

            // We don't want links to break as IB doesn't like this...
            to_nobreak( ctx_text );

            indent = ( Curr_indent < 0 ) ? 0 : Curr_indent;
            // find out the maximum allowed length for hyper-link text:
            ctr = Right_Margin - indent - ( ( IB_Hyper_Brace_L == '<' ) ? 2 : 0 );

            // if the link name is too long then we warn & truncate it
            if( strlen( ctx_text ) > ctr ) {
                warning( "Hyperlink name too long", Line_num );
                ctx_text[ctr] = '\0';
            }

            /* If hyper-link bracing is on we have to do a kludge to fix
             * the spacing. The "XX" will make the wrap routine happy.
             * They're stripped off when it comes time to write the file.
             */
            if( IB_Hyper_Brace_L == '<' ) {
                trans_add_str_wrap( "XX", section, &size );
            }
            trans_add_str_wrap( ctx_text, section, &size );
            trans_add_char( CHR_HLINK_BREAK , section, &size );
            trans_add_str( ctx_name, section, &size );
            if( ch == CH_FLINK ) {
                trans_add_char( CHR_HLINK_BREAK, section, &size );
                trans_add_str( file_name, section, &size );
            }
            trans_add_char( CHR_TEMP_HLINK , section, &size );
        } else if( ch == CH_LIST_ITEM ) {
            if( Curr_list->type != LIST_TYPE_SIMPLE ) {
                buf[0] = '\0';
                if( Curr_list->type == LIST_TYPE_UNORDERED ) {
                    // generate a bullet, correctly spaced for tab size
                    for( ctr = 0; ctr < Text_Indent; ctr++ ) {
                        strcat( buf, " " );
                    }
                    buf[Text_Indent / 2 - 1] = CHR_BULLET;
                } else if( Curr_list->type == LIST_TYPE_ORDERED ) {
                    /* ordered list type */
                    sprintf( buf, "%*d. ", Text_Indent - 2, Curr_list->number );
                    ++Curr_list->number;
                }
                trans_add_str_wrap( buf, section, &size );
            }
            Eat_blanks = true;
            ptr = skip_blank( ptr + 1 );
        } else if( ch == CH_DLIST_DESC ) {
            ptr = skip_blank( ptr + 1 );
        } else if( ch == CH_DLIST_TERM ) {
            /* definition list term */
            trans_add_str( STR_BOLD_ON, section, &size );
            Line_postfix = LPOSTFIX_TERM;
            ptr = skip_blank( ptr + 1 );
            Eat_blanks = true;
        } else if( ch == CH_CTX_KW ) {
            end = strchr( ptr + 1, CH_CTX_KW );
            len = end - ptr - 1;
            memcpy( buf, ptr + 1, len );
            buf[len] = '\0';
            add_ctx_keyword( Curr_ctx, buf );
            ptr = end + 1;
            if( *ptr == ' ' ) {
                /* kludge fix cuz of GML: GML thinks that keywords are
                   are real words, so it puts a space after them.
                   This should fix that */
                ++ptr;
            }
        } else if( ch == CH_PAR_RESET ) {
            // we ignore paragraph resets
            ++ptr;
        } else if( ch == CH_BMP ) {
            // we ignore bitmaps
            ptr = strchr( ptr + 3, CH_BMP ) + 1;
        } else if( ch == CH_FONTSTYLE_START ) {
            ++ptr;
            end = strchr( ptr, CH_FONTSTYLE_START );
            for( ; ptr != end; ++ptr ) {
                switch( *ptr ) {
                // bold and italic map to bold
                case 'b':
                case 'i':
                    trans_add_str( STR_BOLD_ON, section, &size );
                    break;
                // underline and underscore map to underline
                case 'u':
                case 's':
                    trans_add_str( STR_UNDERLINE_ON, section, &size );
                    break;
                }
            }
            ++ptr;
        } else if( ch == CH_FONTSTYLE_END ) {
            // reset style (bold off, underline off)
            trans_add_str( Reset_Style, section, &size );
            ++ptr;
        } else if( ch == CH_FONTTYPE ) {
            // we basically ignore font type changes
            ptr = strchr( strchr( ptr + 1 , CH_FONTTYPE ) + 1, CH_FONTTYPE ) + 1;
        } else {
            ++ptr;
            if( !Eat_blanks || ch != ' ' ) {
                Curr_ctx->empty = false;
                if( Tab_xmp && ch == Tab_xmp_char ) {
                    size_t      ch_len;

                    indent = ( Curr_indent < 0 ) ? 0 : Curr_indent;
                    // find out how close we are to "col 0" for the current indent
                    ch_len = Cursor_X - indent - ( Box_Mode ? 2 : 0 );
                    tab_align( ch_len, section, &size );
                    ptr = skip_blank( ptr );
                } else {
                    trans_add_char_wrap( ch, section, &size );
                }
                Eat_blanks = false;
            }
        }
    }

    return( size );
}

static void find_browse_pair( ctx_def *ctx, ctx_def **prev, ctx_def **next )
/**************************************************************************/
{
    browse_ctx                  *curr_ctx;
    browse_ctx                  *prev_ctx;
    browse_ctx                  *next_ctx;
    browse_def                  *browse_list;

    for( browse_list = Browse_list; browse_list != NULL; browse_list = browse_list->next ) {
        prev_ctx = NULL;
        for( curr_ctx = browse_list->ctx_list; curr_ctx != NULL; curr_ctx = curr_ctx->next ) {
            if( curr_ctx->ctx == ctx ) {
                if( prev_ctx == NULL ) {
                    *prev = ctx;
                } else {
                    *prev = prev_ctx->ctx;
                }

                next_ctx = curr_ctx;
                for(;;) {
                    next_ctx = next_ctx->next;

                    if( next_ctx == NULL ) {
                        next_ctx = curr_ctx;
                        break;
                    }

                    if( !Remove_empty || !next_ctx->ctx->empty ) {
                        break;
                    }
                }

                *next = next_ctx->ctx;
                return;
            }

            if( !Remove_empty || !curr_ctx->ctx->empty ) {
                prev_ctx = curr_ctx;
            }
        }
        /* if we've reached this point we know this browse list is no good
         * so we go onto the next one
         */
    }
    /* and if we get here we know there are no next and prev, so we point
     * back at the same context
     */
    *prev = ctx;
    *next = ctx;
    return;
}

static void ib_append_line( FILE *outfile, char *infnam )
/*******************************************************/
{
    FILE                        *infile;
    int                         inchar;

    if( infnam[0] != '\0' ) {
        infile = fopen( infnam, "rt" );
        if( infile != NULL ) {
            for(;;) {
                inchar = fgetc( infile );

                if( inchar == EOF || inchar == '\n' ) {
                    break;
                }

                whp_fprintf( outfile, "%c", inchar );
            }
            fclose( infile );
        }
    }
}

static void fake_hlink( FILE *file, char *label )
/***********************************************/
{
    if( IB_Hyper_Brace_L == '<' ) {
        whp_fprintf( file, "<<" );
    }
    whp_fprintf( file, "%s%s%s", STR_BOLD_ON, label, STR_BOLD_OFF );
    if( IB_Hyper_Brace_R == '>' ) {
        whp_fprintf( file, ">>" );
    }
    whp_fprintf( file, " " );
}

static void output_ctx_hdr( ctx_def *ctx )
/****************************************/
{
    ctx_def                     *temp_ctx;
    ctx_def                     *prev;      // for << button
    ctx_def                     *next;      // for >> button

    // output topic name
    whp_fprintf( Out_file, "::::\"" );
    str_out_ib( Out_file, ctx->title );
    whp_fprintf( Out_file, "\" 0 0\n" );

    // Header stuff:
    if( Do_tc_button
        || Do_idx_button
        || Do_browse
        || Do_up
        || Header_File[0] != '\0' ) {

        //beginning of header
        whp_fprintf( Out_file, ":h\n" );

        if( Do_tc_button ) {
            if( stricmp( ctx->ctx_name, "table_of_contents" ) != 0 ) {
                whp_fprintf( Out_file,
                                "%c" HB_CONTENTS "%cTable of Contents%c ",
                                IB_Hyper_Brace_L,
                                CHR_HLINK_BREAK,
                                IB_Hyper_Brace_R );
            } else {
                fake_hlink( Out_file, HB_CONTENTS );
            }
        }

        if( Do_kw_button ) {
            if( stricmp( ctx->ctx_name, "keyword_search" ) != 0 ) {
                whp_fprintf( Out_file,
                                "%c" HB_KEYWORDS "%cKeyword Search%c ",
                                IB_Hyper_Brace_L,
                                CHR_HLINK_BREAK,
                                IB_Hyper_Brace_R );
            } else {
                fake_hlink( Out_file, HB_KEYWORDS );
            }
        }

        if( Do_browse ) {
            find_browse_pair( ctx, &prev, &next );

            // << browse button
            if( prev != ctx ) {
                whp_fprintf( Out_file, "%c" HB_PREV "%c", IB_Hyper_Brace_L, CHR_HLINK_BREAK );
                str_out_ib( Out_file, prev->title );
                whp_fprintf( Out_file, "%c ", IB_Hyper_Brace_R );
            } else {
                fake_hlink( Out_file, HB_PREV );
            }

            // >> browse button (relies on the find_browse_pair above)
            if( next != ctx ) {
                whp_fprintf( Out_file, "%c" HB_NEXT "%c", IB_Hyper_Brace_L, CHR_HLINK_BREAK );
                str_out_ib( Out_file, next->title );
                whp_fprintf( Out_file, "%c ", IB_Hyper_Brace_R );
            } else {
                fake_hlink( Out_file, HB_NEXT );
            }
        }

        if( Do_idx_button ) {
            if( stricmp( ctx->ctx_name, "index_of_topics" ) != 0 ) {
                whp_fprintf( Out_file,
                                "%c" HB_INDEX "%cIndex of Topics%c ",
                                IB_Hyper_Brace_L,
                                CHR_HLINK_BREAK,
                                IB_Hyper_Brace_R );
            } else {
                fake_hlink( Out_file, HB_INDEX );
            }
        }

        // "Up" button
        if( Do_up ) {
            // find "parent" context
            for( temp_ctx = ctx->up_ctx; temp_ctx != NULL; temp_ctx = temp_ctx->up_ctx ) {
                if( !Remove_empty || !temp_ctx->empty ) {
                    break;
                }
            }

            // use table_of_contents if no "parent" context
            if( temp_ctx == NULL ) {
                temp_ctx = find_ctx( "table_of_contents" );
                if( temp_ctx == ctx) {
                    temp_ctx = NULL;
                }
            }

            // spit out up button stuff
            if( temp_ctx != NULL ) {
                whp_fprintf( Out_file, "%c" HB_UP "%c", IB_Hyper_Brace_L, CHR_HLINK_BREAK );
                str_out_ib( Out_file, temp_ctx->title );
                whp_fprintf( Out_file, "%c ", IB_Hyper_Brace_R );
            } else {
                fake_hlink( Out_file, HB_UP );
            }
        }
        // append user header file
        ib_append_line( Out_file, Header_File );

        // end of header
        whp_fprintf( Out_file, "\n:eh\n" );
    }
    // append user footer file
    if( Footer_File[0] != '\0' ) {
        whp_fprintf( Out_file, ":f\n" );
        ib_append_line( Out_file, Footer_File );
        whp_fprintf( Out_file, "\n:ef\n" );
    }
}

static void output_end( void )
/****************************/
{
    whp_fprintf( Out_file, "\n" );
}

static void output_section_ib( section_def *section )
/***************************************************/
{
    size_t              len;
    ctx_def             *ctx;
    unsigned int        line;
    char                *label;
    size_t              label_len;
    char                ch;
    char                *file;
    char                *topic;
    char                *p;
    char                *end;

    p = section->section_text;
    end = p + section->section_size;
    len = 0;
    while( p + len < end ) {
        // stop when we hit a hyper-link
        if( *(p + len) != CHR_TEMP_HLINK ) {
            len++;
        } else {
            // write out the block of text we've got so far
            whp_fwrite( p, 1, len, Out_file );
            p += len + 1;

            // grab the line number
            for( len = 0; ; ++len ) {
                if( *(p + len) == CHR_TEMP_HLINK ) {
                    break;
                }
            }
            p[len] = '\0';
            line = atoi( p );
            p += len + 1;

            // find the length of the link label (what the user sees)
            for( len = 0; ; ++len ) {
                if( *(p + len) == CHR_HLINK_BREAK ) {
                    break;
                }
            }

            // if we're using the brace mode we strip off the "XX"
            if( IB_Hyper_Brace_L == '<' ) {
                p += 2;
                len -= 2;
            }

            // output the label and the break
            label = p;
            label_len = len + 1;
            p += len + 1;

            // find the length of the link context
            for( len = 0; ; ++len ) {
                ch = *(p + len);
                if( ch == CHR_TEMP_HLINK || ch == CHR_HLINK_BREAK ) {
                    break;
                }
            }
            // null terminate the context name, and find the associated topic
            *(p + len) = '\0';
            topic = p;
            ctx = find_ctx( topic );

            // output the topic name that belongs to the context
            if( ctx == NULL && ch != CHR_HLINK_BREAK ) {
                warning( "Link to nonexistent context", line );
                printf( "For topic=%s\n", topic );
                whp_fwrite( label, 1, label_len - 1, Out_file );
            } else {
                // now we start writing the hyper-link
                whp_fwrite( &IB_Hyper_Brace_L, 1, 1, Out_file );
                whp_fwrite( label, 1, label_len, Out_file );
                if( ctx != NULL ) {
                    str_out_ib( Out_file, ctx->title );
                } else {
                    str_out_ib( Out_file, topic );
                }
                if( ch == CHR_HLINK_BREAK ) {
                    /* file link. Get the file name */
                    file = p + len + 1;
                    for( ;; ) {
                        ++len;
                        if( *(p + len) == CHR_TEMP_HLINK ) {
                            break;
                        }
                    }
                    *(p + len) = '\0';
                    whp_fprintf( Out_file, "%c%s", CHR_HLINK_BREAK, file );
                }
                whp_fwrite( &IB_Hyper_Brace_R, 1, 1, Out_file );
            }

            // adjust the len and ctr counters appropriately
            p += len + 1;
            len = 0;
        }
    }
    // output whatever's left
    if( p < end ) {
        whp_fwrite( p, 1, end - p, Out_file );
    }
}

static void output_ctx_sections( ctx_def *ctx )
/*********************************************/
{
    section_def                 *section;

    for( section = ctx->section_list; section != NULL; section = section->next ) {
        if( section->section_size > 0 ) {
            output_section_ib( section );
        }
    }
}

void ib_output_file( void )
/*************************/
{
    ctx_def         *ctx;

    if( IB_def_topic != NULL ) {
        fprintf( Out_file, "DEFTOPIC::::\"%s\"\n", IB_def_topic );
        free( IB_def_topic );
    }
    if( IB_help_desc != NULL ) {
        fprintf( Out_file, "DESCRIPTION::::\"%s\"\n", IB_help_desc );
        free( IB_help_desc );
    }
    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( !Remove_empty || !ctx->empty || ctx->req_by_link ) {
            output_ctx_hdr( ctx );
            output_ctx_sections( ctx );
        }
    }
    output_end();
}

