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
* Description:  This file defines the wiki specific functions.
*
****************************************************************************/


#include "whpcvt.h"

#include "clibext.h"


#define BOX_LINE_SIZE           200

#define FONT_STYLE_BOLD         1
#define FONT_STYLE_ITALIC       2
#define FONT_STYLE_UNDERLINE    4

#define WIKI_SPACE              " " //"&nbsp;"

#define WIKI_TRANS_LEN          50

#define MAX_TABS                100     // up to 100 tab stops

static int          Curr_head_level = 0;
static int          Curr_head_skip = 0;

static char         *Font_match[] = {
    "",                     // 0: PLAIN
    "<b>",                  // 1: BOLD
    "<i>",                  // 2: ITALIC
    "<b><i>",               // 3: BOLD + ITALIC
    "<u>",                  // 4: UNDERLINE
    "<b><u>",               // 5: BOLD + UNDERLINE
    "<i><u>",               // 6: ITALIC + UNDERLINE
    "<i><u><b>",            // 7: ITALIC + BOLD + UNDERLINE
};

static char         *Font_end[] = {
    "",                     // 0: PLAIN
    "</b>",                 // 1: BOLD
    "</i>",                 // 2: ITALIC
    "</i></b>",             // 3: BOLD + ITALIC
    "</u>",                 // 4: UNDERLINE
    "</u></b>",             // 5: BOLD + UNDERLINE
    "</u></i>",             // 6: ITALIC + UNDERLINE
    "</b></u></i>",         // 7: ITALIC + BOLD + UNDERLINE
};

static int          Font_list[100];      // up to 100 nested fonts
static int          Font_list_curr = 0;

static bool         Blank_line_pfx = false;
static bool         Blank_line_sfx = true;

static char         *Trans_str = NULL;
static size_t       Trans_len = 0;

static unsigned     Tab_list[MAX_TABS];
static int          tabs_num = 0;

static void draw_line( section_def *section, size_t *size )
/*********************************************************/
{
    trans_add_str( "----\n", section, size );
}

static size_t translate_char_wiki( char ch, char next_ch, char *buf )
/*******************************************************************/
{
    switch( ch ) {
#if 0
    case '<':
        strcpy( buf, "&lt;" );
        break;
    case '>':
        strcpy( buf, "&gt;" );
        break;
    case '&':
        strcpy( buf, "&amp;" );
        break;
    case '"':
        strcpy( buf, "&quot;" );
        break;
#endif
    case ' ':
        if( next_ch == ' ' ) {
            strcpy( buf, WIKI_SPACE );
            break;
        }
        /* fall through */
    default:
        buf[0] = ch;
        buf[1] = '\0';
        break;
    }
    return( strlen( buf ) );
}

static char *translate_str_wiki( const char *str )
/************************************************/
{
    const char      *t_str;
    size_t          len;
    char            buf[WIKI_TRANS_LEN];
    char            *ptr;

    len = 1;
    for( t_str = str; *t_str != '\0'; ++t_str ) {
        len += translate_char_wiki( t_str[0], t_str[1], buf );
    }
    if( len > Trans_len ) {
        if( Trans_str != NULL ) {
            _free( Trans_str );
        }
        _new( Trans_str, len );
        Trans_len = len;
    }
    ptr = Trans_str;
    for( t_str = str; *t_str != '\0'; ++t_str ) {
        len = translate_char_wiki( t_str[0], t_str[1], buf );
        strcpy( ptr, buf );
        ptr += len;
    }
    *ptr = '\0';

    return( Trans_str );
}

static size_t trans_add_char_wiki( char ch, char next_ch, section_def *section, size_t *size )
/********************************************************************************************/
{
    char            buf[WIKI_TRANS_LEN];

    translate_char_wiki( ch, next_ch, buf );
    return( trans_add_str( buf, section, size ) );
}

static size_t trans_add_str_wiki( const char *str, section_def *section, size_t *size )
/*************************************************************************************/
{
    size_t          len;
    const char      *ptr;

    len = 0;
    for( ptr = str; *ptr != '\0'; ++ptr ) {
        len += trans_add_char_wiki( ptr[0], ptr[1], section, size );
    }
    return( len );
}

static size_t trans_add_list( char *list, section_def *section, size_t *size, char *ptr )
/***************************************************************************************/
{
    size_t          len;

    len = trans_add_str( list, section, size );
    ++ptr;
#if 0
    if( *ptr == 'c' ) {
        len += trans_add_str( " compact", section, size );
    }
    len += trans_add_str( ">\n", section, size );
#endif
    return( len );
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
        trans_add_str_wiki( WIKI_SPACE, section, size );
    }
    return( len );
}

