/****************************************************************************
*
*                            Open Watcom Project
*
* Copyright (c) 2002-2019 The Open Watcom Contributors. All Rights Reserved.
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
* Description:  This program converts WHP files (WATCOM Help) into OS
*               dependent help files. Currently supported formats:
*
*                * Windows RTF
*                * OS/2 IPF
*                * DOS InfoBench Help (maybe)
*                * HTML
*                * MediaWiki tags
*
****************************************************************************/


#define WHPCVT_GBL
#include "whpcvt.h"

#include "clibext.h"


#define HELP_PREFIX     "HLP_"

#define BUF_GROW        150

#define ARG_DEFS() \
    pick( ARG_I,    "i" ) \
    pick( ARG_S,    "s" ) \
    pick( ARG_K,    "k" ) \
    pick( ARG_XL,   "xl" ) \
    pick( ARG_DL,   "dl" ) \
    pick( ARG_SL,   "sl" ) \
    pick( ARG_OL,   "ol" ) \
    pick( ARG_UL,   "ul" ) \
    pick( ARG_H,    "h" ) \
    pick( ARG_HH,   "hh" ) \
    pick( ARG_HN,   "hn" ) \
    pick( ARG_B,    "b" ) \
    pick( ARG_UP,   "up" ) \
    pick( ARG_IW,   "iw" ) \
    pick( ARG_RTF,  "rtf" ) \
    pick( ARG_IPF,  "ipf" ) \
    pick( ARG_BL,   "bl" ) \
    pick( ARG_T,    "t" ) \
    pick( ARG_E,    "e" ) \
    pick( ARG_RF,   "rf" ) \
    pick( ARG_LK,   "lk" ) \
    pick( ARG_KT,   "kt" ) \
    pick( ARG_IB,   "ib" ) \
    pick( ARG_RM,   "rm" ) \
    pick( ARG_HD,   "hd" ) \
    pick( ARG_FT,   "ft" ) \
    pick( ARG_TAB,  "tab" ) \
    pick( ARG_HB,   "hb" ) \
    pick( ARG_BR,   "br" ) \
    pick( ARG_TC,   "tc" ) \
    pick( ARG_IX,   "ix" ) \
    pick( ARG_KW,   "kw" ) \
    pick( ARG_KB,   "kb" ) \
    pick( ARG_TL,   "tl" ) \
    pick( ARG_EX,   "ex" ) \
    pick( ARG_MC,   "mc" ) \
    pick( ARG_OF,   "@" ) \
    pick( ARG_DT,   "dt" ) \
    pick( ARG_DS,   "ds" ) \
    pick( ARG_DPT,  "dpt" ) \
    pick( ARG_DPK,  "dpk" ) \
    pick( ARG_DPI,  "dpi" ) \
    pick( ARG_DPB,  "dpb" ) \
    pick( ARG_HTML, "html" ) \
    pick( ARG_WIKI, "wiki" ) \
    pick( ARG_END,  NULL )

enum {
    TITLE_CASE_UPPER,
    TITLE_CASE_MIXED,
    TITLE_CASE_LAST,
};

enum {
    GEN_TITLE_CONTENTS,
    GEN_TITLE_INDEX,
    GEN_TITLE_KEYWORD,
    GEN_TITLE_BROWSE,
    GEN_TITLE_LAST,
};

enum {
    #define pick(e,t) e,
    ARG_DEFS()
    #undef pick
};

enum {
    OUT_RTF,
    OUT_IPF,
    OUT_IB,
    OUT_HTML,
    OUT_WIKI
};

/* global variables */

char Help_fname[100];
char Header_File[100];
char Footer_File[100];

const char Fonttype_roman[] = "roman";
const char Fonttype_symbol[] = "symbol";
const char Fonttype_helv[] = "helv";
const char Fonttype_courier[] = "courier";

/* local variables */

static const char *Help_info[] = {
    "Usage: whpcvt [options] <in_file> [<out_file>]",
    "   Options for all help file types:",
    "     -rtf     : generate Windows RTF output",
    "     -ipf     : generate OS/2 IPF output",
    "     -ib      : generate DOS InfoBench output",
    "     -html    : generate HTML output",
    "     -wiki    : generate wiki output",
    "     -@ <opt_file> : read more options from <opt_file> after the command line",
    "     -i       : generate <in_file>.idx file containing index of topics",
    "     -iw      : generate '.idx' file in WHP format (instead of GML)",
    "     -b       : generate <in_file>.blt file containing browse lists (GML)",
    "     -kw      : generate <in_file>.kw file containing keywords (GML)",
    "     -s       : sort browse lists",
    "     -h       : generate <in_file>.h topic define-const file",
    "     -hn      : read old <in_file>.h file to set new ctx numbers",
    "     -bl      : allow links to break across lines",
    "     -t       : generate <in_file>.tbl file containing table of contents",
    "     -e       : remove empty topics (except from table of contents)",
    "     -lk      : don't remove empty topics which have links to them",
    "     -kt      : don't add topic titles as search keywords",
    "     -mc      : use mixed case for generated topic titles (i.e. contents, etc.)",
    "     -dpt     : don't put popup topics in .tbl file",
    "     -dpk     : don't put popup topics in .kw file",
    "     -dpi     : don't put popup topics in .idx file",
    "     -dpb     : don't put popup topics in .blt file",
    "",
    "   Options for Windows RTF format:",
    "     -hh      : generate <in_file>.hh topic const file for help compiler",
    "     -up      : enable up topic support",
    "     -k       : keep titles in nonscrolling region at top",
    "     -xl      : initial indent for all lists",
    "     -dl      : initial indent for definition lists",
    "     -sl      : initial indent for simple lists",
    "     -ol      : initial indent for ordered lists",
    "     -ul      : initial indent for unordered lists",
    "",
    "   Options for OS/2 IPF & HTML format:",
    "     -rf      : use specified font types, not just system and courier",
    "     -tl \"title\" : sets a title for the help window. Put the title in quotes",
    "",
    "   Options for OS/2 IPF, HTML & Wiki format:",
    "     -ex      : exclude special topics",
    "",
    "   Options for DOS InfoBench format:",
    "     -rm <n>  : set right margin to column <n>. Defaults to 76",
    "     -hd <f>  : include first line of file <f> in header",
    "     -ft <f>  : include first line of file <f> in footer",
    "     -tab <n> : set tab spacing to <n>. defaults to 4",
    "     -hb      : turns hyperlink bracing on",
    "     -up      : enable up topic support",
    "     -br      : enable browse button support",
    "     -tc      : enable contents button support",
    "     -ix      : enable index button support",
    "     -kb      : enable keyword button support",
    "     -dt \"topic\" : default topic",
    "     -ds \"desc\"  : description string for help file",
    "",
    "   You can specify \"-@ <opt_file>\" to put more options in a file",
    "       - Each option must be on a separate line",
    "   Default <in_file> extension is .whp",
    "   Default <out> extension is",
    "       for RTF:           " EXT_OUTRTF_FILE,
    "       for OS/2 IPF:      " EXT_OUTIPF_FILE,
    "       for Dos Infobench: " EXT_OUTIB_FILE,
    "       for HTML:          " EXT_OUTHTML_FILE,
    "       for wiki:          " EXT_OUTWIKI_FILE,
    NULL
};

static const char *Args[]={
    #define pick(e,t) t,
    ARG_DEFS()
    #undef pick
};

static const char *(Gen_titles[GEN_TITLE_LAST][TITLE_CASE_LAST])={
     { "Table of Contents",             "Table of contents"     },
     { "Index of Topics",               "Index of topics"       },
     { "Keyword Search",                "Keyword search"        },
     { "Browse Lists",                  "Browse lists"          }
};

