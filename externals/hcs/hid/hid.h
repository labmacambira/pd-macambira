#ifndef _HID_H
#define _HID_H

#include <stdio.h>

#include <m_pd.h>

/* 
 * this is automatically generated from linux/input.h by
 * make-arrays-from-input.h.pl to be the cross-platform event types and codes 
 */
#include "input_arrays.h"

static char *version = "$Revision: 1.6 $";

/*------------------------------------------------------------------------------
 *  CLASS DEF
 */
static t_class *hid_class;

typedef struct _hid 
{
		t_object            x_obj;
		t_int               x_fd;
		t_symbol            *x_devname;
		t_int               x_device_number;
		t_clock             *x_clock;
		t_int               x_read_ok;
		t_int               x_started;
		t_int               x_delay;
		long                x_locID;
} t_hid;


/*------------------------------------------------------------------------------
 *  GLOBALS
 */

/* TODO: what are these for again? */
char *deviceList[64];
char *typeList[256];
char *codeList[256];

/*------------------------------------------------------------------------------
 *  FUNCTION PROTOTYPES FOR DIFFERENT PLATFORMS
 */

/* generic, cross-platform functions */
t_int hid_open_device(t_hid *x, t_int device_number);
t_int hid_close_device(t_hid *x);
t_int hid_devicelist_refresh(t_hid* x);
t_int hid_output_events(t_hid *x) ;

#endif  /* #ifndef _HID_H */
