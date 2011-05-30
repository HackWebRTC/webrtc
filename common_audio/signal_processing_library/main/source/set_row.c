/*
 * set_row.c
 *
 * This file contains the function WebRtcSpl_SetRow().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#ifdef _DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

/*
 * Insert a vector into a row in the matrix
 */
void WebRtcSpl_SetRow(G_CONST WebRtc_Word32 *in_row, WebRtc_Word16 in_column_length,
                      WebRtc_Word32 *matrix, WebRtc_Word16 number_of_rows,
                      WebRtc_Word16 number_of_cols, WebRtc_Word16 row_chosen)
{
    WebRtc_Word16 i;
    G_CONST WebRtc_Word32 *inarrptr = in_row;
    WebRtc_Word32 *matptr = &matrix[row_chosen * number_of_cols];

#ifdef _DEBUG
    if (in_column_length != number_of_cols)
    {
        printf(" SetRow : the vector to be inserted does not have the same length as a row in the matrix\n");
        exit(0);
    }
    if ((row_chosen < 0) || (row_chosen >= number_of_rows))
    {
        printf(" SetRow : selected row is negative or larger than the dimension of the matrix\n");
        exit(0);
    }
#endif
    /* Unused input variable */
    number_of_rows = number_of_rows;

    for (i = 0; i < in_column_length; i++)
    {
        (*matptr++) = (*inarrptr++);
    }
}