/* File stuff */
static jmp_buf          Jmp_buf;
static size_t           Line_buf_size;

/* Processing globals */
static bool             Exclude_on = false;
static char             Delim[] = " \t";
static bool             Do_index = false;
static bool             Do_blist = false;
static bool             Do_keywords = false;
static bool             Browse_sort = false;
static bool             Do_def = false;
static bool             Do_hdef = false;
static bool             Do_ctx_ids = false;
static bool             Do_contents = false;
static bool             Index_gml_fmt = true;
static int              Brace_count = 0;
static bool             Brace_check = false;
static bool             Do_topic_keyword = true;
static int              Title_case = TITLE_CASE_UPPER;
static bool             Dump_popup_t = false;
static bool             Dump_popup_k = false;
static bool             Dump_popup_i = false;
static bool             Dump_popup_b = false;
static int              Output_type = OUT_RTF;


static const char *Error_list[]={
    "Expecting topic definition, or topic section",
    "Context topic already exists",
    "A defined context topic must have a title",
    "Out of memory",
    "Bad cross-reference (hyperlink) or definition",
    "Maximum of 20 nested numbered lists allowed",
    "Cross-reference (hyperlink) to undefined topic",
    "Cross-reference (hyperlink) to an empty topic (use -lk option)",
    "Invalid number of parameters."
};

static char *Options_file = NULL;

static char *Chk_buf = NULL;

static char Output_file_ext[10];

static ctx_def **Ctx_list_end = NULL;

static keyword_def *Keyword_list = NULL;
static int Keyword_id = 1;

static void print_help( void )
/****************************/
{
    const char      **p;

    for( p = Help_info; *p != NULL; ++p ) {
        printf( "%s\n", *p );
    }
}

static void error_quit( void )
/****************************/
{
    longjmp( Jmp_buf, 1 );
}

void error_str( const char *err_str )
/***********************************/
{
    printf( "****%s\n", err_str );
    error_quit();
}

void error( int err, bool line_num )
/**********************************/
{
    if( line_num ) {
        printf( "Error in input file on line %d.\n", Line_num );
    }
    error_str( Error_list[err] );
}

#if 0
static void error_line( int err, int line_num )
/*********************************************/
{
    Line_num = line_num;

    error( err, true );
}
#endif

static void warning_str( const char *warn_str )
/*********************************************/
{
    printf( "****%s\n", warn_str );
}

static void warning( int err, bool line_num )
/*******************************************/
{
    if( line_num ) {
        printf( "Warning - in input file on line %d.\n", Line_num );
    }
    warning_str( Error_list[err] );
}

static void warning_line( int err, int line_num )
/***********************************************/
{
    Line_num = line_num;

    warning( err, true );
}

void *check_alloc( size_t size )
/******************************/
{
    void                *p;

    p = malloc( size );

    if( p == NULL && size != 0) {
        error( ERR_NO_MEMORY, false );
    }

    return( p );
}

void *check_realloc( void *ptr, size_t size )
/*******************************************/
{
    ptr = realloc( ptr, size );

    if( ptr == NULL && size != 0 ) {
        error( ERR_NO_MEMORY, false );
    }

    return( ptr );
}

static void check_brace( const char *str, size_t len )
/****************************************************/
{
    const char          *ptr;
    char                ch;
    char                prev_ch;

    if( Brace_check && Output_type == OUT_RTF ) {
        prev_ch = ' ';
        for( ptr = str; len-- > 0; ++ptr ) {
            ch = *ptr;
            if( ch == '{' && prev_ch != '\\' ) {
                ++Brace_count;
            } else if( ch == '}' && prev_ch != '\\' ) {
                --Brace_count;
            }
            prev_ch = ch;
        }
    }
}

void whp_fprintf( FILE *file, const char *fmt, ... )
/**************************************************/
{
    va_list             arglist;

    if( Chk_buf == NULL ) {
        _new( Chk_buf, 10000 );
    }

    if( Chk_buf != NULL ) {
        va_start( arglist, fmt );
        vsprintf( Chk_buf, fmt, arglist );
        check_brace( Chk_buf, strlen( Chk_buf ) );
        fputs( Chk_buf, file );
        va_end( arglist );
    }
}

void whp_fwrite( const char *buf, size_t el_size, size_t num_el, FILE *f )
/************************************************************************/
{
    check_brace( buf, el_size * num_el );
    fwrite( buf, el_size, num_el, f );
}

static int process_args( int argc, char *argv[] )
/***********************************************/
{
    int                 i;
    int                 start_arg;

    for( start_arg = 0; start_arg < argc; ++start_arg ) {
        if( argv[start_arg][0] == '-' || argv[start_arg][0] == '/' ) {
            for( i = 0; Args[i] != NULL; ++i ) {
                if( stricmp( Args[i], &argv[start_arg][1] ) == 0 ) {
                    break;
                }
            }
            switch( i ) {
            case ARG_DPT:
                Dump_popup_t = true;
                break;
            case ARG_DPI:
                Dump_popup_i = true;
                break;
            case ARG_DPK:
                Dump_popup_k = true;
                break;
            case ARG_DPB:
                Dump_popup_b = true;
                break;
            case ARG_UP:
                Do_up = true;
                break;
            case ARG_KB:
                Do_kw_button = true;
                break;
            case ARG_I:
                Do_index = true;
                break;
            case ARG_KW:
                Do_keywords = true;
                break;
            case ARG_B:
                Do_blist = true;
                break;
            case ARG_S:
                Browse_sort = true;
                break;
            case ARG_K:
                Keep_titles = true;
                break;
            case ARG_EX:
                Exclude_special_topics = true;
                break;
            case ARG_H:
                Do_def = true;
                break;
            case ARG_HH:
                Do_hdef = true;
                break;
            case ARG_HN:
                Do_ctx_ids = true;
                break;
            case ARG_XL:
                Start_inc_sl = INDENT_INC;
                Start_inc_ol = INDENT_INC;
                Start_inc_dl = INDENT_INC;
                Start_inc_ul = INDENT_INC;
                break;
            case ARG_DL:
                Start_inc_dl = INDENT_INC;
                break;
            case ARG_SL:
                Start_inc_sl = INDENT_INC;
                break;
            case ARG_OL:
                Start_inc_ol = INDENT_INC;
                break;
            case ARG_UL:
                Start_inc_ul = INDENT_INC;
                break;
            case ARG_IW:
                Index_gml_fmt = false;
                break;
            case ARG_RTF:
                Output_type = OUT_RTF;
                break;
            case ARG_IPF:
                Output_type = OUT_IPF;
                break;
            case ARG_IB:
                Output_type = OUT_IB;
                break;
            case ARG_HTML:
                Output_type = OUT_HTML;
                break;
            case ARG_WIKI:
                Output_type = OUT_WIKI;
                break;
            case ARG_BL:
                Break_link = true;
                break;
            case ARG_T:
                Do_contents = true;
                break;
            case ARG_E:
                Remove_empty = true;
                break;
            case ARG_RF:
                Ipf_or_Html_Real_font = true;
                break;
            case ARG_LK:
                Keep_link_topics = true;
                break;
            case ARG_KT:
                Do_topic_keyword=false;
                break;
            case ARG_RM:
                start_arg++;
                if( start_arg < argc && (i = atoi( argv[start_arg] )) >= 0 ) {
                    Right_Margin = i;
                } else {
                    error( ERR_BAD_ARGS, false );
                }
                break;
            case ARG_TAB:
                start_arg++;
                if( start_arg < argc && (i = atoi( argv[start_arg] )) >= 0 ) {
                    Text_Indent = i;
                } else {
                    error( ERR_BAD_ARGS, false );
                }
                break;
            case ARG_HD:
                start_arg++;
                if( start_arg < argc ) {
                    strncpy( Header_File, argv[start_arg], 100 );
                    Header_File[99] = '\0';
                } else {
                    error( ERR_BAD_ARGS, false );
                }
                break;
            case ARG_FT:
                start_arg++;
                if( start_arg < argc ) {
                    strncpy( Header_File, argv[start_arg], 100 );
                    Header_File[99] = '\0';
                } else {
                    error( ERR_BAD_ARGS, false );
                }
                break;
            case ARG_HB:
                IB_Hyper_Brace_L = IB_BRACE_L_CHAR;
                IB_Hyper_Brace_R = IB_BRACE_R_CHAR;
                break;
            case ARG_BR:
                Do_browse = true;
                break;
            case ARG_TC:
                Do_tc_button = true;
                break;
            case ARG_IX:
                Do_idx_button = true;
                break;
            case ARG_TL:
                ++start_arg;
                if( start_arg < argc ) {
                    _new( Ipf_or_Html_title, strlen( argv[start_arg] ) + 1 );
                    strcpy( Ipf_or_Html_title, argv[start_arg] );
                } else {
                    error( ERR_BAD_ARGS, false );
                }
                break;
            case ARG_MC:
                Title_case = TITLE_CASE_MIXED;
                break;
            case ARG_DT:
                ++start_arg;
                if( start_arg < argc ) {
                    _new( IB_def_topic, strlen( argv[start_arg] ) + 1 );
                    strcpy( IB_def_topic, argv[start_arg] );
                } else {
                    error( ERR_BAD_ARGS, false );
                }
                break;
            case ARG_DS:
                ++start_arg;
                if( start_arg < argc ) {
                    _new( IB_help_desc, strlen( argv[start_arg] ) + 1 );
                    strcpy( IB_help_desc, argv[start_arg] );
                } else {
                    error( ERR_BAD_ARGS, false );
                }
                break;
            case ARG_OF:
                ++start_arg;
                if( start_arg < argc ) {
                    _new( Options_file, strlen( argv[start_arg] ) + 1 );
                    strcpy( Options_file, argv[start_arg] );
                } else {
                    error( ERR_BAD_ARGS, false );
                }
                break;
            default:
                return( 0 );
            }
        } else {
            break;
        }
    }

    return( start_arg );
}

