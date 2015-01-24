/* The MIT License (MIT)
 *
 * Copyright (c) 2014 - 2015, Andreas Merkle
 * http://www.blue-andi.de
 * vscp@blue-andi.de
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*******************************************************************************
    DESCRIPTION
*******************************************************************************/
/**
@brief  Decision matrix XML loader
@file   dm_xml.c
@author Andreas Merkle, http://www.blue-andi.de

@section desc Description
@see dm_xml.h

@section svn Subversion
$Author: amerkle $
$Rev: 449 $
$Date: 2015-01-05 20:23:52 +0100 (Mo, 05 Jan 2015) $
*******************************************************************************/

/*******************************************************************************
    INCLUDES
*******************************************************************************/
#include "dm_xml.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "expat.h"

/*******************************************************************************
    COMPILER SWITCHES
*******************************************************************************/

/** Set it to 1 to shows log debug information. */
#define DM_XML_ENABLE_DEBUG_INFO    0

/*******************************************************************************
    CONSTANTS
*******************************************************************************/

/** Current standard decision matrix xml version */
#define DM_STD_XML_VERSION  "1.0"

/** Current decision matrix extension xml version */
#define DM_EXT_XML_VERSION  "1.0"

/** Current decision matrix next generation xml version */
#define DM_NG_XML_VERSION   "1.0"

/** Decision matrix level (node level) */
#define DM_LEVEL            "1"

/*******************************************************************************
    MACROS
*******************************************************************************/

/** Validate pointer */
#define DM_XML_VALIDATE_PTR(__ptr)  do{ if (NULL == (__ptr)) return DM_XML_RET_ENULL; }while(0)

/*******************************************************************************
    TYPES AND STRUCTURES
*******************************************************************************/

/** This type defines the decision matrix xml parser context. */
typedef struct
{
    XML_Parser              xmlParser;  /**< XML parser */
    
    struct _dm_xml_Element* children;   /**< Children of the current element */
    uint8_t                 depth;      /**< Idendention depth */
    
#if VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM )

    vscp_dm_MatrixRow*  dmStorage;  /**< Standard decision matrix storage */

#endif  /* VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM ) */

#if VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM_EXTENSION )

    vscp_dm_ExtRow*     extStorage; /**< Decision matrix extension storage */

#endif  /* VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM_EXTENSION ) */

    uint8_t             maxRows;    /**< Max. decision matrix rows */
    uint8_t             index;      /**< Decision matrix row index */
    
    uint8_t             id;         /**< Decision matrix extension: Data byte id */
    BOOL                dmFound;    /**< Flag: Decision matrix found */
    BOOL                rowFound;   /**< Flag: Decision matrix row found */
    BOOL                dataFound;  /**< Flag: Decision matrix extension data found */
    
    XML_Char*           buffer;     /**< Character buffer, used for the text between xml tags. */
    BOOL                isAborted;  /**< Parsing aborted or not */

} dm_xml_Context;

typedef DM_XML_RET (*dm_xml_AttrCb)(dm_xml_Context* const con, char const * const name, char const * const value);

typedef struct _dm_xml_Attribute
{
    char*           name;
    BOOL            required;
    dm_xml_AttrCb   callBack;

} dm_xml_Attribute;

typedef DM_XML_RET (*dm_xml_ElementStartCb)(dm_xml_Context* const con);

typedef DM_XML_RET (*dm_xml_ElementEndCb)(dm_xml_Context* const con);

typedef struct _dm_xml_Element
{
    char*                       name;
    BOOL                        required;
    dm_xml_ElementStartCb       startCallback;
    dm_xml_ElementEndCb         endCallback;
    dm_xml_Attribute const *    attributes;
    struct _dm_xml_Element*     parent;
    struct _dm_xml_Element*     children;
    
} dm_xml_Element;

/*******************************************************************************
    PROTOTYPES
*******************************************************************************/

static long dm_xml_getFileSize(FILE* fd);
static char* dm_xml_loadFileToMem(char const * const fileName);
static int dm_xml_strcmpi(char const * str1, char const * str2);

#if VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM )

static unsigned long dm_xml_getDataValue(char const * const str);
static char const * const dm_xml_getAttribute(const XML_Char **atts, char const * const name);
static DM_XML_RET dm_xml_parseStd(dm_xml_Context* const con, char const * const xml);
static void dm_xml_characterDataHandler(void *userData, const XML_Char *str, int len);

#endif  /* VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM ) */

#if VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM_EXTENSION )

static DM_XML_RET dm_xml_parseExt(dm_xml_Context* const con, char const * const xml);
static void XMLCALL dm_xml_startElementExt(void *userData, const XML_Char *name, const XML_Char **atts);
static void XMLCALL dm_xml_endElementExt(void *userData, const XML_Char *name);

#endif  /* VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM_EXTENSION ) */

