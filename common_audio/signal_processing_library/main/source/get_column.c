/*
 * get_column.c
 *
 * This file contains the function WebRtcSpl_GetColumn().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

WebRtc_Word16 WebRtcSpl_GetColumn(G_CONST WebRtc_Word32 *matrix, WebRtc_Word16 number_of_rows,
                                  WebRtc_Word16 number_of_cols, WebRtc_Word16 column_chosen,
                                  WebRtc_Word32 *column_out, WebRtc_Word16 max_length)
{
    WebRtc_Word16 i;
    WebRtc_Word32 *outarrptr = column_out;
    G_CONST WebRtc_Word32 *matptr = &matrix[column_chosen];

#ifdef _DEBUG
    if (max_length < number_of_rows)
    {
        printf(" GetColumn : out vector is shorter than the column length\n");
        exit(0);
    }
    if ((column_chosen < 0) || (column_chosen >= number_of_cols))
    {
        printf(" GetColumn : selected column is negative or larger than the dimension of the matrix\n");
        exit(0);
    }
#endif

    /* Unused input variable */
    max_length = max_length;

    for (i = 0; i < number_of_rows; i++)
    {
        (*outarrptr++) = (*matptr);
        matptr += number_of_cols;
    }
    return number_of_rows;
}