static int valid_args( int argc, char *argv[] )
/*********************************************/
{
    int                 start_arg;
    FILE                *opt_file;
    char                line[200];
    int                 ret;
    int                 i;
    size_t              j;
    char                *x;

    Tab_xmp = false;

    Start_inc_ul = 0;
    Start_inc_ol = 0;
    Start_inc_sl = 0;
    Start_inc_dl = 0;
    Browse_sort = false;
    Do_index = false;
    Do_contents = false;
    Keep_titles = false;
    Exclude_special_topics = false;
    Do_def = false;
    Do_hdef = false;
    Do_ctx_ids = false;
    Do_blist = false;
    Do_up = false;
    Do_kw_button = false;
    Index_gml_fmt=true;
    Brace_count = 0;
    Output_type = OUT_RTF;
    Break_link = false;
    Do_contents = false;
    Remove_empty = false;
    Ipf_or_Html_Real_font = false;
    Keep_link_topics = false;
    Do_topic_keyword = true;
    Dump_popup_t = false;
    Dump_popup_i = false;
    Dump_popup_b = false;
    Dump_popup_k = false;

    Right_Margin = 76;
    Header_File[0] = '\0';
    Footer_File[0] = '\0';
    Text_Indent = 4;
    IB_Hyper_Brace_L = IB_HLINK_L_CHAR;
    IB_Hyper_Brace_R = IB_HLINK_R_CHAR;
    Do_browse = false;
    Do_tc_button = false;
    Do_idx_button = false;
    Title_case = TITLE_CASE_UPPER;

    start_arg = process_args( argc, argv );
    if( start_arg < argc - 2 || start_arg >= argc ) {
        return( -1 );
    }

    for( ;; ) {
        if( Options_file != NULL ) {
            opt_file = fopen( Options_file, "r" );
            if( opt_file == NULL ) {
                return( -1 );
            }
            for( argc = 0 ;; ++argc ) {
                if( fgets( line, sizeof( line ), opt_file ) == NULL ) {
                    break;
                }
            }
            fclose( opt_file );
            opt_file = fopen( Options_file, "r" );

            free( Options_file );
            Options_file = NULL;

            _new( argv, argc );
            for( argc = 0;; ++argc ) {
                if( fgets( line, sizeof( line ), opt_file ) == NULL ) {
                    break;
                }
                line[sizeof( line ) - 1] = 0;
                for( x = line, i = 0; i < sizeof( line ) && *x != 0; i++, x++ ) {
                    if( *x != ' ' && *x != 9 ) {
                        strcpy( line, x );
                        break;
                    }
                }
                for( j = strlen( line ), x = line + j - 1; j > 0; j--, x-- ) {
                    if( *x == '\n' || *x == ' ' || *x == 9 ) {
                        *x = 0;
                    } else {
                        break;
                    }
                }
                _new( argv[argc], j + 1 );
                strcpy( argv[argc], line );
            }
            fclose( opt_file );
            ret = process_args( argc, argv );
            for( i = 0 ; i < argc; ++i ) {
                free( argv[i] );
            }
            free( argv );
            if( ret != argc ) {
                return( -1 );
            }
        } else {
            break;
        }
    }
    return( start_arg );
}

char *skip_blank( char *ptr )
/***************************/
{
    for( ; *ptr == ' ' || *ptr == '\t'; ++ptr );

    return( ptr );
}

bool read_line( void )
/********************/
{
    int                 c;
    char                ch;
    char                *buf;
    size_t              len;
    bool                eat_blank;

    eat_blank = false;
    for( ;; ) {
        ++Line_num;
        for( buf = Line_buf, len = 0;; ++buf ) {
#if defined __QNX__ || defined __UNIX__
            do {
                c = fgetc( In_file );
            } while( c == '\r' );
#else
            c = fgetc( In_file );
#endif
            if( c == EOF ) {
                return( false );
            }
            ++len;
            if( len > Line_buf_size ) {
                Line_buf_size += BUF_GROW;
                Line_buf = _realloc( Line_buf, Line_buf_size );
                buf = &Line_buf[len - 1];
            }

            ch = (char)c;
            if( ch == CH_SPACE_NOBREAK ) {
                ch = ' ';        // convert special blanks to regular blanks
            }
            *buf = ch;

            if( *buf == '\n' ) {
                if( eat_blank ) {
                    eat_blank = false;
                    if( *skip_blank( Line_buf ) == '\n' ) {
                        /* the 'exclude off, but eat blank line after' character
                           is used to do 'dummy' figures (to get the the
                           figure numbers right). Since real text has
                           to get ejected, an extra blank line is hard to
                           prevent. So this is used to eat one blank line
                           after the fake figure */
                        break;
                    }
                }
                ch = *Line_buf;
                if( ch == CH_EXCLUDE_OFF ) {
                    Exclude_on = false;
                    break;
                } else if( ch == CH_EXCLUDE_OFF_BLANK ) {
                    Exclude_on = false;
                    eat_blank = true;
                    break;
                } else if( ch == CH_EXCLUDE_ON ) {
                    Exclude_on = true;
                    break;
                } else if( Exclude_on ) {
                    break;
                } else {
                    *buf = '\0';
                    return( true );
                }
            }
        }
    }
}