void wiki_topic_init( void )
/**************************/
{
}

size_t wiki_trans_line( section_def *section, size_t size )
/*********************************************************/
{
    char                *ptr;
    char                *end;
    char                ch;
    char                *ctx_name;
    char                *ctx_text;
    char                buf[500];
    int                 font_idx;
    size_t              line_len;
    bool                term_fix;
    size_t              ch_len;
    size_t              len;
    char                *file_name;

    /* check for special column 0 stuff first */
    ptr = Line_buf;
    ch = *ptr;
    ch_len = 0;
    line_len = 0;

    switch( ch ) {
    case CH_TABXMP:
        if( *skip_blank( ptr + 1 ) == '\0' ) {
            Tab_xmp = false;
            trans_add_str( "</pre>\n", section, &size );
            Blank_line_sfx = false;     // remove following blanks
        } else {
            read_tabs( ptr + 1 );
            trans_add_str( "<pre>\n", section, &size );
            Tab_xmp = true;
            Blank_line_pfx = false;     // remove preceding blanks
        }
        return( size );
    case CH_BOX_ON:
        /* Table support is the closest thing to boxing in IPF, but it
           doesn't work well with changing fonts on items in the tables
           (the edges don't line up). So we draw long lines at the
           top and bottom instead */
        draw_line( section, &size );
        Blank_line_pfx = false;
        return( size );
    case CH_BOX_OFF:
        draw_line( section, &size );
        Blank_line_sfx = false;
        return( size );
    case CH_OLIST_START:
        trans_add_list( "# ", section, &size, ptr );
        Blank_line_pfx = false;
        return( size );
    case CH_LIST_START:
        trans_add_list( "* ", section, &size, ptr );
        Blank_line_pfx = false;
        return( size );
    case CH_DLIST_START:
        trans_add_str( "; ", section, &size );
        Blank_line_pfx = false;
        return( size );
    case CH_SLIST_START:
        trans_add_list( "* ", section, &size, ptr );
        Blank_line_pfx = false;
        return( size );
    case CH_SLIST_END:
        trans_add_str( "\n", section, &size );
        Blank_line_sfx = false;
        return( size );
    case CH_OLIST_END:
        trans_add_str( "\n", section, &size );
        Blank_line_sfx = false;
        return( size );
    case CH_LIST_END:
        trans_add_str( "\n", section, &size );
        Blank_line_sfx = false;
        return( size );
    case CH_DLIST_END:
        trans_add_str( "\n", section, &size );
        Blank_line_sfx = false;
        return( size );
    case CH_LIST_ITEM:
    case CH_DLIST_TERM:
        /* eat blank lines before list items and terms */
        Blank_line_pfx = false;
        break;
    case CH_CTX_KW:
        ptr = whole_keyword_line( ptr );
        if( ptr == NULL ) {
            return( size );
        }
        break;
    }

    if( *skip_blank( ptr ) == '\0' ) {
        /* ignore blanks lines before the topic starts */
        if( !Curr_ctx->empty ) {
            /* the line is completely blank. This tells us to output
               a blank line. BUT, all lists and things automatically
               generate blank lines before they display, so we
               must pend the line */
            Blank_line_pfx = true;
        }
        return( size );
    }

    /* An explanation of 'Blank_line_pfx': when we hit a blank line,
       we set Blank_line_pfx to true. On the non-tag next line, the
       blank line is generated.
       Some tags automatically generate a blank line, so they
       turn this flag off. This causes the next non-tag line to NOT
       put out the blank line */

    if( Blank_line_pfx || Blank_line_sfx ) {
        line_len += trans_add_str( "\n", section, &size );
        Blank_line_pfx = false;
    }
    Blank_line_sfx = true;

    /* An explanation of 'Blank_line_sfx': some ending tags automatically
       generate a blank line, so no blank line after them should get
       generated. Normally, this flag is set to true, but ending
       tags and Defn list term tags set this false, so no extra '<P>'
       is generated.
       But, this rule only applies if a blank line immediately
       follows the tag, so its reset here regardless */
#if 0
   if( *ptr != CH_LIST_ITEM && *ptr != CH_DLIST_TERM && *ptr != CH_DLIST_DESC && !Tab_xmp ) {
        /* a .br in front of li and dt would generate extra spaces */
        if( !done_blank ) {
           line_len += trans_add_str( "<P>", section, &size );
        }
    }
#endif

    term_fix = false;
    for( ;; ) {
        ch = *ptr;
        if( ch == '\0' ) {
            if( term_fix ) {
//              trans_add_str( "</hp2>", section, &size );
                term_fix = false;
            }
            trans_add_char( '\n', section, &size );
            break;
        } else if( ch == CH_HLINK || ch == CH_DFN ) {
            Curr_ctx->empty = false;
            /* there are no popups in IPF, so treat them as links */
            ctx_name = ptr + 1;
            ptr = strchr( ptr + 1, ch );
            if( ptr == NULL ) {
                error( ERR_BAD_LINK_DFN, true );
            }
            *ptr = '\0';
            ctx_text = ptr + 1;
            ptr = strchr( ctx_text + 1, ch );
            if( ptr == NULL ) {
              error( ERR_BAD_LINK_DFN, true );
            }
            *ptr = '\0';
            add_link( ctx_name );
            sprintf( buf, "<A HREF=\"#%s\">", ctx_name );
            line_len += trans_add_str( buf, section, &size );
            line_len += trans_add_str_wiki( ctx_text, section, &size );
            ch_len += strlen( ctx_text );
            line_len += trans_add_str( "</A>", section, &size );
            ++ptr;
        } else if( ch == CH_FLINK ) {
            Curr_ctx->empty = false;
            file_name = strchr( ptr + 1, ch );
            if( file_name == NULL ) {
                error( ERR_BAD_LINK_DFN, true );
            }
            ctx_name = strchr( file_name + 1, ch );
            if( ctx_name == NULL ) {
                error( ERR_BAD_LINK_DFN, true );
            }
            ctx_text = strchr( ctx_name + 1, ch );
            if( ctx_text == NULL ) {
                error( ERR_BAD_LINK_DFN, true );
            }
            *ctx_text = '\0';
            ctx_text = ctx_name + 1;
            *ctx_name = '\0';
            ctx_name = file_name + 1;
            *file_name = '\0';
            file_name = ptr + 1;
            sprintf( buf, "<A HREF=\"#%s\">", ctx_name );
            line_len += trans_add_str( buf, section, &size );
            line_len += trans_add_str_wiki( ctx_text, section, &size );
            ch_len += strlen( ctx_text );
            line_len += trans_add_str( "</A>", section, &size );
            ptr = ctx_text + strlen( ctx_text ) + 1;
        } else if( ch == CH_LIST_ITEM ) {
            /* list item */
            line_len += trans_add_str( "*", section, &size );
            ptr = skip_blank( ptr + 1 );
        } else if( ch == CH_DLIST_DESC ) {
            trans_add_str( ":", section, &size );
            ptr = skip_blank( ptr + 1 );
        } else if( ch == CH_DLIST_TERM ) {
            /* definition list term */
            line_len += trans_add_str( ";", section, &size );
            term_fix = true;
            ptr = skip_blank( ptr + 1 );
            Blank_line_sfx = false;
        } else if( ch == CH_CTX_KW ) {
            end = strchr( ptr + 1, CH_CTX_KW );
            memcpy( buf, ptr + 1, end - ptr - 1 );
            buf[end - ptr - 1] = '\0';
            add_ctx_keyword( Curr_ctx, buf );
            ptr = end + 1;
            if( *ptr == ' ' ) {
                /* kludge fix cuz of GML: GML thinks that keywords
                   are real words, so it puts a space after them.
                   This should fix that */
                ++ptr;
            }
        } else if( ch == CH_PAR_RESET ) {
            /* this can be ignored for IPF */
            ++ptr;
        } else if( ch == CH_BMP ) {
            Curr_ctx->empty = false;
            ++ptr;
            ch = *ptr;
            ptr += 2;
            end = strchr( ptr, CH_BMP );
            *end = '\0';
            // convert filenames to lower case
            strlwr( ptr );
            switch( ch ) {
            case 'i':
                sprintf( buf, "<IMG SRC=\"%s\">", ptr );
                break;
            case 'l':
                sprintf( buf, "<IMG SRC=\"%s\" ALIGN=TOP>", ptr );
                break;
            case 'r':
                sprintf( buf, "<IMG SRC=\"%s\" ALIGN=BOTTOM>", ptr );
                break;
            case 'c':
                sprintf( buf, "<IMG SRC=\"%s\" ALIGN=MIDDLE>", ptr );
                break;
            default:
                *buf = '\0';
                break;
            }
            line_len += trans_add_str( buf, section, &size );
            ptr = end + 1;
        } else if( ch == CH_FONTSTYLE_START ) {
            ++ptr;
            end = strchr( ptr, CH_FONTSTYLE_START );
            font_idx = 0;
            for( ; ptr != end; ++ptr ) {
                switch( *ptr ) {
                case 'b':
                    font_idx |= FONT_STYLE_BOLD;
                    break;
                case 'i':
                    font_idx |= FONT_STYLE_ITALIC;
                    break;
                case 'u':
                case 's':
                    font_idx |= FONT_STYLE_UNDERLINE;
                    break;
                }
            }
            line_len += trans_add_str( Font_match[font_idx], section, &size );
            Font_list[Font_list_curr] = font_idx;
            ++Font_list_curr;
            ++ptr;
        } else if( ch == CH_FONTSTYLE_END ) {
            --Font_list_curr;
            line_len += trans_add_str( Font_end[Font_list[Font_list_curr]], section, &size );
            ++ptr;
        } else if( ch == CH_FONTTYPE ) {
            ++ptr;
            end = strchr( ptr, CH_FONTTYPE );
            *end = '\0';

            if( stricmp( ptr, Fonttype_courier ) == 0 ) {
               strcpy( buf, "<tt>" );
            } else {
               /* default system font */
               strcpy( buf, "</tt>" );
            }
            ptr = end + 1;
            end = strchr( ptr, CH_FONTTYPE );
            line_len += trans_add_str( buf, section, &size );
            ptr = end + 1;
        } else {
            ++ptr;
            Curr_ctx->empty = false;
            if( Tab_xmp && ch == Tab_xmp_char ) {
                len = tab_align( ch_len, section, &size );
                ch_len += len;
                line_len += len * sizeof( WIKI_SPACE );
                ptr = skip_blank( ptr );
            }
            if( line_len > 120 && ch == ' ' && !Tab_xmp ) {
                /* break onto the next line */
                line_len = 0;
                trans_add_char( '\n', section, &size );
                if( *ptr == ' ' ) {
                    line_len += trans_add_str( WIKI_SPACE, section, &size );
                    ++ch_len;
                    ptr++;
                }
            } else {
                line_len += trans_add_char_wiki( ch, *ptr, section, &size );
                ++ch_len;
            }
        }
    }

    return( size );
}