static void XMLCALL dm_xml_startElement(void *userData, const XML_Char *name, const XML_Char **atts);
static void XMLCALL dm_xml_endElement(void *userData, const XML_Char *name);
static DM_XML_RET dm_xml_handleDmAttrVersion(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleDmAttrLevel(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleDmAttrType(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleRowAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleRowStart(dm_xml_Context* const con);
static DM_XML_RET dm_xml_handleRowEnd(dm_xml_Context* const con);
static DM_XML_RET dm_xml_handleOAddrAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleHardcodedAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleMaskAttrClass(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleMaskAttrType(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleFilterAttrClass(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleFilterAttrType(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleZoneAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleSubZoneAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value);
static DM_XML_RET dm_xml_handleOAddrEnd(dm_xml_Context* const con);
static DM_XML_RET dm_xml_handleActionEnd(dm_xml_Context* const con);
static DM_XML_RET dm_xml_handleParamEnd(dm_xml_Context* const con);

/*******************************************************************************
    LOCAL VARIABLES
*******************************************************************************/

/** Marks the end or the bottom of the parse tree. In other words, every element which has no children, use that one. */
static dm_xml_Element           dm_xml_elemTail[] =
{
    /* Name             Required    Start                   End                     Attributes              Parent   Children */
    { NULL,             FALSE,      NULL,                   NULL,                   NULL,                   NULL,    NULL               }
};

/** Attributes of the SUBZONE element. */
static const dm_xml_Attribute   dm_xml_attrSUBZONE[] =
{
    /* Name         Required    Callback */
    { "enabled",    TRUE,       dm_xml_handleSubZoneAttrEnabled     },
    { NULL,         FALSE,      NULL                                }
};

/** Attributes of the ZONE element. */
static const dm_xml_Attribute   dm_xml_attrZONE[] =
{
    /* Name         Required    Callback */
    { "enabled",    TRUE,       dm_xml_handleZoneAttrEnabled        },
    { NULL,         FALSE,      NULL                                }
};

/** Attributes of the FILTER element. */
static const dm_xml_Attribute   dm_xml_attrFILTER[] =
{
    /* Name         Required    Callback */
    { "class",      TRUE,       dm_xml_handleFilterAttrClass        },
    { "type",       TRUE,       dm_xml_handleFilterAttrType         },
    { NULL,         FALSE,      NULL                                }
};

/** Attributes of the MASK element. */
static const dm_xml_Attribute   dm_xml_attrMASK[] =
{
    /* Name         Required    Callback */
    { "class",      TRUE,       dm_xml_handleMaskAttrClass          },
    { "type",       TRUE,       dm_xml_handleMaskAttrType           },
    { NULL,         FALSE,      NULL                                }
};

/** Attributes of the HARDCODED element. */
static const dm_xml_Attribute   dm_xml_attrHARDCODED[] =
{
    /* Name         Required    Callback */
    { "enabled",    TRUE,       dm_xml_handleHardcodedAttrEnabled   },
    { NULL,         FALSE,      NULL                                }
};

/** Attributes of the OADDR element. */
static const dm_xml_Attribute   dm_xml_attrOADDR[] =
{
    /* Name         Required    Callback */
    { "enabled",    TRUE,       dm_xml_handleOAddrAttrEnabled       },
    { NULL,         FALSE,      NULL                                }
};

/** Attributes of the ROW element. */
static const dm_xml_Attribute   dm_xml_attrROW[] =
{
    /* Name         Required    Callback */
    { "enabled",    TRUE,       dm_xml_handleRowAttrEnabled         },
    { NULL,         FALSE,      NULL                                }
};

/** Children of the ROW element. */
static dm_xml_Element           dm_xml_elemROW[] =
{
    /* Name             Required    Start                   End                     Attributes              Parent   Children */
    { "description",    FALSE,      NULL,                   NULL,                   NULL,                   NULL,    dm_xml_elemTail    },
    { "oaddr",          TRUE,       NULL,                   dm_xml_handleOAddrEnd,  dm_xml_attrOADDR,       NULL,    dm_xml_elemTail    },
    { "hardcoded",      TRUE,       NULL,                   NULL,                   dm_xml_attrHARDCODED,   NULL,    dm_xml_elemTail    },
    { "mask",           TRUE,       NULL,                   NULL,                   dm_xml_attrMASK,        NULL,    dm_xml_elemTail    },
    { "filter",         TRUE,       NULL,                   NULL,                   dm_xml_attrFILTER,      NULL,    dm_xml_elemTail    },
    { "zone",           TRUE,       NULL,                   NULL,                   dm_xml_attrZONE,        NULL,    dm_xml_elemTail    },
    { "subzone",        TRUE,       NULL,                   NULL,                   dm_xml_attrSUBZONE,     NULL,    dm_xml_elemTail    },
    { "action",         TRUE,       NULL,                   dm_xml_handleActionEnd, NULL,                   NULL,    dm_xml_elemTail    },
    { "param",          TRUE,       NULL,                   dm_xml_handleParamEnd,  NULL,                   NULL,    dm_xml_elemTail    },
    { NULL,             FALSE,      NULL,                   NULL,                   NULL,                   NULL,    dm_xml_elemTail    },
};

/** Attributes of the DM element. */
static const dm_xml_Attribute   dm_xml_attrDM[] =
{
    /* Name         Required    Callback */
    { "version",    TRUE,       dm_xml_handleDmAttrVersion          },
    { "level",      TRUE,       dm_xml_handleDmAttrLevel            },
    { "type",       TRUE,       dm_xml_handleDmAttrType             },
    { NULL,         FALSE,      NULL                                }
};

/** Children of the DM element. */
static dm_xml_Element           dm_xml_elemDM[] =
{
    /* Name             Required    Start                   End                     Attributes              Parent   Children */
    { "row",            FALSE,      dm_xml_handleRowStart,  dm_xml_handleRowEnd,    dm_xml_attrROW,         NULL,    dm_xml_elemROW     },
    { NULL,             FALSE,      NULL,                   NULL,                   NULL,                   NULL,    NULL               }
};

/** Standard decision matrix root element of the parse tree. */
static dm_xml_Element           dm_xml_elemRoot[] =
{
    /* Name             Required    Start                   End                     Attributes              Parent   Children */
    { "dm",             TRUE,       NULL,                   NULL,                   dm_xml_attrDM,          NULL,    dm_xml_elemDM,     },
    { NULL,             FALSE,      NULL,                   NULL,                   NULL,                   NULL,    NULL               }
};

/*******************************************************************************
    GLOBAL VARIABLES
*******************************************************************************/

/*******************************************************************************
    GLOBAL FUNCTIONS
*******************************************************************************/

/**
 * This function initializes this module.
 */
extern void dm_xml_init(void)
{
    /* Implement your code here ... */

    return;
}

#if VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM )

/**
 * This function loads the content of a standard decision matrix xml file to the
 * specified memory location.
 *
 * @param[in]   fileName    Standard decision matrix xml file name
 * @param[out]  dm          Decision matrix storage
 * @param[in]   rows        Max. number of decision matrix rows in the storage
 * @return Status
 */
extern DM_XML_RET dm_xml_loadStd(char const * const fileName, vscp_dm_MatrixRow * const dm, uint8_t rows)
{
    DM_XML_RET      ret         = DM_XML_RET_OK;
    char*           fileBuffer  = NULL;
    dm_xml_Context  parserContext;
    dm_xml_Context* con         = &parserContext;

    DM_XML_VALIDATE_PTR(fileName);
    DM_XML_VALIDATE_PTR(dm);

    /* Load standard decision matrix xml file */
    fileBuffer = dm_xml_loadFileToMem(fileName);

    if (NULL == fileBuffer)
    {
        return DM_XML_RET_EFILE;
    }

    /* Clear standard decision matrix */
    memset(dm, 0, sizeof(vscp_dm_MatrixRow) * rows);

    /* Initialize parser context */
    memset(con, 0, sizeof(dm_xml_Context));

    con->dmStorage = dm;
    con->maxRows   = rows;

    ret = dm_xml_parseStd(con, fileBuffer);

    /* If any error happened, clear the standard decision matrix. */
    if ((DM_XML_RET_OK != ret) ||
        (TRUE == con->isAborted))
    {
        /* Clear standard decision matrix */
        memset(dm, 0, sizeof(vscp_dm_MatrixRow) * rows);

        if ((DM_XML_RET_OK == ret) &&
            (TRUE == con->isAborted))
        {
            ret = DM_XML_RET_ERROR;
        }
    }
    else
    {
        LOG_INFO_UINT32("Number of configured standard decision matrix rows:", con->index + 1);
    }

    /* Free allocated ressources */
    free(fileBuffer);
    fileBuffer = NULL;

    return ret;
}

#endif  /* VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM ) */

#if VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM_EXTENSION )

/**
 * This function loads the content of a extended decision matrix xml file to the
 * specified memory locations.
 *
 * @param[in]   fileName    Extended decision matrix xml file name
 * @param[out]  dm          Decision matrix storage
 * @param[out]  ext         Decision matrix extension storage
 * @param[in]   rows        Max. number of decision matrix rows in the storage
 * @return Status
 */
extern DM_XML_RET dm_xml_loadExt(char const * const fileName, vscp_dm_MatrixRow * const dm, vscp_dm_ExtRow * const ext, uint8_t rows)
{
    DM_XML_RET      ret         = DM_XML_RET_OK;
    char*           fileBuffer  = NULL;
    dm_xml_Context  parserContext;

    DM_XML_VALIDATE_PTR(fileName);
    DM_XML_VALIDATE_PTR(dm);
    DM_XML_VALIDATE_PTR(ext);

    /* Load extended decision matrix xml file */
    fileBuffer = dm_xml_loadFileToMem(fileName);

    if (NULL == fileBuffer)
    {
        return DM_XML_RET_EFILE;
    }

    /* Clear extended decision matrix */
    memset(dm, 0, sizeof(vscp_dm_MatrixRow) * rows);
    memset(ext, 0, sizeof(vscp_dm_ExtRow) * rows);

    /* Initialize parser context */
    memset(&parserContext, 0, sizeof(parserContext));

    parserContext.dmStorage     = dm;
    parserContext.extStorage    = ext;
    parserContext.maxRows       = rows;

    ret = dm_xml_parseExt(&parserContext, fileBuffer);

    /* If any error happened, clear the standard decision matrix. */
    if ((DM_XML_RET_OK != ret) ||
        (TRUE == parserContext.isAborted))
    {
        /* Clear extended decision matrix */
        memset(dm, 0, sizeof(vscp_dm_MatrixRow) * rows);
        memset(ext, 0, sizeof(vscp_dm_ExtRow) * rows);

        if ((DM_XML_RET_OK == ret) &&
            (TRUE == parserContext.isAborted))
        {
            ret = DM_XML_RET_ERROR;
        }
    }

    /* Free allocated ressources */
    free(fileBuffer);
    fileBuffer = NULL;

    return ret;
}

#endif  /* VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM_EXTENSION ) */

/**
 * This function loads the content of a decision matrix next generation xml file to the
 * specified memory location.
 *
 * @param[in]   fileName    Eecision matrix next generation xml file name
 * @param[out]  storage     Decision matrix next generation storage
 * @param[in]   size        Decision matrix next generation storage size in bytes
 * @return Status
 */
extern DM_XML_RET dm_xml_loadNG(char const * const fileName, uint8_t* storage, uint8_t size)
{
    DM_XML_RET      ret         = DM_XML_RET_OK;
    char*           fileBuffer  = NULL;
    dm_xml_Context  parserContext;

    DM_XML_VALIDATE_PTR(fileName);
    DM_XML_VALIDATE_PTR(storage);

    /* Load decision matrix next generation xml file */
    fileBuffer = dm_xml_loadFileToMem(fileName);

    if (NULL == fileBuffer)
    {
        return DM_XML_RET_EFILE;
    }

    /* Initialize parser context */
    memset(&parserContext, 0, sizeof(parserContext));

    /* Free allocated ressources */
    free(fileBuffer);
    fileBuffer = NULL;

    return ret;
}

/*******************************************************************************
    LOCAL FUNCTIONS
*******************************************************************************/

/**
 * This function returns the file size.
 *
 * @param[in]   fd  File descriptor
 * @return File size in byte
 */
static long dm_xml_getFileSize(FILE* fd)
{
    long    fileSize    = 0;
    long    oldPos      = 0;
    BOOL    error       = FALSE;

    if (NULL == fd)
    {
        return 0;
    }

    /* Remember current position */
    oldPos = ftell(fd);

    /* Determine file size */
    if (0 != fseek(fd, 0L, SEEK_END))
    {
        error = TRUE;
    }
    else
    {
        fileSize = ftell(fd);

        if (0 != fseek(fd, 0L, SEEK_SET))
        {
            error = TRUE;
        }
        else
        {
            fileSize -= ftell(fd);
        }
    }

    if (TRUE == error)
    {
        fileSize = 0;
    }

    /* Jump back to old position */
    fseek(fd, oldPos, SEEK_SET);

    return fileSize;
}

/**
 * This function loads a file to memory.
 * The returned buffer will be '\0' terminated.
 * Don't forget to free the buffer later!
 *
 * @param[in]   fileName    Decision matrix xml file name
 * @return Pointer to buffer
 */
static char* dm_xml_loadFileToMem(char const * const fileName)
{
    FILE    *fd         = NULL;
    long    fileSize    = 0;
    char    *buffer     = NULL;

    /* If a decision matrix file exists, it will be loaded. */
    fd = fopen(fileName, "rb");

    /* No decision matrix file found? */
    if (NULL == fd)
    {
        return NULL;
    }

    /* Determine file size */
    fileSize = dm_xml_getFileSize(fd);

    /* Read the whole file to the memory and process it. */
    buffer = (char*)malloc(fileSize + 1);

    if (NULL != buffer)
    {
        /* Read whole file to memory */
        if (fileSize != fread(buffer, 1, fileSize, fd))
        {
            /* Error happened, clear buffer */
            free(buffer);
            buffer = NULL;
        }
        else
        {
            buffer[fileSize] = '\0';
        }
    }

    /* Close decision matrix file */
    fclose(fd);
    fd = NULL;

    return buffer;
}

/**
 * This function compares case-insensitive.
 *
 * @param[in] str1  String 1
 * @param[in] str2  String 2
 * @return Compare result
 * @retval <0   The first character that does not match has a lower value in str1 than in str2.
 * @retval 0    The content of both strings is equal.
 * @retval >0   The first character that does not match has a greater value in str1 than in str2.
 */
static int dm_xml_strcmpi(char const * str1, char const * str2)
{
    int result = 0;
    
    if ((NULL == str1) && (NULL != str2))
    {
        result = -1;
    }
    else if ((NULL != str1) && (NULL == str2))
    {
        result = 1;
    }
    else
    {
        while(('\0' != *str1) && ('\0' != *str2) && (0 == result))
        {
            const char character1 = tolower(*str1);
            const char character2 = tolower(*str2);
            
            if (character1 < character2)
            {
                result = -1;
            }
            else if (character1 > character2)
            {
                result = 1;
            }
            else
            {
                ++str1;
                ++str2;
            }
        }
    }
    
    return result;
}

#if VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM )

/**
 * This function returns a data value from the string.
 * The data can be coded as decimal value or hexa-decimal value.
 *
 * @param[in]   str Data string
 * @return Value
 */
static unsigned long dm_xml_getDataValue(char const * const str)
{
    unsigned long   value   = 0;
    char*           nstop   = NULL;

    if ((NULL != strchr(str, 'x' )) ||
        (NULL != strchr(str, 'X' )))
    {
        value = strtoul(str, &nstop, 16);
    }
    else
    {
        value = strtoul(str, &nstop, 10);
    }

    return value;
}

/**
 * This function searches for the given attribute name and returns it value.
 *
 * @param[in]   atts    Array of attributes
 * @param[in]   name    Attribute name
 * @return Attribute value
 */
static char const * const dm_xml_getAttribute(const XML_Char **atts, char const * const name)
{
    XML_Char const *    attrValue   = NULL;

    if (NULL != atts)
    {
        uint32_t    index   = 0;

        while(NULL != atts[index])
        {
            XML_Char const * const  attrName    = atts[index];

            attrValue = atts[index + 1];

            /* Internal error happened? */
            if ((NULL == attrName) ||
                (NULL == attrValue))
            {
                break;
            }
            /* Attribute found? */
            else if (0 == strcmp(attrName, name))
            {
                break;
            }

            index += 2;
        }
    }

    return attrValue;
}

/**
 * This function parse the standard decision matrix xml and writes it to the
 * defined storage.
 *
 * @param[in]   con Parser context
 * @param[in]   xml Decision matrix xml as terminated string.
 * @return Status
 * @retval FALSE    Failed
 * @retval TRUE     Successful
 */
static DM_XML_RET dm_xml_parseStd(dm_xml_Context* const con, char const * const xml)
{
    DM_XML_RET  ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(xml);

    /* Setup expat XML parser */
    con->xmlParser = XML_ParserCreate(NULL);

    XML_SetUserData(con->xmlParser, con);
    XML_SetElementHandler(con->xmlParser, dm_xml_startElement, dm_xml_endElement);
    XML_SetCharacterDataHandler(con->xmlParser, dm_xml_characterDataHandler);
    
    /* Use the standard decision matrix tree for parsing */
    con->children = dm_xml_elemRoot;
    
    if (XML_STATUS_ERROR == XML_Parse(con->xmlParser, xml, strlen(xml), 1))
    {
        /* Parser error */
        LOG_ERROR_INT32(XML_ErrorString(XML_GetErrorCode(con->xmlParser)), XML_GetCurrentLineNumber(con->xmlParser));
        ret = DM_XML_RET_ERROR;
    }

    XML_ParserFree(con->xmlParser);

    /* Free allocated ressources */
    if (NULL != con->buffer)
    {
        free(con->buffer);
        con->buffer = NULL;
    }

    return ret;
}

/**
 * This function is called by the xml parser for every element text.
 *
 * @param[in]   userData    User data
 * @param[in]   str         Element text
 * @param[in]   len         Element text length
 */
static void dm_xml_characterDataHandler(void *userData, const XML_Char *str, int len)
{
    dm_xml_Context* con = (dm_xml_Context*)userData;

    if ((NULL == userData) ||
        (NULL == str))
    {
        return;
    }

    if (NULL != con->buffer)
    {
        free(con->buffer);
        con->buffer = NULL;
    }

    con->buffer = (XML_Char*)malloc(len + 1);

    if (NULL != con->buffer)
    {
        strncpy(con->buffer, str, len);
        con->buffer[len] = '\0';
    }

    return;
}

#endif  /* VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM ) */

#if VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM_EXTENSION )

/**
 * This function parse the decision matrix extension xml and writes it to the
 * defined storage.
 *
 * @param[in]   con Parser context
 * @param[in]   xml Decision matrix extension xml as terminated string.
 * @return Status
 * @retval FALSE    Failed
 * @retval TRUE     Successful
 */
static DM_XML_RET dm_xml_parseExt(dm_xml_Context* const con, char const * const xml)
{
    DM_XML_RET  ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(xml);

    /* Setup expat XML parser */
    con->xmlParser = XML_ParserCreate(NULL);

    XML_SetUserData(con->xmlParser, con);
    XML_SetElementHandler(con->xmlParser, dm_xml_startElementExt, dm_xml_endElementExt);
    XML_SetCharacterDataHandler(con->xmlParser, dm_xml_characterDataHandler);

    if (XML_STATUS_ERROR == XML_Parse(con->xmlParser, xml, strlen(xml), 1))
    {
        /* Parser error */
        LOG_ERROR_INT32(XML_ErrorString(XML_GetErrorCode(con->xmlParser)), XML_GetCurrentLineNumber(con->xmlParser));
        ret = DM_XML_RET_ERROR;
    }

    XML_ParserFree(con->xmlParser);

    /* Free allocated ressources */
    if (NULL != con->buffer)
    {
        free(con->buffer);
        con->buffer = NULL;
    }

    return ret;
}

/**
 * This function is called by the xml parser for every found element start (xml tag).
 *
 * @param[in]   userData    User data
 * @param[in]   name        Element name
 * @param[in]   atts        Array of attributes
 */
static void XMLCALL dm_xml_startElementExt(void *userData, const XML_Char *name, const XML_Char **atts)
{
    dm_xml_Context* con     = (dm_xml_Context*)userData;
    BOOL            abort   = FALSE;

    if ((NULL == userData) ||
        (NULL == name))
    {
        return;
    }

    if (0 == strcmp(name, "dm"))
    {
        XML_Char const *    attrValue       = NULL;
        BOOL                versionCorrect  = FALSE;
        BOOL                levelCorrect    = FALSE;
        BOOL                typeCorrect     = FALSE;

        con->dmFound = TRUE;

        /* Get the standard decision matrix file format version */
        attrValue = dm_xml_getAttribute(atts, "version");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute version missing at line", XML_GetCurrentLineNumber(con->xmlParser));
        }
        else
        {
            /* Check the version */
            if (0 != strcmp(attrValue, DM_EXT_XML_VERSION))
            {
                /* Wrong xml version */
                LOG_ERROR("Wrong extended decision matrix file format version.");
            }
            else
            {
                versionCorrect = TRUE;
            }
        }

        /* Get the decision matrix level */
        attrValue = dm_xml_getAttribute(atts, "level");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute level missing at line", XML_GetCurrentLineNumber(con->xmlParser));
        }
        else
        {
            /* Check the level */
            if (0 != strcmp(attrValue, DM_LEVEL))
            {
                /* Wrong decision matrix level */
                LOG_ERROR("Wrong decision matrix level.");
            }
            else
            {
                levelCorrect = TRUE;
            }
        }

        /* Get the decision matrix type */
        attrValue = dm_xml_getAttribute(atts, "type");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute type missing at line", XML_GetCurrentLineNumber(con->xmlParser));
        }
        else
        {
            /* Check the type */
            if (0 != strcmp(attrValue, "ext"))
            {
                /* Wrong decision matrix type */
                LOG_ERROR("Wrong decision matrix type.");
            }
            else
            {
                typeCorrect = TRUE;
            }
        }

        /* All attributes are required */
        if ((FALSE == versionCorrect) ||
            (FALSE == levelCorrect) ||
            (FALSE == typeCorrect))
        {
            abort = TRUE;
        }
    }
    else if (FALSE == con->dmFound)
    {
        LOG_ERROR_INT32("Invalid extended decision matrix XML at line", XML_GetCurrentLineNumber(con->xmlParser));
        abort = TRUE;
    }
    else if (0 == strcmp(name, "row"))
    {
        XML_Char const *    attrValue   = NULL;

        con->rowFound = TRUE;

        /* Clear dm row */
        memset(&con->dmStorage[con->index], 0, sizeof(vscp_dm_MatrixRow));
        memset(&con->extStorage[con->index], 0, sizeof(vscp_dm_ExtRow));

        /* Is the decision matrix row enabled? */
        attrValue = dm_xml_getAttribute(atts, "enabled");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute enabled missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            if (0 == strcmp(attrValue, "true"))
            {
                con->dmStorage[con->index].flags |= VSCP_DM_FLAG_ENABLE;
            }
        }

        /* Standard of extended? */
        attrValue = dm_xml_getAttribute(atts, "type");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute type missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            if (0 == strcmp(attrValue, "std"))
            {
                /* Nothing to do */
            }
            else if (0 == strcmp(attrValue, "ext"))
            {
                /* Extend the standard decision matrix row */
                con->dmStorage[con->index].action = VSCP_DM_ACTION_EXTENDED_DM;
            }
            else
            {
                LOG_ERROR_INT32("Attribute type is invalid line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (FALSE == con->rowFound)
    {
        LOG_ERROR_INT32("Invalid extended decision matrix XML at line", XML_GetCurrentLineNumber(con->xmlParser));
        abort = TRUE;
    }
    else if (0 == strcmp(name, "mask"))
    {
        XML_Char const *    attrValue   = NULL;

        /* Get class mask */
        attrValue = dm_xml_getAttribute(atts, "class");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute class missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            unsigned long value = dm_xml_getDataValue(attrValue);

            if (0x1ff >= value)
            {
                con->dmStorage[con->index].classMask = (uint8_t)(value & 0xff);

                if (0 != (value & 0x100))
                {
                    con->dmStorage[con->index].flags |= VSCP_DM_FLAG_CLASS_MASK_BIT8;
                }
            }
            else
            {
                LOG_ERROR_INT32("Attribute class out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }

        /* Get type mask */
        attrValue = dm_xml_getAttribute(atts, "type");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute type missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            unsigned long value = dm_xml_getDataValue(attrValue);

            if (0xff >= value)
            {
                con->dmStorage[con->index].typeMask = value;
            }
            else
            {
                LOG_ERROR_INT32("Attribute type out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (0 == strcmp(name, "filter"))
    {
        XML_Char const *    attrValue   = NULL;

        /* Get class filter */
        attrValue = dm_xml_getAttribute(atts, "class");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute class missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            unsigned long value = dm_xml_getDataValue(attrValue);

            if (0x1ff >= value)
            {
                con->dmStorage[con->index].classFilter = (uint8_t)(value & 0xff);

                if (0 != (value & 0x100))
                {
                    con->dmStorage[con->index].flags |= VSCP_DM_FLAG_CLASS_FILTER_BIT8;
                }
            }
            else
            {
                LOG_ERROR_INT32("Attribute class out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }

        /* Get type filter */
        attrValue = dm_xml_getAttribute(atts, "type");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute type missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            unsigned long value = dm_xml_getDataValue(attrValue);

            if (0xff >= value)
            {
                con->dmStorage[con->index].typeFilter = value;
            }
            else
            {
                LOG_ERROR_INT32("Attribute type out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (0 == strcmp(name, "zone"))
    {
        XML_Char const *    attrValue   = NULL;

        /* Get zone enabled flag */
        attrValue = dm_xml_getAttribute(atts, "enabled");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute enabled missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            if (0 == strcmp(attrValue, "true"))
            {
                con->dmStorage[con->index].flags |= VSCP_DM_FLAG_MATCH_ZONE;
            }
            else if (0 == strcmp(attrValue, "false"))
            {
                con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_MATCH_ZONE;
            }
            else
            {
                LOG_ERROR_INT32("Invalid value at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (0 == strcmp(name, "subzone"))
    {
        XML_Char const *    attrValue   = NULL;

        /* Get sub-zone enabled flag */
        attrValue = dm_xml_getAttribute(atts, "enabled");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute enabled missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            if (0 == strcmp(attrValue, "true"))
            {
                con->dmStorage[con->index].flags |= VSCP_DM_FLAG_MATCH_SUB_ZONE;
            }
            else if (0 == strcmp(attrValue, "false"))
            {
                con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_MATCH_SUB_ZONE;
            }
            else
            {
                LOG_ERROR_INT32("Invalid value at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (0 == strcmp(name, "action"))
    {
        /* Nothing to do */
    }
    else if (0 == strcmp(name, "param"))
    {
        /* Nothing to do */
    }
    else if (0 == strcmp(name, "oaddr"))
    {
        XML_Char const *    attrValue   = NULL;

        /* Get originating address enabled flag */
        attrValue = dm_xml_getAttribute(atts, "enabled");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute enabled missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            if (0 == strcmp(attrValue, "true"))
            {
                con->dmStorage[con->index].flags |= VSCP_DM_FLAG_CHECK_OADDR;
            }
            else if (0 == strcmp(attrValue, "false"))
            {
                con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_CHECK_OADDR;
            }
            else
            {
                LOG_ERROR_INT32("Invalid value at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (0 == strcmp(name, "hardcoded"))
    {
        XML_Char const *    attrValue   = NULL;

        /* Get hardcoded node enabled flag */
        attrValue = dm_xml_getAttribute(atts, "enabled");

        if (NULL == attrValue)
        {
            LOG_ERROR_INT32("Attribute enabled missing at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            if (0 == strcmp(attrValue, "true"))
            {
                con->dmStorage[con->index].flags |= VSCP_DM_FLAG_HARDCODED;
            }
            else if (0 == strcmp(attrValue, "false"))
            {
                con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_HARDCODED;
            }
            else
            {
                LOG_ERROR_INT32("Invalid value at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (0 == strcmp(name, "data"))
    {
        con->dataFound = TRUE;
    }
    else if (0 == strcmp(name, "byte"))
    {
        if (FALSE == con->dataFound)
        {
            LOG_ERROR_INT32("Invalid extended decision matrix XML at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            XML_Char const *    attrValue   = NULL;

            /* Get id */
            attrValue = dm_xml_getAttribute(atts, "id");

            if (NULL == attrValue)
            {
                LOG_ERROR_INT32("Attribute id missing at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
            else
            {
                unsigned long id  = dm_xml_getDataValue(attrValue);

                if ((0 == id) ||
                    ((3 <= id) && (5 >= id)))
                {
                    con->id = id;
                }
                else
                {
                    LOG_ERROR_INT32("Attribute id out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
                    abort = TRUE;
                }
            }

            /* Get enabled flag */
            attrValue = dm_xml_getAttribute(atts, "enabled");

            if (NULL == attrValue)
            {
                LOG_ERROR_INT32("Attribute enabled missing at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
            else
            {
                if (0 == strcmp(attrValue, "true"))
                {
                    if (0 == con->id)
                    {
                        con->dmStorage[con->index].actionPar |= VSCP_DM_EXTFLAG_MATCH_PAR_0;
                    }
                    else if (3 == con->id)
                    {
                        con->dmStorage[con->index].actionPar |= VSCP_DM_EXTFLAG_MATCH_PAR_3;
                    }
                    else if (4 == con->id)
                    {
                        con->dmStorage[con->index].actionPar |= VSCP_DM_EXTFLAG_MATCH_PAR_4;
                    }
                    else if (5 == con->id)
                    {
                        con->dmStorage[con->index].actionPar |= VSCP_DM_EXTFLAG_MATCH_PAR_5;
                    }
                }
                else if (0 == strcmp(attrValue, "false"))
                {
                    if (0 == con->id)
                    {
                        con->dmStorage[con->index].actionPar &= ~VSCP_DM_EXTFLAG_MATCH_PAR_0;
                    }
                    else if (3 == con->id)
                    {
                        con->dmStorage[con->index].actionPar &= ~VSCP_DM_EXTFLAG_MATCH_PAR_3;
                    }
                    else if (4 == con->id)
                    {
                        con->dmStorage[con->index].actionPar &= ~VSCP_DM_EXTFLAG_MATCH_PAR_4;
                    }
                    else if (5 == con->id)
                    {
                        con->dmStorage[con->index].actionPar &= ~VSCP_DM_EXTFLAG_MATCH_PAR_5;
                    }
                }
                else
                {
                    LOG_ERROR_INT32("Invalid value at line", XML_GetCurrentLineNumber(con->xmlParser));
                    abort = TRUE;
                }
            }
        }
    }

    /* Abort parsing? */
    if (TRUE == abort)
    {
        con->isAborted = TRUE;

        XML_SetElementHandler(con->xmlParser, NULL, NULL);
        XML_SetCharacterDataHandler(con->xmlParser, NULL);
    }

    return;
}

/**
 * This function is called by the xml parser for every found element end (xml tag).
 *
 * @param[in]   userData    User data
 * @param[in]   name        Element name
 */
static void XMLCALL dm_xml_endElementExt(void *userData, const XML_Char *name)
{
    dm_xml_Context* con     = (dm_xml_Context*)userData;
    BOOL            abort   = FALSE;

    if ((NULL == userData) ||
        (NULL == name))
    {
        return;
    }

    if (0 == strcmp(name, "dm"))
    {
        con->dmFound = FALSE;
    }
    else if (FALSE == con->dmFound)
    {
        LOG_ERROR_INT32("Invalid extended decision matrix XML at line", XML_GetCurrentLineNumber(con->xmlParser));
        abort = TRUE;
    }
    else if (0 == strcmp(name, "row"))
    {
        /* Avoid decision matrix storage overflow by writing more rows than reserved for the decision matrix. */
        if (con->maxRows > con->index)
        {
            con->rowFound = FALSE;
            ++con->index;
        }
        else
        {
            LOG_ERROR("DM XML file contains more rows, than max. possible.");
            abort = TRUE;
        }
    }
    else if (FALSE == con->rowFound)
    {
        LOG_ERROR_INT32("Invalid extended decision matrix XML at line", XML_GetCurrentLineNumber(con->xmlParser));
        abort = TRUE;
    }
    else if (0 == strcmp(name, "mask"))
    {
        /* Nothing to do */
    }
    else if (0 == strcmp(name, "filter"))
    {
        /* Nothing to do */
    }
    else if (0 == strcmp(name, "zone"))
    {
        if (VSCP_DM_ACTION_EXTENDED_DM == con->dmStorage[con->index].action)
        {
            if (NULL != con->buffer)
            {
                unsigned long value = dm_xml_getDataValue(con->buffer);

                if (0xff >= value)
                {
                    con->extStorage[con->index].zone = value;
                }
                else
                {
                    LOG_ERROR_INT32("Invalid zone value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
                    abort = TRUE;
                }
            }
            else
            {
                LOG_ERROR_INT32("Missing zone value. Line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (0 == strcmp(name, "subzone"))
    {
        if (VSCP_DM_ACTION_EXTENDED_DM == con->dmStorage[con->index].action)
        {
            if (NULL != con->buffer)
            {
                unsigned long value = dm_xml_getDataValue(con->buffer);

                if (0xff >= value)
                {
                    con->extStorage[con->index].subZone = value;
                }
                else
                {
                    LOG_ERROR_INT32("Invalid sub-zone value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
                    abort = TRUE;
                }
            }
            else
            {
                LOG_ERROR_INT32("Missing sub-zone value. Line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }
    else if (0 == strcmp(name, "action"))
    {
        if (NULL != con->buffer)
        {
            unsigned long value = dm_xml_getDataValue(con->buffer);

            if (0xff >= value)
            {
                /* Standard or extended? */
                if (VSCP_DM_ACTION_EXTENDED_DM != con->dmStorage[con->index].action)
                {
                    con->dmStorage[con->index].action = value;
                }
                else
                {
                    con->extStorage[con->index].action = value;
                }
            }
            else
            {
                LOG_ERROR_INT32("Invalid action value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
        else
        {
            LOG_ERROR_INT32("Missing action value. Line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
    }
    else if (0 == strcmp(name, "param"))
    {
        if (NULL != con->buffer)
        {
            unsigned long value = dm_xml_getDataValue(con->buffer);

            if (0xff >= value)
            {
                /* Standard or extended? */
                if (VSCP_DM_ACTION_EXTENDED_DM != con->dmStorage[con->index].action)
                {
                    con->dmStorage[con->index].actionPar = value;
                }
                else
                {
                    con->extStorage[con->index].actionPar = value;
                }
            }
            else
            {
                LOG_ERROR_INT32("Invalid action parameter value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
        else
        {
            LOG_ERROR_INT32("Missing param value. Line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
    }
    else if (0 == strcmp(name, "oaddr"))
    {
        if (NULL != con->buffer)
        {
            unsigned long value = dm_xml_getDataValue(con->buffer);

            if (0xff >= value)
            {
                con->dmStorage[con->index].oaddr = value;
            }
            else
            {
                LOG_ERROR_INT32("Invalid oaddr value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
        else
        {
            LOG_ERROR_INT32("Missing oaddr value. Line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
    }
    else if (0 == strcmp(name, "hardcoded"))
    {
        /* Nothing to do */
    }
    else if (0 == strcmp(name, "data"))
    {
        con->dataFound = FALSE;
    }
    else if (0 == strcmp(name, "byte"))
    {
        if (FALSE == con->dataFound)
        {
            LOG_ERROR_INT32("Invalid extended decision matrix XML at line", XML_GetCurrentLineNumber(con->xmlParser));
            abort = TRUE;
        }
        else
        {
            if (NULL != con->buffer)
            {
                unsigned long value = dm_xml_getDataValue(con->buffer);

                if (0xff >= value)
                {
                    if (0 == con->id)
                    {
                        con->extStorage[con->index].par0 = value;
                    }
                    else if (3 == con->id)
                    {
                        con->extStorage[con->index].par3 = value;
                    }
                    else if (4 == con->id)
                    {
                        con->extStorage[con->index].par4 = value;
                    }
                    else if (5 == con->id)
                    {
                        con->extStorage[con->index].par5 = value;
                    }
                }
                else
                {
                    LOG_ERROR_INT32("Invalid byte value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
                    abort = TRUE;
                }
            }
            else
            {
                LOG_ERROR_INT32("Missing byte value. Line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
    }

    /* Abort parsing? */
    if (TRUE == abort)
    {
        con->isAborted = TRUE;

        XML_SetElementHandler(con->xmlParser, NULL, NULL);
        XML_SetCharacterDataHandler(con->xmlParser, NULL);
    }

    return;
}

#endif  /* VSCP_CONFIG_BASE_IS_ENABLED( VSCP_CONFIG_ENABLE_DM_EXTENSION ) */

/**
 * This function is called by the xml parser for every found element start (xml tag).
 *
 * @param[in]   userData    User data
 * @param[in]   name        Element name
 * @param[in]   atts        Array of attributes
 */
static void XMLCALL dm_xml_startElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
    dm_xml_Context* con     = (dm_xml_Context*)userData;
    BOOL            abort   = FALSE;
    dm_xml_Element* elem    = NULL;

    if ((NULL == userData) ||
        (NULL == name))
    {
        return;
    }
       
    /* Find this element in the childrens. */
    elem = con->children;
    while((NULL != elem) && (NULL != elem->name))
    {
        /* Element found? */
        if (0 == dm_xml_strcmpi(elem->name, name))
        {
            break;
        }
    
        ++elem;
    }
    
    /* Bottom of parse tree reached? */
    if (NULL == elem)
    {
        LOG_ERROR_INT32("Bottom of parse tree reached at line", XML_GetCurrentLineNumber(con->xmlParser));
        abort = TRUE;
    }
    /* Element not found? */
    else if (NULL == elem->name)
    {
        LOG_DEBUG_STR("Unknown element:", name);
        LOG_ERROR_INT32("Unknown element at line", XML_GetCurrentLineNumber(con->xmlParser));
        abort = TRUE;
    }
    /* Element is known */
    else
    {
#if (1 == DM_XML_ENABLE_DEBUG_INFO)
        LOG_DEBUG_STR("Element start:", elem->name);
#endif  /* (1 == DM_XML_ENABLE_DEBUG_INFO) */
        
        /* Handle all attributes of the known element */
        if (NULL != elem->attributes)
        {
            dm_xml_Attribute const *    attr    = elem->attributes;
            
            while(NULL != attr->name)
            {
                char const *    attrValue = dm_xml_getAttribute(atts, attr->name);
                
                /* Attribute not found? */
                if (NULL == attrValue)
                {
                    /* Is the attribute required? */
                    if (TRUE == attr->required)
                    {
                        LOG_DEBUG_STR("Attribute missing:", attr->name);
                        LOG_ERROR_INT32("Attribute missing at line", XML_GetCurrentLineNumber(con->xmlParser));
                        abort = TRUE;
                    }
                }
                /* Attribute found */
                else
                {
#if (1 == DM_XML_ENABLE_DEBUG_INFO)
                    LOG_DEBUG_STR("Attribute:", attr->name);
#endif  /* (1 == DM_XML_ENABLE_DEBUG_INFO) */

                    /* Notify application */
                    if (NULL != attr->callBack)
                    {
                        if (DM_XML_RET_OK != attr->callBack(con, attr->name, attrValue))
                        {
                            LOG_ERROR_INT32("Internal error happened at line", XML_GetCurrentLineNumber(con->xmlParser));
                            abort = TRUE;
                        }
                    }
                }
            
                /* Next attribute in the list */
                ++attr;
            }
        }
        
        /* Get element information */
        if (NULL != elem->startCallback)
        {
            if (DM_XML_RET_OK != elem->startCallback(con))
            {
                LOG_ERROR_INT32("Internal error happened at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
        
        /* Next time handle the children of this element.
         * Remember the current element (parent) in the first child!
         */
        elem->children->parent = con->children;
        con->children = elem->children;
        
        /* One step down in the tree */
        ++con->depth;
    }
    
    /* Abort parsing? */
    if (TRUE == abort)
    {
        con->isAborted = TRUE;

        XML_SetElementHandler(con->xmlParser, NULL, NULL);
        XML_SetCharacterDataHandler(con->xmlParser, NULL);
    }
    
    return;
}

/**
 * This function is called by the xml parser for every found element end (xml tag).
 *
 * @param[in]   userData    User data
 * @param[in]   name        Element name
 */
static void XMLCALL dm_xml_endElement(void *userData, const XML_Char *name)
{
    dm_xml_Context* con     = (dm_xml_Context*)userData;
    BOOL            abort   = FALSE;
    dm_xml_Element* elem    = NULL;

    if ((NULL == userData) ||
        (NULL == name))
    {
        return;
    }
    
    /* Go back to the parent element and clear the parent pointer */
    elem = con->children;
    con->children = con->children->parent;
    elem->parent = NULL;

    /* Find this element in the children. */
    elem = con->children;
    while((NULL != elem) && (NULL != elem->name))
    {
        /* Element found? */
        if (0 == dm_xml_strcmpi(elem->name, name))
        {
            break;
        }
    
        ++elem;
    }
    
    /* Bottom of parse tree reached? */
    if (NULL == elem)
    {
        LOG_ERROR_INT32("Bottom of parse tree reached at line", XML_GetCurrentLineNumber(con->xmlParser));
        abort = TRUE;
    }
    /* Element not found? */
    else if (NULL == elem->name)
    {
        LOG_DEBUG_STR("Unknown element:", name);
        LOG_ERROR_INT32("Unknown element at line", XML_GetCurrentLineNumber(con->xmlParser));
        abort = TRUE;
    }
    /* Element is known */
    else
    {
#if (1 == DM_XML_ENABLE_DEBUG_INFO)
        LOG_DEBUG_STR("Element end:", name);
#endif  /* (1 == DM_XML_ENABLE_DEBUG_INFO) */

        /* Get element information */
        if (NULL != elem->endCallback)
        {
            if (DM_XML_RET_OK != elem->endCallback(con))
            {
                LOG_ERROR_INT32("Internal error happened at line", XML_GetCurrentLineNumber(con->xmlParser));
                abort = TRUE;
            }
        }
        
        /* One step up in the tree */
        if (0 > con->depth)
        {
            --con->depth;
        }
    }
    
    /* Abort parsing? */
    if (TRUE == abort)
    {
        con->isAborted = TRUE;

        XML_SetElementHandler(con->xmlParser, NULL, NULL);
        XML_SetCharacterDataHandler(con->xmlParser, NULL);
    }
    
    return;
}

/**
 * This function handles the "version" attribute of the DM element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleDmAttrVersion(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);
    
    /* Check the version */
    if (0 != dm_xml_strcmpi(value, DM_STD_XML_VERSION))
    {
        /* Wrong xml version */
        LOG_ERROR("Wrong standard decision matrix file format version.");
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "level" attribute of the DM element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleDmAttrLevel(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);
    
    /* Check the level */
    if (0 != dm_xml_strcmpi(value, DM_LEVEL))
    {
        /* Wrong decision matrix level */
        LOG_ERROR("Wrong decision matrix level.");
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "type" attribute of the DM element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleDmAttrType(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);
    
    /* Check the type */
    if (0 != dm_xml_strcmpi(value, "std"))
    {
        /* Wrong decision matrix type */
        LOG_ERROR("Wrong decision matrix type.");
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "enabled" attribute of the ROW element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleRowAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);

    if (0 == dm_xml_strcmpi(value, "true"))
    {
        con->dmStorage[con->index].flags |= VSCP_DM_FLAG_ENABLE;
    }
    else if (0 == dm_xml_strcmpi(value, "false"))
    {
        con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_ENABLE;
    }
    else
    {
        LOG_ERROR_INT32("Attribute value invalid at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handle the start of the ROW element.
 *
 * @param[in]   con Parser context
 * @return Status
 */
static DM_XML_RET dm_xml_handleRowStart(dm_xml_Context* const con)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    
    /* Clear decision matrix row */
    memset(&con->dmStorage[con->index], 0, sizeof(vscp_dm_MatrixRow));

    return ret;
}

/**
 * This function handle the end of the ROW element.
 *
 * @param[in]   con Parser context
 * @return Status
 */
static DM_XML_RET dm_xml_handleRowEnd(dm_xml_Context* const con)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    
    /* Avoid decision matrix storage overflow by writing more rows than reserved for the decision matrix. */
    if (con->maxRows > con->index)
    {
        ++con->index;
    }
    else
    {
        LOG_ERROR("DM XML file contains more rows, than max. possible.");
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "enabled" attribute of the OADDR element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleOAddrAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);

    if (0 == dm_xml_strcmpi(value, "true"))
    {
        con->dmStorage[con->index].flags |= VSCP_DM_FLAG_CHECK_OADDR;
    }
    else if (0 == dm_xml_strcmpi(value, "false"))
    {
        con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_CHECK_OADDR;
    }
    else
    {
        LOG_ERROR_INT32("Attribute value invalid at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "enabled" attribute of the HARDCODED element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleHardcodedAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);

    if (0 == dm_xml_strcmpi(value, "true"))
    {
        con->dmStorage[con->index].flags |= VSCP_DM_FLAG_HARDCODED;
    }
    else if (0 == dm_xml_strcmpi(value, "false"))
    {
        con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_HARDCODED;
    }
    else
    {
        LOG_ERROR_INT32("Attribute value invalid at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "class" attribute of the MASK element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleMaskAttrClass(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET      ret     = DM_XML_RET_OK;
    unsigned long   valueUL = 0;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);

    valueUL = dm_xml_getDataValue(value);

    if (0x1ff >= valueUL)
    {
        con->dmStorage[con->index].classMask = (uint8_t)(valueUL & 0xff);

        if (0 != (valueUL & 0x100))
        {
            con->dmStorage[con->index].flags |= VSCP_DM_FLAG_CLASS_MASK_BIT8;
        }
    }
    else
    {
        LOG_ERROR_INT32("Attribute class out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "type" attribute of the MASK element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleMaskAttrType(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET      ret     = DM_XML_RET_OK;
    unsigned long   valueUL = 0;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);
    
    valueUL = dm_xml_getDataValue(value);

    if (0xff >= valueUL)
    {
        con->dmStorage[con->index].typeMask = valueUL;
    }
    else
    {
        LOG_ERROR_INT32("Attribute type out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "class" attribute of the FILTER element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleFilterAttrClass(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET      ret     = DM_XML_RET_OK;
    unsigned long   valueUL = 0;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);

    valueUL = dm_xml_getDataValue(value);

    if (0x1ff >= valueUL)
    {
        con->dmStorage[con->index].classMask = (uint8_t)(valueUL & 0xff);

        if (0 != (valueUL & 0x100))
        {
            con->dmStorage[con->index].flags |= VSCP_DM_FLAG_CLASS_FILTER_BIT8;
        }
    }
    else
    {
        LOG_ERROR_INT32("Attribute class out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "type" attribute of the FILTER element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleFilterAttrType(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET      ret     = DM_XML_RET_OK;
    unsigned long   valueUL = 0;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);
    
    valueUL = dm_xml_getDataValue(value);

    if (0xff >= valueUL)
    {
        con->dmStorage[con->index].typeFilter = valueUL;
    }
    else
    {
        LOG_ERROR_INT32("Attribute type out of range at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "enabled" attribute of the ZONE element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleZoneAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);

    if (0 == dm_xml_strcmpi(value, "true"))
    {
        con->dmStorage[con->index].flags |= VSCP_DM_FLAG_MATCH_ZONE;
    }
    else if (0 == dm_xml_strcmpi(value, "false"))
    {
        con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_MATCH_ZONE;
    }
    else
    {
        LOG_ERROR_INT32("Attribute value invalid at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handles the "enabled" attribute of the SUBZONE element.
 *
 * @param[in]   con     Parser context
 * @param[in]   name    Attribute name
 * @param[in]   value   Attribute value
 * @return Status
 */
static DM_XML_RET dm_xml_handleSubZoneAttrEnabled(dm_xml_Context* const con, char const * const name, char const * const value)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    DM_XML_VALIDATE_PTR(name);
    DM_XML_VALIDATE_PTR(value);

    if (0 == dm_xml_strcmpi(value, "true"))
    {
        con->dmStorage[con->index].flags |= VSCP_DM_FLAG_MATCH_SUB_ZONE;
    }
    else if (0 == dm_xml_strcmpi(value, "false"))
    {
        con->dmStorage[con->index].flags &= ~VSCP_DM_FLAG_MATCH_SUB_ZONE;
    }
    else
    {
        LOG_ERROR_INT32("Attribute value invalid at line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handle the end of the OADDR element.
 *
 * @param[in]   con Parser context
 * @return Status
 */
static DM_XML_RET dm_xml_handleOAddrEnd(dm_xml_Context* const con)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    
    if (NULL != con->buffer)
    {
        unsigned long valueUL = dm_xml_getDataValue(con->buffer);

        if (0xff >= valueUL)
        {
            con->dmStorage[con->index].oaddr = valueUL;
        }
        else
        {
            LOG_ERROR_INT32("Invalid oaddr value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
            
            ret = DM_XML_RET_ERROR;
        }
    }
    else
    {
        LOG_ERROR_INT32("Missing oaddr value. Line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handle the end of the ACTION element.
 *
 * @param[in]   con Parser context
 * @return Status
 */
static DM_XML_RET dm_xml_handleActionEnd(dm_xml_Context* const con)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    
    if (NULL != con->buffer)
    {
        unsigned long valueUL = dm_xml_getDataValue(con->buffer);

        if (0xff >= valueUL)
        {
            con->dmStorage[con->index].action = valueUL;
        }
        else
        {
            LOG_ERROR_INT32("Invalid oaddr value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
            
            ret = DM_XML_RET_ERROR;
        }
    }
    else
    {
        LOG_ERROR_INT32("Missing oaddr value. Line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}

/**
 * This function handle the end of the PARAM element.
 *
 * @param[in]   con Parser context
 * @return Status
 */
static DM_XML_RET dm_xml_handleParamEnd(dm_xml_Context* const con)
{
    DM_XML_RET ret = DM_XML_RET_OK;

    DM_XML_VALIDATE_PTR(con);
    
    if (NULL != con->buffer)
    {
        unsigned long valueUL = dm_xml_getDataValue(con->buffer);

        if (0xff >= valueUL)
        {
            con->dmStorage[con->index].actionPar = valueUL;
        }
        else
        {
            LOG_ERROR_INT32("Invalid oaddr value. Can only be in the range 0 - 255. Line", XML_GetCurrentLineNumber(con->xmlParser));
            
            ret = DM_XML_RET_ERROR;
        }
    }
    else
    {
        LOG_ERROR_INT32("Missing oaddr value. Line", XML_GetCurrentLineNumber(con->xmlParser));
        
        ret = DM_XML_RET_ERROR;
    }
    
    return ret;
}