char *whole_keyword_line( char *ptr )
/***********************************/
{
    char                buf[100];
    char                *end;
    size_t              len;

    /* this is a kludge case! If a line contains nothing but keywords,
       then parse it without generating a blank line. This
       can happen in GML when people use index entries to generate
       keywords, so we have to look for this case */

    for( ; *ptr == CH_CTX_KW; ) {
        end = strchr( ptr + 1, CH_CTX_KW );
        len = end - ptr - 1;
        memcpy( buf, ptr + 1, len );
        buf[len] = '\0';
        add_ctx_keyword( Curr_ctx, buf );
        ptr = end + 1;
        if( *ptr == '\0' ) {
            return( NULL );
        } else if( *ptr == ' ' ) {
            /* kludge fix cuz of GML: GML thinks that keywords are
               are real words, so it puts a space after them.
               This should fix that */
            ++ptr;
        }
    }

    return( ptr );
}

size_t trans_add_char( char ch, section_def *section, size_t *size )
/******************************************************************/
{
    section->section_size++;
    if( section->section_size > *size ) {
        *size += 1024;    // grow by a good, big amount
        _renew( section->section_text, *size );
    }
    section->section_text[section->section_size - 1] = ch;
    return( 1 );
}

size_t trans_add_str( const char *str, section_def *section, size_t *size )
/*************************************************************************/
{
    size_t      len;

    len = 0;
    for( ; *str != '\0'; ++str ) {
        trans_add_char( *str, section, size );
        ++len;
    }

    return( len );
}

size_t trans_add_str_nobreak( const char *str, section_def *section, size_t *size )
/*********************************************************************************/
{
    size_t      len;

    len = 0;
    for( ; *str != '\0'; ++str ) {
        if( *str != ' ' || Break_link ) {
            len = trans_add_char( *str, section, size );
        } else {
            /* non-breaking space */
            if( Output_type == OUT_RTF ) {
                len = trans_add_char( '\\', section, size );
                len += trans_add_char( '~', section, size );
            } else {
                /* IPF and InfoBench do not break alternate spaces */
                len = trans_add_char( CH_SPACE_NOBREAK, section, size );
            }
        }
    }

    return( len );
}

void add_link( const char *link_name )
/************************************/
{
    link_def            *link;

    _new( link, 1 );

    link->next = NULL;
    Link_list = link;
    _new( link->link_name, strlen( link_name ) + 1 );
    strcpy( link->link_name, link_name );
    link->line_num = Line_num;
}

bool find_keyword( ctx_def *ctx, const char *keyword )
/****************************************************/
{
    keylist_def         *keylist;

    for( keylist = ctx->keylist; keylist != NULL; keylist = keylist->next ) {
        if( stricmp( keylist->key->keyword, keyword ) == 0 ) {
            return( true );
        }
    }

    return( false );
}

keyword_def *find_keyword_all( const char *keyword )
/**************************************************/
{
    keyword_def         *key;

    for( key = Keyword_list; key != NULL; key = key->next ) {
        if( stricmp( key->keyword, keyword ) == 0 ) {
            return( key );
        }
    }

    return( NULL );
}

static void add_key_ctx( keyword_def *key, ctx_def *ctx )
/*******************************************************/
{
    if( key->ctx_list == NULL ) {
        _new( key->ctx_list, 1 );
        key->ctx_list_alloc = 1;
        key->ctx_list_size = 0;
    }

    ++key->ctx_list_size;
    if( key->ctx_list_size > key->ctx_list_alloc ) {
        key->ctx_list_alloc += 16;      // grow by a reasonable amount
        _renew( key->ctx_list, key->ctx_list_alloc );
    }
    key->ctx_list[key->ctx_list_size - 1] = ctx;
}


void add_ctx_keyword( ctx_def *ctx, const char *keyword )
/*******************************************************/
{
    keyword_def         *key;
    keylist_def         *keylist;

    if( !find_keyword( ctx, keyword ) ) {
        key = find_keyword_all( keyword );
        if( key != NULL ) {
            key->duplicate = true;
        } else {
            _new( key, 1 );
            key->duplicate = false;
            key->defined_ctx = ctx;
            _new( key->keyword, strlen( keyword ) + 1 );
            strcpy( key->keyword, keyword );
            key->next = Keyword_list;
            key->id = Keyword_id;
            ++Keyword_id;
            Keyword_list = key;

            key->ctx_list = NULL;
        }
        add_key_ctx( key, ctx );

        _new( keylist, 1 );
        keylist->key = key;
        keylist->next = ctx->keylist;
        ctx->keylist = keylist;
    }
}


static size_t trans_line( section_def *section, size_t size )
/***********************************************************/
{
    switch( Output_type ) {
    case OUT_RTF:
        return( rtf_trans_line( section, size ) );
    case OUT_IPF:
        return( ipf_trans_line( section, size ) );
    case OUT_IB:
        return( ib_trans_line( section, size ) );
    case OUT_HTML:
        return( html_trans_line( section, size ) );
    case OUT_WIKI:
        return( wiki_trans_line( section, size ) );
    }
    return( 0 );
}

static void topic_init( void )
/****************************/
{
    switch( Output_type ) {
    case OUT_RTF:
        rtf_topic_init();
        break;
    case OUT_IPF:
        ipf_topic_init();
        break;
    case OUT_IB:
        ib_topic_init();
        break;
    case OUT_HTML:
        html_topic_init();
        break;
    case OUT_WIKI:
        wiki_topic_init();
        break;
    }
}



static bool read_topic_text( ctx_def *ctx, bool is_blank, int order_num )
/***********************************************************************/
{
    bool                more_to_do;
    section_def         *section;
    section_def         **ins_section;
    size_t              sect_size;

    section = NULL;
    sect_size = 0;
    topic_init();
    for( ;; ) {
        more_to_do = read_line();
        if( !more_to_do ) {
            break;
        }
        if( *Line_buf == CH_CTX_DEF || *Line_buf == CH_TOPIC ) {
            break;
        }
        if( section == NULL ) {
            _new( section, 1 );
            section->section_text = NULL;
            section->section_size = 0;
            sect_size = 0;
        }
        sect_size = trans_line( section, sect_size );
    }

    if( section != NULL ) {
        for( ins_section = &ctx->section_list; *ins_section != NULL; ins_section = &((*ins_section)->next) ) {
            if( is_blank && !(*ins_section)->blank_order ) {
                break;
            } else if( order_num < (*ins_section)->order_num ) {
                break;
            }
        }
        section->next = *ins_section;
        *ins_section = section;
        section->blank_order = is_blank;
        section->order_num = order_num;
    }

    return( more_to_do );
}

ctx_def *find_ctx( const char *ctx_name )
/***************************************/
{
    ctx_def             *ctx;

    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( stricmp( ctx->ctx_name, ctx_name ) == 0 ) {
            return( ctx );
        }
    }

    return( NULL );
}

static char *skip_prep( char *str )
/*********************************/
{
    char                        ch;
    char                        *start;
    char                        *end;
    char                        *next;

    for( start = str; *start == ' ' || *start == '\t'; ++start );
    for( end = start; *end != ' ' && *end != '\t' && *end != '\0'; ++end );
    /* now 'end' points to the terminating char after the first word */
    if( start == end ) {
        /* no first word */
        return( str );
    }
    for( next = end; *next == ' ' || *next == '\t'; ++next );
    if( *next == '\0' ) {
        /* nothing after the first word */
        return( start );
    }

    ch = *end;
    *end = '\0';
    if( stricmp( "the", start ) == 0 ||
        stricmp( "a", start ) == 0 ||
        stricmp( "an", start ) == 0 ) {
        start = next;
    }
    *end = ch;

    return( start );
}

