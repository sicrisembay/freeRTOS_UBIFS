//*****************************************************************************
//!
//! \file zplCompat.c
//!
//! \brief Insert brief description.
//!
//! $Author: sicrisre001 $
//! $Date: 2016-06-08 19:06:50 +0800 (Wed, 08 Jun 2016) $
//! $Revision: 677 $
//!
//*****************************************************************************

//*****************************************************************************
// File dependencies.
//*****************************************************************************
#include "zplCompat.h"

#if !defined(__TEST_APP__)

#define COMPAT_VAR_NAME_MAX_LEN                 (32)
#define COMPAT_VAR_VAL_MAX_LEN                  (128)

typedef struct {
    char strName[COMPAT_VAR_NAME_MAX_LEN];
    char strValue[COMPAT_VAR_VAL_MAX_LEN];
} envEntry_t;

static envEntry_t compatEnvEntry[] = {
        { "mtddevname", ""},
        { "mtddevnum", ""},
        { "partition", PARTITION_DEFAULT},
        { "mtdparts", MTDPARTS_DEFAULT},
        { "mtdids", MTDIDS_DEFAULT},
        { "filesize", ""},
        { "fileaddr", ""}
};

#define nEnvEntry       (sizeof(compatEnvEntry) / sizeof(compatEnvEntry[0]))

int setenv(const char * varname, const char *varvalue)
{
    uint32_t idx;
    if((varname == NULL) || (varvalue == NULL)) {
        return 1;
    }

    for(idx = 0; idx < nEnvEntry; idx++) {
        if(strcmp(compatEnvEntry[idx].strName, varname) == 0) {
            break;
        }
    }

    if(idx >= nEnvEntry) {
        /* not found */
        return 1;
    }

    /* set value */
    strcpy(compatEnvEntry[idx].strValue, varvalue);

    return 0;
}

char * getenv(const char *varname)
{
    uint32_t idx;
    if(varname == NULL) {
        return NULL;
    }

    for(idx = 0; idx < nEnvEntry; idx++) {
        if(strcmp(compatEnvEntry[idx].strName, varname) == 0) {
            break;
        }
    }

    if(idx >= nEnvEntry) {
        /* not found */
        return NULL;
    }

    return(compatEnvEntry[idx].strValue);
}
int setenv_ulong(const char *varname, uint32_t value)
{
    char tempStr[COMPAT_VAR_VAL_MAX_LEN];
    snprintf(tempStr, COMPAT_VAR_VAL_MAX_LEN - 1, "%d", value);
    /* make sure it's null terminated */
    tempStr[COMPAT_VAR_NAME_MAX_LEN - 1] = '\0';
    return (setenv(varname, tempStr));
}

int setenv_hex(const char *varname, ulong value)
{
    char tempStr[COMPAT_VAR_VAL_MAX_LEN];
    snprintf(tempStr, COMPAT_VAR_VAL_MAX_LEN - 1, "%lx", value);
    /* make sure it's null terminated */
    tempStr[COMPAT_VAR_NAME_MAX_LEN - 1] = '\0';
    return (setenv(varname, tempStr));
}

#endif
