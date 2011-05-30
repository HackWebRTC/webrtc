/*
 * get_rows.c
 *
 * This file contains the function WebRtcSpl_GetRow().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

WebRtc_Word16 WebRtcSpl_GetRow(G_CONST WebRtc_Word32 *matrix, WebRtc_Word16 number_of_rows,
                               WebRtc_Word16 number_of_cols, WebRtc_Word16 row_chosen,
                               WebRtc_Word32 *row_out, WebRtc_Word16 max_length)
{
    WebRtc_Word16 i;
    WebRtc_Word32 *outarrptr = row_out;
    G_CONST WebRtc_Word32 *matptr = &matrix[row_chosen * number_of_cols];

#ifdef _DEBUG
    if (max_length < number_of_cols)
    {
        printf(" GetRow : out vector is shorter than the row length\n");
        exit(0);
    }
    if ((row_chosen < 0) || (row_chosen >= number_of_rows))
    {
        printf(" GetRow : selected row is negative or larger than the dimension of the matrix\n");
        exit(0);
    }
#endif
    /* Unused input variable */
    max_length = max_length;
    number_of_rows = number_of_rows;

    for (i = 0; i < number_of_cols; i++)
    {
        (*outarrptr++) = (*matptr++);
    }
    return number_of_cols;
}