static browse_def *add_browse( const char *browse_name, ctx_def *ctx )
/********************************************************************/
{
    browse_def          *browse;
    browse_def          *browse_prev;
    browse_ctx          *b_ctx;
    browse_ctx          **b_ctx_list;

    if( browse_name == NULL ) {
        return( NULL );
    }

    browse_prev = NULL;
    for( browse = Browse_list;; browse_prev = browse, browse = browse->next ) {
        if( browse == NULL ) {
            _new( browse, 1 );
            _new( browse->browse_name, strlen( browse_name ) + 1 );
            strcpy( browse->browse_name, browse_name );
            browse->ctx_list = NULL;
            browse->next =  NULL;
            /* keep the browse list in the order they are parsed */
            if( browse_prev == NULL ) {
                Browse_list = browse;
            } else {
                browse_prev->next = browse;
            }
            break;
        } else if( stricmp( browse->browse_name, browse_name ) == 0 ) {
            break;
        }
    }

    _new( b_ctx, 1 );
    b_ctx->ctx = ctx;

    for( b_ctx_list = &browse->ctx_list; *b_ctx_list != NULL; b_ctx_list = &((*b_ctx_list)->next) ) {
        if( Browse_sort && stricmp( skip_prep( ctx->title ), skip_prep( (*b_ctx_list)->ctx->title ) ) < 0 ) {
            break;
        }
    }

    b_ctx->next = *b_ctx_list;
    *b_ctx_list = b_ctx;

    return( browse );
}


static void add_ctx( ctx_def *ctx, const char *title, char *keywords, const char *browse_name, int head_level )
/*************************************************************************************************************/
{
    char                *ptr;
    char                *end;
    char                ch;
    ctx_def             *up_ctx;
    ctx_def             *ctx_list;

    if( title != NULL && ctx->title == NULL ) {
        _new( ctx->title, strlen( title ) + 1 );
        strcpy( ctx->title, title );
    }
    if( keywords != NULL && ctx->keylist == NULL && *skip_blank( keywords ) != '\0' ) {
        for( ptr = keywords;; ) {
            for( end = ptr; *end != ',' && *end != ';' && *end != '\0'; ++end );
            ch = *end;
            *end = '\0';
            if( !find_keyword( ctx, ptr ) ) {
                add_ctx_keyword( ctx, ptr );
            }
            *end = ch;
            if( ch == '\0' ) {
                break;
            }
            ptr = end + 1;
        }
    }
    if( Do_topic_keyword && ( keywords != NULL )) {
        add_ctx_keyword( ctx, ctx->title );
    }

    ctx->head_level = head_level;
    up_ctx = NULL;
    for( ctx_list = Ctx_list; ctx_list != ctx; ctx_list = ctx_list->next ) {
        if( ctx_list->head_level < head_level ) {
            up_ctx = ctx_list;
        }
    }
    ctx->up_ctx = up_ctx;
    ctx->browse = add_browse( browse_name, ctx );
}

static ctx_def *init_ctx( char *ctx_name )
/****************************************/
{
    ctx_def             *ctx;

    _new( ctx, 1 );
    ctx->section_list = NULL;
    if( Ctx_list == NULL ) {
        Ctx_list = ctx;
    } else {
        *Ctx_list_end = ctx;
    }
    Ctx_list_end = &(ctx->next);
    ctx->next = NULL;
    ctx->keylist = NULL;
    ctx->browse = NULL;
    ctx->title = NULL;
    ctx->ctx_id = -1;
    ctx->empty = true;
    ctx->req_by_link = false;
    _new( ctx->ctx_name, strlen( ctx_name ) + 1 );
    strcpy( ctx->ctx_name, ctx_name );

    return( ctx );
}

static ctx_def *define_ctx( void )
/********************************/
{
    char                *title;
    char                *keywords;
    char                *browse_name;
    char                *ctx_name;
    ctx_def             *ctx;
    ctx_def             *old_ctx;
    int                 title_fmt;
    int                 head_level;
    char                *ptr;
    char                ch, o_ch;
    int                 i;

    Delim[0] = CH_CTX_DEF;
    ptr = strtok( Line_buf + 1, Delim );
    head_level = atoi( ptr );
    ctx_name = strtok( NULL, Delim );

    title_fmt = TITLE_FMT_DEFAULT;
    if( *ctx_name == CH_TOPIC_LN ) {
        title_fmt = TITLE_FMT_LINE;
        ++ctx_name;
    } else if( *ctx_name == CH_TOPIC_NOLN ) {
        title_fmt = TITLE_FMT_NOLINE;
        ++ctx_name;
    }
    ctx = find_ctx( ctx_name );
    old_ctx = ctx;
    if( ctx != NULL && ctx->title != NULL ) {
        if(( head_level != 0 ) && ( ctx->head_level != 0 )) {
            printf( "topic already exists: %s\n", ctx_name );
            warning( ERR_CTX_EXISTS, true );
        }
        for( i = 0; i < strlen( ctx_name ); ++i ) {
            o_ch = ctx_name[i];
            for( ch = 'A'; ch < 'Z'; ++ch ) {
                ctx_name[i] = ch;
                if( find_ctx( ctx_name ) == NULL ) {
                    break;
                }
            }
            if( ch != 'Z' ) {
                ctx = NULL;
                break;
            }
            ctx_name[i] = o_ch;
        }
    }

    title = strtok( NULL, Delim );
    if( title == NULL ) {
        error( ERR_NO_TITLE, true );
    }

    browse_name = strtok( NULL, Delim );
    if( browse_name != NULL ) {
        keywords = strtok( NULL, Delim );
    } else {
        keywords = NULL;
    }

    if( ctx == NULL ) {
        ctx = init_ctx( ctx_name );
    }
    ctx->title_fmt = title_fmt;
    add_ctx( ctx, title, keywords, browse_name, head_level );

    if(( old_ctx != NULL ) && ( old_ctx != ctx ) && ( old_ctx->head_level == 0 )) {
        ptr = old_ctx->ctx_name;
        old_ctx->ctx_name = ctx->ctx_name;
        ctx->ctx_name = ptr;
    }

    return( ctx );
}

static bool read_ctx_def( void )
/******************************/
{
    return( read_topic_text( define_ctx(), true, 0 ) );
}

static bool read_ctx_topic( void )
/********************************/
{
    char                *ctx_name;
    ctx_def             *ctx;
    char                *order_str;

    Delim[0] = CH_TOPIC;
    ctx_name = strtok( Line_buf, Delim );

    ctx = find_ctx( ctx_name );

    if( ctx == NULL ) {
        ctx = init_ctx( ctx_name );
        add_ctx( ctx, NULL, NULL, NULL, -1 );
    }
    Curr_ctx = ctx;

    order_str = strtok( NULL, Delim );
    if( order_str == NULL ) {
        return( read_topic_text( ctx, true, 0 ) );
    } else {
        return( read_topic_text( ctx, false, atoi( order_str ) ) );
    }
}

static bool read_topic( void )
/****************************/
{
    switch( *Line_buf ) {
    case CH_CTX_DEF:
        return( read_ctx_def() );
    case CH_TOPIC:
        return( read_ctx_topic() );
    case ' ':
    case '\t':
    case '\0':
        if( *skip_blank( Line_buf ) == '\0' ) {
            /* due to a bug on GML, skip completely blank lines
               between topics, like this */
            return( read_line() );
        }
        /* fall through */
    default:
        printf( "Error in input file on line %d.\n", Line_num );
        printf( "****%s\n", Error_list[ERR_NO_TOPIC] );
    }
    return( false );
}