static void output_ctx_hdr( ctx_def *ctx )
/****************************************/
{
    int                         head_level;
    int                         p_skip;
    int                         i;

    head_level = ctx->head_level;
    if( head_level == 0 ) {
        /* OS/2 can't handle heading level 0 */
        head_level = 1;
    }
    head_level -= Curr_head_skip;
    if( head_level > Curr_head_level + 1 ) {
        /* you can't skip heading levels upwards in IPF. To handle this,
           you go up to the next level, and keep track of the
           skip for future heading levels. */
        p_skip = head_level - Curr_head_level - 1 ;
        head_level -= p_skip;
        Curr_head_skip += p_skip;
    } else if( head_level < Curr_head_level ) {
        head_level += Curr_head_skip;
        if( head_level > Curr_head_level ) {
            /* we moved down levels, but we're still too high! */
            Curr_head_skip = head_level - Curr_head_level;
            head_level -= Curr_head_skip;
        } else {
            Curr_head_skip = 0;
        }
    }

    Curr_head_level = head_level;

    whp_fprintf( Out_file, "\n" );

    for( i = 0; i < head_level; ++i )
        whp_fprintf( Out_file, "=" );

    whp_fprintf( Out_file, "%s", translate_str_wiki( ctx->title ) );

    for( i = 0; i < head_level; ++i )
        whp_fprintf( Out_file, "=" );

    whp_fprintf( Out_file, "\n" );

    /* browse lists are not used in wiki */
    /* nor does 'Up' topicing have any relevance */
}

static void output_ctx_sections( ctx_def *ctx )
/*********************************************/
{
    section_def     *section;

    for( section = ctx->section_list; section != NULL; section = section->next ) {
        if( section->section_size > 0 ) {
            whp_fwrite( section->section_text, 1, section->section_size, Out_file );
        }
    }
}

void wiki_output_file( void )
/***************************/
{
    ctx_def         *ctx;

    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( !Remove_empty || !ctx->empty || ctx->req_by_link ) {
            if( !Exclude_special_topics || !is_special_topic( ctx, false ) ) {
                output_ctx_hdr( ctx );
                output_ctx_sections( ctx );
            }
        }
    }
}
