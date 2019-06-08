/*===========================================================================*/
/**
    @file    convlb.c

    @brief   Convert line breaks to Unix format

@verbatim
=============================================================================

    Copyright 2018-2019 NXP

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================
@endverbatim */

/*===========================================================================
                                INCLUDE FILES
=============================================================================*/
#include <stdlib.h>
#include <stdio.h>

/*===========================================================================
                                GLOBALS
=============================================================================*/

#define NULL ((void *)0)

#define TEMPFILENAME ".convlb.tmp"

char *toolname = NULL;

/*===========================================================================
                                LOCAL FUNCTIONS
=============================================================================*/
static void print_usage(void)
{
    printf("Usage: %s <filepath>\n", toolname);
}

static void err(const char *msg)
{
    printf("[ERROR] %s: %s\n\n", toolname, msg);
    print_usage();
    exit(1);
}

/*===========================================================================
                                GLOBAL FUNCTIONS
=============================================================================*/
int main (int argc, char *argv[])
{
    char *filepath   = NULL;
    FILE *file       = NULL;
    FILE *tempfile   = NULL;

    toolname = argv[0];
    filepath = argv[1];

    if (NULL == filepath) {
        err("invalid input file path");
    }

    file = fopen(filepath, "rb");
    if (NULL == file) {
        err("cannot open input file");
    }

    tempfile = fopen(TEMPFILENAME, "wb+");
    if (NULL == tempfile) {
        err("cannot create a temporary file");
    }

    char byte;
    while (0 != fread(&byte, sizeof(char), 1, file)) {
        if ('\r' != byte) {
            fwrite(&byte, sizeof(char), 1, tempfile);
        }
    }

    /* Re-open the file to destroy the previous content */
    fclose(file);
    file = fopen(filepath, "wb");
    if (NULL == file) {
        err("cannot update input file");
    }
    fseek(tempfile, 0, SEEK_SET);
    while (0 != fread(&byte, sizeof(char), 1, tempfile)) {
        fwrite(&byte, sizeof(char), 1, file);
    }

    fclose(file);
    fclose(tempfile);
    remove(TEMPFILENAME);

    printf("%s has been executed successfully.\n", toolname);
    return 0;
}