static void read_whp_file( void )
/*******************************/
{
    if( !read_line() ) {
        return;
    }
    for( ;; ) {
        if( !read_topic() ) {
            break;
        }
    }
}

static void set_browse_numbers( void )
/************************************/
{
    browse_def                  *browse;
    browse_ctx                  *b_ctx;
    int                         order;

    for( browse = Browse_list; browse != NULL; browse = browse->next ) {
        order = 1;
        for( b_ctx = browse->ctx_list; b_ctx != NULL; b_ctx = b_ctx->next ) {
            b_ctx->ctx->browse_num = order;
            ++order;
        }
    }
}

static void sort_ctx_list( void )
/*******************************/
{
    ctx_def                     *ctx;
    ctx_def                     *sort_list;
    ctx_def                     **sort_spot;
    ctx_def                     *next_ctx;
    char                        buf[100];

    sort_list = NULL;
    for( ctx = Ctx_list; ctx != NULL; ctx = next_ctx ) {
        next_ctx = ctx->next;
        if( ctx->title == NULL ) {
            /* ctx item without a definition */
            sprintf( buf, "The context id '%s' is used but not defined", ctx->ctx_name );
            error_str( buf );
        }
        for( sort_spot = &sort_list; *sort_spot != NULL; sort_spot = &((*sort_spot)->next) ) {
            if( stricmp( skip_prep( ctx->title ), skip_prep( (*sort_spot)->title ) ) < 0 ) {
                break;
            }
        }
        ctx->next = *sort_spot;
        *sort_spot = ctx;
    }
    Ctx_list = sort_list;
}

bool is_special_topic( ctx_def *ctx, bool dump_popup )
/****************************************************/
{
    bool                        res;

    if( ctx == NULL ) {
        res = false;
    } else {
        res = ( stricmp( ctx->ctx_name, "table_of_contents" ) == 0 ||
                stricmp( ctx->ctx_name, "index_of_topics" ) == 0 ||
                stricmp( ctx->ctx_name, "keyword_search" ) == 0 ||
                stricmp( ctx->ctx_name, "browse_lists" ) == 0  ||
                ( ctx->title_fmt == TITLE_FMT_NOLINE && dump_popup ) );
    }
    return( res );
}

static void output_idx_file( void )
/*********************************/
{
    char                        ch;
    char                        ch2;
    ctx_def                     *ctx;
    char                        pfx[20];
    bool                        new_topic;

    ch = 0;
    if( Index_gml_fmt ) {
        whp_fprintf( Idx_file, ":H1.%s\n", Gen_titles[GEN_TITLE_INDEX][Title_case] );
    } else {
        whp_fprintf( Idx_file, "%s:\n\n", Gen_titles[GEN_TITLE_INDEX][Title_case] );
    }
    new_topic = false;
    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( ctx->empty || ( Dump_popup_i && ctx->title_fmt == TITLE_FMT_NOLINE ) ) {
            continue;
        }
        ch2 = (char)toupper( *(unsigned char *)skip_prep( ctx->title ) );
        if( ch != ch2 ) {
            if( Index_gml_fmt ) {
                if( ch == 0 ) {
                    sprintf( pfx, "%c\n:pb.", CH_DLIST_START );
                } else {
                    strcpy( pfx, ":p." );
                }
                whp_fprintf( Idx_file, "%s%c- %c -\n", pfx, CH_DLIST_TERM, ch2 );
                new_topic = true;
            } else {
                whp_fprintf( Idx_file, "- %c -\n", ch2 );
            }
            ch = ch2;
        }
        if( Index_gml_fmt ) {
            whp_fprintf( Idx_file, ":pb." );
            if( new_topic ) {
                whp_fprintf( Idx_file, "%c", CH_DLIST_DESC );
                new_topic = false;
            }
            whp_fprintf( Idx_file, "%c%s%c%s%c\n",
                CH_HLINK, ctx->ctx_name, CH_HLINK, ctx->title, CH_HLINK );
        } else {
            whp_fprintf( Idx_file, "    %c%s%c%s%c\n",
                CH_HLINK, ctx->ctx_name, CH_HLINK, ctx->title, CH_HLINK );
        }
    }
    if( ch != 0 && Index_gml_fmt ) {
        whp_fprintf( Idx_file, ":pb.%c\n\n", CH_DLIST_END );
    }
}

static int kw_cmp( const void *_kw1, const void *_kw2 )
/*****************************************************/
{
    keyword_def **kw1 = (keyword_def**)_kw1;
    keyword_def **kw2 = (keyword_def**)_kw2;

    return( stricmp( (*kw1)->keyword, (*kw2)->keyword ) );
}

static int ctx_cmp( const void *_ctx1, const void *_ctx2 )
/********************************************************/
{
    ctx_def **ctx1 = (ctx_def**)_ctx1;
    ctx_def **ctx2 = (ctx_def**)_ctx2;

    return( stricmp( (*ctx1)->title, (*ctx2)->title ) );
}

// removes empty contexts from a given keyword_def if Remove_empty == true
static void compress_kw( keyword_def *kw )
/****************************************/
{
    int         o_size;
    int         n_size = 0;
    int         o_idx;
    int         n_idx = 0;

    if( Remove_empty ) {
        o_size = kw->ctx_list_size;
        for( o_idx = 0; o_idx < o_size; ++o_idx ) {
            if( !kw->ctx_list[o_idx]->empty ) {
                kw->ctx_list[n_idx] = kw->ctx_list[o_idx];
                n_idx++;
                n_size++;
            }
        }
        kw->ctx_list_size = n_size;
    }
}

static void output_kw_file( void )
/********************************/
{
    keyword_def                 *temp_kw;   // used for grabbing keywords
    keyword_def                 **kw;       // array of *keyword_def's
    ctx_def                     **ctx;      // array of *ctx_def's
    int                         ctx_num;    // number of ctx we're on
    int                         kw_num = 0; // number of keywords
    int                         i;          // counter
    bool                        title;      // whether we've printed this kw

    // output header
    whp_fprintf( KW_file, ":H1.%s\n", Gen_titles[GEN_TITLE_KEYWORD][Title_case] );
    whp_fprintf( KW_file, ":pb.%cc\n", CH_SLIST_START );

    // count the number of keywords in our list
    for( temp_kw = Keyword_list; temp_kw !=NULL; temp_kw = temp_kw->next )
        kw_num++;

    // if we've got any keywords ...
    if( kw_num > 0 ) {
        // ... we allocate an array of pointers to keywords ...
        _new( kw, kw_num );

        // ... fill it up ...
        kw_num = 0;
        for( temp_kw = Keyword_list; temp_kw !=NULL; temp_kw = temp_kw->next ) {
            kw[kw_num] = temp_kw;
            kw_num++;
        }

        // .. and then sort it.
        qsort( kw, kw_num, sizeof( keyword_def * ), kw_cmp );

        for( i = 0; i < kw_num; i++ ) {
            title = false;

            // remove empty contexts if we need to
            compress_kw( kw[i] );

            // get our array of contexts
            ctx = kw[i]->ctx_list;

            // we treat keywords with only one context as a special case
            if( kw[i]->ctx_list_size == 1 ) {
                if( !is_special_topic( ctx[0], Dump_popup_k ) ) {
                    whp_fprintf( KW_file,
                                ":pb.%c:pb.%c%s%c%s%c\n",
                                CH_LIST_ITEM,
                                CH_HLINK,
                                ctx[0]->ctx_name,
                                CH_HLINK,
                                kw[i]->keyword,
                                CH_HLINK );
                }
            } else if( kw[i]->ctx_list_size > 1 ) {
                // sort the list of contexts by title.
                qsort( ctx, kw[i]->ctx_list_size, sizeof( ctx_def * ), ctx_cmp );

                for( ctx_num = 0; ctx_num < kw[i]->ctx_list_size; ctx_num++ ) {
                    // if the context is not special output a hyperlink
                    if( !is_special_topic( ctx[ctx_num], Dump_popup_k ) ) {
                        if( !title ) {
                            // output keyword
                            whp_fprintf( KW_file,
                                        ":pb.%c:pb.%cb%c%s%c\n"
                                        ":pb.%cc\n",

                                        CH_LIST_ITEM,
                                        CH_FONTSTYLE_START,
                                        CH_FONTSTYLE_START,
                                        kw[i]->keyword,
                                        CH_FONTSTYLE_END,

                                        CH_SLIST_START );
                            title = true;
                        }
                        whp_fprintf( KW_file, ":pb.%c%c%s%c%s%c\n",
                                CH_LIST_ITEM,
                                CH_HLINK,
                                ctx[ctx_num]->ctx_name,
                                CH_HLINK,
                                ctx[ctx_num]->title,
                                CH_HLINK );
                    }
                    // go to the next context on our list
                }
                if( title ) {
                    whp_fprintf( KW_file, ":pb.%c\n", CH_SLIST_END );
                }
            }
        }
        free( kw );
    }

    // the end
    whp_fprintf( KW_file, ":pb.%c\n", CH_SLIST_END );
}

static void output_blist_file( void )
/***********************************/
{
    browse_def                  *browse;
    browse_ctx                  *b_ctx;
    browse_ctx                  *b_ctx_next;
    ctx_def                     *ctx;
    char                        *pfx;

    whp_fprintf( Blist_file, ":H1.%s\n%c\n",
                Gen_titles[GEN_TITLE_BROWSE][Title_case], CH_DLIST_START );
    pfx = ":pb.";
    for( browse = Browse_list; browse != NULL; browse = browse->next ) {
        for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
            if( stricmp( ctx->title, browse->browse_name ) == 0 ) {
                break;
            }
        }
        if( ctx != NULL ) {
            if( is_special_topic( ctx, Dump_popup_b ) ) {
                /* kludge fix to make life easier for the FET books */
                continue;
            }

            whp_fprintf( Blist_file, "%s%c%cb%c%c%s%c%s%c%c\n",
                        pfx, CH_DLIST_TERM, CH_FONTSTYLE_START,
                        CH_FONTSTYLE_START, CH_HLINK, ctx->ctx_name,
                        CH_HLINK, ctx->title, CH_HLINK, CH_FONTSTYLE_END );
        } else {
            whp_fprintf( Blist_file, "%s%c%s\n", pfx, CH_DLIST_TERM,
                                                    browse->browse_name );
        }
        pfx = ":p.";

        whp_fprintf( Blist_file, ":pb.%c", CH_DLIST_DESC );
        for( b_ctx = browse->ctx_list; ; ) {
            b_ctx_next = b_ctx->next;
            if( b_ctx->ctx != ctx && !b_ctx->ctx->empty ) {
                whp_fprintf( Blist_file, "%c%s%c%s%c", CH_HLINK,
                                            b_ctx->ctx->ctx_name, CH_HLINK,
                                            b_ctx->ctx->title, CH_HLINK );
                if( b_ctx_next != NULL ) {
                    whp_fprintf( Blist_file, "\n:pb." );
                }
            }
            b_ctx = b_ctx_next;
            if( b_ctx == NULL ) {
                break;
            }
        }
        whp_fprintf( Blist_file, "\n" );
    }
    whp_fprintf( Blist_file, ":pb.%c\n\n", CH_DLIST_END );
}

static void output_contents_file( void )
/**************************************/
{
    ctx_def                     *ctx;
    int                         level;
    int                         i;

    whp_fprintf( Contents_file, ":H1.%s\n",
                                Gen_titles[GEN_TITLE_CONTENTS][Title_case] );
    level = -1;
    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( is_special_topic( ctx, Dump_popup_t ) ) {
            continue;
        }
        if( ctx->head_level > level ) {
            for( i = level + 1; i <= ctx->head_level; ++i ) {
                whp_fprintf( Contents_file, ":pb.%cc\n", CH_SLIST_START );
            }
            level = ctx->head_level;
        } else if( ctx->head_level < level ) {
            for( i = ctx->head_level + 1; i <=level; ++i ) {
                whp_fprintf( Contents_file, ":pb.%c\n", CH_SLIST_END );
            }
            level = ctx->head_level;
        }
        whp_fprintf( Contents_file, ":pb.%c", CH_LIST_ITEM );
        if( level <= 1 ) {
            whp_fprintf( Contents_file, ":pb.%chelv%c12%c%cb%c",
                        CH_FONTTYPE, CH_FONTTYPE, CH_FONTTYPE,
                        CH_FONTSTYLE_START, CH_FONTSTYLE_START );
        }
        if( ctx->empty ) {
            whp_fprintf( Contents_file, "%s", ctx->title );
        } else {
            whp_fprintf( Contents_file, "%c%s%c%s%c",
                    CH_HLINK, ctx->ctx_name, CH_HLINK, ctx->title, CH_HLINK );
        }
        if( level <= 1 ) {
            whp_fprintf( Contents_file, "%c%chelv%c10%c",
                    CH_FONTSTYLE_END, CH_FONTTYPE, CH_FONTTYPE, CH_FONTTYPE );
        }
        whp_fprintf( Contents_file, "\n" );
    }
    for( i = -1; i < level; ++i ) {
        whp_fprintf( Contents_file, ":pb.%c\n", CH_SLIST_END );
    }

}


static void output_def_file( void )
/*********************************/
{
    ctx_def                     *ctx;
    char                        *buf;
    size_t                      len;
    size_t                      max_len;

    whp_fprintf( Def_file, "/* This file was created by WHPCVT.EXE. DO NOT MODIFY BY HAND! */\n\n" );

    max_len = 0;
    buf = NULL;
    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( ctx->empty ) {
            continue;
        }
        len = strlen( ctx->ctx_name ) + 1;
        if( len > max_len ) {
            buf = realloc( buf, len );
            max_len = len;
        }
        strcpy( buf, ctx->ctx_name );
        strupr( buf );
        whp_fprintf( Def_file, "#define %s%-50s\t%d\n", HELP_PREFIX, buf, ctx->ctx_id );
    }
    free( buf );
}

static void output_hdef_file( void )
/**********************************/
{
    ctx_def                     *ctx;

    whp_fprintf( Hdef_file, "/* This file was created by WHPCVT.EXE. DO NOT MODIFY BY HAND! */\n\n" );

    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( !ctx->empty ) {
            whp_fprintf( Hdef_file, "#define %-50s\t%d\n", ctx->ctx_name, ctx->ctx_id );
        }
    }
}

static void read_ctx_ids( void )
/******************************/
{
    ctx_def                     *ctx;
    char                        *ptr;

    for( ; read_line(); ) {
        if( Line_buf[0] == '#' ) {
            Delim[0] = ' ';
            ptr = strtok( Line_buf, Delim );
            if( ptr == NULL ) {
                continue;
            }
            ptr = strtok( NULL, Delim );
            if( ptr == NULL ) {
                continue;
            }

            for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
                if( stricmp( ptr + strlen( HELP_PREFIX ), ctx->ctx_name ) == 0 ) {
                    ptr = strtok( NULL, Delim );
                    if( ptr != NULL ) {
                        ctx->ctx_id = atoi( ptr );
                    }
                    break;
                }
            }
        }
    }
}

static void set_ctx_ids( void )
/*****************************/
{
    ctx_def                     *ctx;
    int                         ctx_id;

    ctx_id = 0;
    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( ctx->ctx_id != -1 && ctx->ctx_id > ctx_id ) {
            ctx_id = ctx->ctx_id;
        }
    }

    for( ctx = Ctx_list; ctx != NULL; ctx = ctx->next ) {
        if( ctx->ctx_id == -1 ) {
            ++ctx_id;
            ctx->ctx_id = ctx_id;
        }
    }
}

static void check_links( void )
/*****************************/
{
    ctx_def             *ctx;
    link_def            *link;
    bool                err;

    err = false;
    for( link = Link_list; link != NULL; link = link->next ) {
        ctx = find_ctx( link->link_name );
        if( ctx == NULL ) {
            printf( "undefined link: %s\n", link->link_name );
            warning_line( ERR_UNDEF_LINK, link->line_num );
            err = true;
        } else if( ctx->empty && Remove_empty ) {
            if( !Keep_link_topics ) {
                warning_line( ERR_EMPTY_LINK, link->line_num );
                err = true;
            } else {
                ctx->req_by_link = true;
            }
        }
    }
    if( err ) {
        error_quit();
    }
}

int main( int argc, char *argv[] )
/********************************/
{
    char                file[200];
    int                 start_arg;
    void                *start_alloc;
    int                 size;
    char                *dot;
    char                *slash;
    int                 rc;

    rc = -1;
    if( argc < 1 ) {
        print_help();
        goto error_exit;
    }

    /* This program can be a memory pig, so, to avoid fragmentation,
       do some big allocs to block out the space */
    for( size = 10; size > 0; --size ) {
        start_alloc = malloc( size * 1000 * 1024 );
        if( start_alloc != NULL ) {
            free( start_alloc );
            break;
        }
    }

    argc--;
    argv++;

    start_arg = valid_args( argc, argv );
    if( start_arg < 0 ) {
        print_help();
        goto error_exit;
    }

    strcpy( file, argv[start_arg] );
    dot = strrchr( file, '.' );
    slash = strrchr( file, '\\' );
    if( dot == NULL || ( slash != NULL && dot < slash ) ) {
        strcat( file, EXT_INPUT_FILE );
    }

    Line_num = 0;
    In_file = fopen( file, "r" );
    if( In_file == NULL ) {
        printf( "Could not open input file: %s\n", file );
        goto error_exit;
    }

    /* this is for the RTF 'Up' button support */
    strcpy( Help_fname, file );
    strcpy( strrchr( Help_fname, '.' ), EXT_HLP_FILE );

    if( Do_index ) {
        strcpy( strrchr( file, '.' ), EXT_IDX_FILE );
        Idx_file = fopen( file, "w" );
        if( Idx_file == NULL ) {
            printf( "Could not open index file: %s\n", file );
            goto error_exit;
        }
    }

    if( Do_keywords ) {
        strcpy( strrchr( file, '.' ), EXT_KW_FILE );
        KW_file = fopen( file, "w" );
        if( KW_file == NULL ) {
            printf( "Could not open index file: %s\n", file );
            goto error_exit;
        }
    }

    if( Do_blist ) {
        strcpy( strrchr( file, '.' ), EXT_BLIST_FILE );
        Blist_file = fopen( file, "w" );
        if( Blist_file == NULL ) {
            printf( "Could not open browse list file: %s\n", file );
            goto error_exit;
        }
    }

    if( Do_contents ) {
        strcpy( strrchr( file, '.' ), EXT_TBL_FILE );
        Contents_file = fopen( file, "w" );
        if( Contents_file == NULL ) {
            printf( "Could not open table of contents file: %s\n", file );
            goto error_exit;
        }
    }


    switch( Output_type ) {
    case OUT_RTF:
        strcpy( Output_file_ext, EXT_OUTRTF_FILE );
        break;
    case OUT_IPF:
        strcpy( Output_file_ext, EXT_OUTIPF_FILE );
        break;
    case OUT_IB:
        strcpy( Output_file_ext, EXT_OUTIB_FILE );
        break;
    case OUT_HTML:
        strcpy( Output_file_ext, EXT_OUTHTML_FILE );
        break;
    case OUT_WIKI:
        strcpy( Output_file_ext, EXT_OUTWIKI_FILE );
        break;
    }

    if( argc == start_arg + 1 ) {
        strcpy( strrchr( file, '.' ), Output_file_ext );
    } else {
        strcpy( file, argv[start_arg + 1] );
        dot = strrchr( file, '.' );
        slash = strrchr( file, '\\' );
        if( dot == NULL || ( slash != NULL && dot < slash ) ) {
            strcat( file, Output_file_ext );
        }
    }
    Out_file = fopen( file, "w" );
    if( Out_file == NULL ) {
        printf( "Could not open output file: %s\n", file );
        goto error_exit;
    }

    if( 0 != setjmp( Jmp_buf ) ) {
        goto error_exit;
    }

    read_whp_file();

    set_browse_numbers();

    fclose( In_file );
    In_file = NULL;

    if( Do_ctx_ids ) {
        strcpy( strrchr( file, '.' ), EXT_DEF_FILE );
        In_file = fopen( file, "r" );
        if( In_file != NULL ) {
            read_ctx_ids();
            fclose( In_file );
            In_file = NULL;
        }
    }
    set_ctx_ids();

    if( Do_def ) {
        strcpy( strrchr( file, '.' ), EXT_DEF_FILE );
        Def_file = fopen( file, "w" );
        if( Def_file == NULL ) {
            printf( "Could not open define file: %s\n", file );
            goto error_exit;
        }
    }

    if( Do_hdef ) {
        strcpy( strrchr( file, '.' ), EXT_HDEF_FILE );
        Hdef_file = fopen( file, "w" );
        if( Hdef_file == NULL ) {
            printf( "Could not open help define file: %s\n", file );
            goto error_exit;
        }
    }

    check_links();

    switch( Output_type ) {
    case OUT_RTF:
        Brace_check = true;
        rtf_output_file();
        Brace_check = false;
        if( Brace_count != 0 ) {
            if( Brace_count < 0 ) {
                printf( "Too many braces ('}'). Off by %d\n", -Brace_count );
            } else {
                printf( "Not enough braces ('}'). Off by %d\n", Brace_count );
            }
            goto error_exit;
        }
        break;
    case OUT_IPF:
        ipf_output_file();
        break;
    case OUT_IB:
        ib_output_file();
        break;
    case OUT_HTML:
        html_output_file();
        break;
    case OUT_WIKI:
        wiki_output_file();
        break;
    }
    if( Do_contents ) {
        /* do this before sorting the context lists */
        output_contents_file();
    }
    sort_ctx_list();
    if( Do_index ) {
        output_idx_file();
    }
    if( Do_keywords ) {
        output_kw_file();
    }
    if( Do_blist ) {
        output_blist_file();
    }
    if( Do_def ) {
        output_def_file();
    }
    if( Do_hdef ) {
        output_hdef_file();
    }
    rc = 0;

error_exit:
    if( Idx_file != NULL ) {
        fclose( Idx_file );
    }
    if( KW_file != NULL ) {
        fclose( KW_file );
    }
    if( Blist_file != NULL ) {
        fclose( Blist_file );
    }
    if( Contents_file != NULL ) {
        fclose( Contents_file );
    }
    if( Def_file != NULL ) {
        fclose( Def_file );
    }
    if( Hdef_file != NULL ) {
        fclose( Hdef_file );
    }
    if( Out_file != NULL ) {
        fclose( Out_file );
    }

    return( rc );
}
