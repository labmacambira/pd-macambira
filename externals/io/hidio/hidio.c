/* --------------------------------------------------------------------------*/
/*                                                                           */
/* interface to native HID (Human Interface Devices) API                     */
/* Written by Hans-Christoph Steiner <hans@at.or.at>                         */
/*                                                                           */
/* Copyright (c) 2004-2006 Hans-Christoph Steiner                            */
/*                                                                           */
/* This program is free software; you can redistribute it and/or             */
/* modify it under the terms of the GNU General Public License               */
/* as published by the Free Software Foundation; either version 2            */
/* of the License, or (at your option) any later version.                    */
/*                                                                           */
/* See file LICENSE for further informations on licensing terms.             */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software Foundation,   */
/* Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.           */
/*                                                                           */
/* --------------------------------------------------------------------------*/

#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hidio.h"

/*------------------------------------------------------------------------------
 * LOCAL DEFINES
 */

#define DEBUG(x)
//#define DEBUG(x) x 

unsigned short global_debug_level = 0;

/*------------------------------------------------------------------------------
 *  GLOBAL VARIABLES
 */

/* count the number of instances of this object so that certain free()
 * functions can be called only after the final instance is detroyed.
 */
t_int hidio_instance_count;

/* this is used to test for the first instance to execute */
double last_execute_time[MAX_DEVICES];

static t_class *hidio_class;

/* mostly for status querying */
unsigned short device_count;

/* store element structs to eliminate symbol table lookups, etc. */
t_hid_element *element[MAX_DEVICES][MAX_ELEMENTS];
/* number of active elements per device */
unsigned short element_count[MAX_DEVICES]; 

/* pre-generated symbols */
t_symbol *ps_open, *ps_device, *ps_poll, *ps_total, *ps_range;

/*------------------------------------------------------------------------------
 * FUNCTION PROTOTYPES
 */

//static void hidio_poll(t_hidio *x, t_float f);
static void hidio_open(t_hidio *x, t_symbol *s, int argc, t_atom *argv);
//static t_int hidio_close(t_hidio *x);
//static t_int hidio_read(t_hidio *x,int fd);
//static void hidio_float(t_hidio* x, t_floatarg f);


/*------------------------------------------------------------------------------
 * SUPPORT FUNCTIONS
 */

void debug_print(t_int message_debug_level, const char *fmt, ...)
{
	if(message_debug_level <= global_debug_level)
	{
		char buf[MAXPDSTRING];
		va_list ap;
		//t_int arg[8];
		va_start(ap, fmt);
		vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
		post(buf);
		va_end(ap);
	}
}

void debug_error(t_hidio *x, t_int message_debug_level, const char *fmt, ...)
{
	if(message_debug_level <= global_debug_level)
	{
		char buf[MAXPDSTRING];
		va_list ap;
		//t_int arg[8];
		va_start(ap, fmt);
		vsnprintf(buf, MAXPDSTRING-1, fmt, ap);
		pd_error(x, buf);
		va_end(ap);
	}
}


static void output_status(t_hidio *x, t_symbol *selector, t_float output_value)
{
	t_atom *output_atom = (t_atom *)getbytes(sizeof(t_atom));
#ifdef PD
	SETFLOAT(output_atom, output_value);
#else
	atom_setlong(output_atom, output_value);
#endif
	outlet_anything( x->x_status_outlet, selector, 1, output_atom);
	freebytes(output_atom,sizeof(t_atom));
}

static void output_open_status(t_hidio *x)
{
	output_status(x, ps_open, x->x_device_open);
}

static void output_device_number(t_hidio *x)
{
	output_status(x, ps_device, x->x_device_number);
}

static void output_poll_time(t_hidio *x)
{
	output_status(x, ps_poll, x->x_delay);
}

static void output_device_count(t_hidio *x)
{
	output_status(x, ps_total, device_count);
}

static void output_element_ranges(t_hidio *x)
{
	if( (x->x_device_number > -1) && (x->x_device_open) )
	{
		unsigned int i;
		t_atom output_data[4];
		
		for(i=0;i<element_count[x->x_device_number];++i)
		{
			SETSYMBOL(output_data, element[x->x_device_number][i]->type);
			SETSYMBOL(output_data + 1, element[x->x_device_number][i]->name);
#ifdef PD
			SETFLOAT(output_data + 2, element[x->x_device_number][i]->min);
			SETFLOAT(output_data + 3, element[x->x_device_number][i]->max);
#else
			atom_setlong(output_data + 2, element[x->x_device_number][i]->min);
			atom_setlong(output_data + 3, element[x->x_device_number][i]->max);
#endif
			outlet_anything(x->x_status_outlet, ps_range, 4, output_data);
		}
	}
}


static unsigned int name_to_usage(char *usage_name)
{ // output usagepage << 16 + usage
	if(strcmp(usage_name,"pointer") == 0)   return(0x00010001);
	if(strcmp(usage_name,"mouse") == 0)     return(0x00010002);
	if(strcmp(usage_name,"joystick") == 0)  return(0x00010004);
	if(strcmp(usage_name,"gamepad") == 0)   return(0x00010005);
	if(strcmp(usage_name,"keyboard") == 0)  return(0x00010006);
	if(strcmp(usage_name,"keypad") == 0)    return(0x00010007);
	if(strcmp(usage_name,"multiaxiscontroller") == 0) return(0x00010008);
	return(0);
}


static short get_device_number_from_arguments(int argc, t_atom *argv)
{
#ifdef PD
	short device_number = -1;
	char device_type_string[MAXPDSTRING] = "";
	unsigned short device_type_instance;
#else
	long device_number = -1;
	char *device_type_string;
	long device_type_instance;
#endif
	unsigned int usage;
	unsigned short vendor_id;
	unsigned short product_id;
	t_symbol *first_argument;
	t_symbol *second_argument;

	if(argc == 1)
	{
#ifdef PD
		first_argument = atom_getsymbolarg(0,argc,argv);
		if(first_argument == &s_) 
#else
		atom_arg_getsym(&first_argument, 0,argc,argv);
		if(first_argument == _sym_nothing) 
#endif
		{ // single float arg means device #
			post("first_argument == &s_");
#ifdef PD
			device_number = (short) atom_getfloatarg(0,argc,argv);
#else
			atom_arg_getlong(&device_number, 0, argc, argv);
#endif
			if(device_number < 0) device_number = -1;
			debug_print(LOG_DEBUG,"[hidio] setting device# to %d",device_number);
		}
		else
		{ // single symbol arg means first instance of a device type
#ifdef PD
			atom_string(argv, device_type_string, MAXPDSTRING-1);
#else
			device_type_string = atom_string(argv);
			// LATER do we have to free this string manually???
#endif
			usage = name_to_usage(device_type_string);
			device_number = get_device_number_from_usage(0, usage >> 16, 
														 usage & 0xffff);
			debug_print(LOG_INFO,"[hidio] using 0x%04x 0x%04x for %s",
						usage >> 16, usage & 0xffff, device_type_string);
		}
	}
	else if(argc == 2)
	{ 
#ifdef PD
		first_argument = atom_getsymbolarg(0,argc,argv);
		second_argument = atom_getsymbolarg(1,argc,argv);
		if( second_argument == &s_ ) 
#else
		atom_arg_getsym(&first_argument, 0,argc,argv);
		atom_arg_getsym(&second_argument, 1,argc,argv);
		if( second_argument == _sym_nothing ) 
#endif
		{ /* a symbol then a float means match on usage */
#ifdef PD
			atom_string(argv, device_type_string, MAXPDSTRING-1);
			usage = name_to_usage(device_type_string);
			device_type_instance = atom_getfloatarg(1,argc,argv);
#else
			device_type_string = atom_string(argv);
			usage = name_to_usage(device_type_string);
			atom_arg_getlong(&device_type_instance, 1, argc, argv);
#endif
			debug_print(LOG_DEBUG,"[hidio] looking for %s at #%d",
						device_type_string, device_type_instance);
			device_number = get_device_number_from_usage(device_type_instance,
															  usage >> 16, 
															  usage & 0xffff);
		}
		else
		{ /* two symbols means idVendor and idProduct in hex */
			vendor_id = 
				(unsigned short) strtol(first_argument->s_name, NULL, 16);
			product_id = 
				(unsigned short) strtol(second_argument->s_name, NULL, 16);
			device_number = get_device_number_by_id(vendor_id,product_id);
		}
	}
	return(device_number);
}


void hidio_output_event(t_hidio *x, t_hid_element *output_data)
{
	if( (output_data->value != output_data->previous_value) ||
		(output_data->relative) )  // relative data should always be output
	{
		t_atom event_data[3];
		SETSYMBOL(event_data, output_data->name);
		SETFLOAT(event_data + 1, output_data->instance);
		SETFLOAT(event_data + 2, output_data->value);
		outlet_anything(x->x_data_outlet,output_data->type,3,event_data);
	} 
}


/* stop polling the device */
static void stop_poll(t_hidio* x) 
{
  debug_print(LOG_DEBUG,"stop_poll");
  
  if (x->x_started) 
  { 
	  clock_unset(x->x_clock);
	  debug_print(LOG_INFO,"[hidio] polling stopped");
	  x->x_started = 0;
  }
}

/*------------------------------------------------------------------------------
 * METHODS FOR [hidio]'s MESSAGES                    
 */


void hidio_poll(t_hidio* x, t_float f) 
{
	debug_print(LOG_DEBUG,"hidio_poll");
  
/*	if the user sets the delay less than 2, set to block size */
	if( f > 2 )
		x->x_delay = (t_int)f;
	else if( f > 0 ) //TODO make this the actual time between message processing
		x->x_delay = 1.54; 
	if(x->x_device_number > -1) 
	{
		if(!x->x_device_open) 
			hidio_open(x,ps_open,0,NULL);
		if(!x->x_started) 
		{
			clock_delay(x->x_clock, x->x_delay);
			debug_print(LOG_DEBUG,"[hidio] polling started");
			x->x_started = 1;
		} 
	}
}

static void hidio_set_from_float(t_hidio *x, t_floatarg f)
{
/* values greater than 1 set the polling delay time */
/* 1 and 0 for start/stop so you can use a [tgl] */
	if(f > 1)
	{
		x->x_delay = (t_int)f;
		hidio_poll(x,f);
	}
	else if(f == 1) 
	{
		if(! x->x_started)
			hidio_poll(x,f);
	}
	else if(f == 0) 		
	{
		stop_poll(x);
	}
}

/* close the device */
t_int hidio_close(t_hidio *x) 
{
	debug_print(LOG_DEBUG,"hidio_close");

/* just to be safe, stop it first */
	stop_poll(x);

	if(! hidio_close_device(x)) 
	{
		debug_print(LOG_INFO,"[hidio] closed device %d",x->x_device_number);
		x->x_device_open = 0;
		return (0);
	}

	return (1);
}


/* hidio_open behavoir
 * current state                 action
 * ---------------------------------------
 * closed / same device          open 
 * open / same device            no action 
 * closed / different device     open 
 * open / different device       close open 
 */
static void hidio_open(t_hidio *x, t_symbol *s, int argc, t_atom *argv) 
{
	debug_print(LOG_DEBUG,"hid_%s",s->s_name);
	short device_number;
	
	pthread_mutex_lock(&x->x_mutex);
	device_number = get_device_number_from_arguments(argc, argv);
	if(device_number > -1)
	{
		if( (device_number != x->x_device_number) && (x->x_device_open) ) 
			hidio_close(x);
		if(! x->x_device_open)
		{
			x->x_device_number = device_number;
			x->x_requestcode = REQUEST_OPEN;
			pthread_cond_signal(&x->x_requestcondition);
		}
	}
	else debug_print(LOG_WARNING,"[hidio] device does not exist");
	pthread_mutex_unlock(&x->x_mutex);
}


t_int hidio_read(t_hidio *x, int fd) 
{
//	debug_print(LOG_DEBUG,"hidio_read");
	unsigned int i;
#ifdef PD
	double right_now = clock_getlogicaltime();
#else
	double right_now = (double)systime_ms();
#endif
	t_hid_element *current_element;
	
	if(right_now > last_execute_time[x->x_device_number])
	{
		hidio_get_events(x);
		last_execute_time[x->x_device_number] = right_now;
/*		post("executing: instance %d/%d at %ld", 
		x->x_instance, hidio_instance_count, right_now);*/
	}
	for(i=0; i< element_count[x->x_device_number]; ++i)
	{
		current_element = element[x->x_device_number][i];
		if(current_element->previous_value != current_element->value)
		{
			hidio_output_event(x, current_element);
			if(!current_element->relative)
				current_element->previous_value = current_element->value;
		}
	}
	if (x->x_started) 
	{
		clock_delay(x->x_clock, x->x_delay);
	}
	
	// TODO: why is this 1? 
	return 1; 
}

static void hidio_info(t_hidio *x)
{
	pthread_mutex_lock(&x->x_mutex);
	x->x_requestcode = REQUEST_INFO;
	pthread_cond_signal(&x->x_requestcondition);
	pthread_mutex_unlock(&x->x_mutex);
}

static void hidio_float(t_hidio* x, t_floatarg f) 
{
	debug_print(LOG_DEBUG,"hid_float");

	hidio_set_from_float(x,f);
}

#ifndef PD
static void hidio_int(t_hidio* x, long l) 
{
	debug_print(LOG_DEBUG,"hid_int");

	hidio_set_from_float(x, (float)l);
}
#endif

static void hidio_debug(t_hidio *x, t_float f)
{
	global_debug_level = f;
}


/*------------------------------------------------------------------------------
 * child thread
 */
 
static void *hidio_child(void *zz)
{
    t_hidio *x = zz;

    pthread_mutex_lock(&x->x_mutex);
    while (1)
    {
		if (x->x_requestcode == REQUEST_NOTHING)
		{
			pthread_cond_signal(&x->x_answercondition);
			pthread_cond_wait(&x->x_requestcondition, &x->x_mutex);
		}
		else if (x->x_requestcode == REQUEST_OPEN)
		{
			short device_number = x->x_device_number;
			/* store running state to be restored after the device has been opened */
			t_int started = x->x_started;
			int err;
			pthread_mutex_unlock(&x->x_mutex);
			err = hidio_open_device(x, device_number);
			pthread_mutex_lock(&x->x_mutex);
			if (err)
			{
				x->x_device_number = -1;
				error("[hidio] can not open device %d",device_number);
			}
			else
			{
				x->x_device_open = 1;
				pthread_mutex_unlock(&x->x_mutex);
				/* restore the polling state so that when I [tgl] is used to start/stop [hidio],
				 * the [tgl]'s state will continue to accurately reflect [hidio]'s state  */
				if (started)
					hidio_set_from_float(x,x->x_delay);
				debug_print(LOG_DEBUG,"[hidio] set device# to %d",device_number);
				output_open_status(x);
				output_device_number(x);
				pthread_mutex_lock(&x->x_mutex);
			}
			if (x->x_requestcode == REQUEST_OPEN)
				x->x_requestcode = REQUEST_NOTHING;
			pthread_cond_signal(&x->x_answercondition);
		}
		else if (x->x_requestcode == REQUEST_READ)
		{
			if (x->x_requestcode == REQUEST_READ)
				x->x_requestcode = REQUEST_NOTHING;
			pthread_cond_signal(&x->x_answercondition);
		}
		else if (x->x_requestcode == REQUEST_SEND)
		{
			if (x->x_requestcode == REQUEST_SEND)
				x->x_requestcode = REQUEST_NOTHING;
			pthread_cond_signal(&x->x_answercondition);
		}
		else if (x->x_requestcode == REQUEST_INFO)
		{
			pthread_mutex_unlock(&x->x_mutex);
			output_open_status(x);
			output_device_number(x);
			output_device_count(x);
			output_poll_time(x);
			output_element_ranges(x);
			hidio_platform_specific_info(x);
			pthread_mutex_lock(&x->x_mutex);
			if (x->x_requestcode == REQUEST_INFO)
				x->x_requestcode = REQUEST_NOTHING;
			pthread_cond_signal(&x->x_answercondition);
		}
		else if (x->x_requestcode == REQUEST_CLOSE)
		{
			pthread_mutex_unlock(&x->x_mutex);
			hidio_close(x);
			pthread_mutex_lock(&x->x_mutex);
			if (x->x_requestcode == REQUEST_CLOSE)
				x->x_requestcode = REQUEST_NOTHING;
			pthread_cond_signal(&x->x_answercondition);
		}
		else if (x->x_requestcode == REQUEST_QUIT)
		{
			pthread_mutex_unlock(&x->x_mutex);
			hidio_close(x);
			pthread_mutex_lock(&x->x_mutex);
			x->x_requestcode = REQUEST_NOTHING;
			pthread_cond_signal(&x->x_answercondition);
			break;	/* leave the while loop */
		}
		else
		{
			;	/* nothing: shouldn't get here anyway */
		}
    }
    pthread_mutex_unlock(&x->x_mutex);
    return (0);
}

 
/*------------------------------------------------------------------------------
 * system functions 
 */
static void hidio_free(t_hidio* x) 
{
    void *threadrtn;

	debug_print(LOG_DEBUG,"hidio_free");
		
	/* stop polling for input */
	if (x->x_clock)
		clock_unset(x->x_clock);
		
    pthread_mutex_lock(&x->x_mutex);
    /* request QUIT and wait for acknowledge */
    x->x_requestcode = REQUEST_QUIT;
	if (x->x_thread)
	{
		post("hidio: stopping worker thread. . .");
		pthread_cond_signal(&x->x_requestcondition);
		while (x->x_requestcode != REQUEST_NOTHING)
		{
			post("hidio: ...signalling...");
			pthread_cond_signal(&x->x_requestcondition);
    		pthread_cond_wait(&x->x_answercondition, &x->x_mutex);
		}
		pthread_mutex_unlock(&x->x_mutex);
		if (pthread_join(x->x_thread, &threadrtn))
    		error("hidio_free: join failed");
		post("hidio: ...done");
    }
	else pthread_mutex_unlock(&x->x_mutex);

	clock_free(x->x_clock);
	hidio_instance_count--;

	hidio_platform_specific_free(x);
	
    pthread_cond_destroy(&x->x_requestcondition);
    pthread_cond_destroy(&x->x_answercondition);
    pthread_mutex_destroy(&x->x_mutex);
}

/* create a new instance of this class */
static void *hidio_new(t_symbol *s, int argc, t_atom *argv) 
{
#ifdef PD
	t_hidio *x = (t_hidio *)pd_new(hidio_class);
	unsigned int i;
	
#if !defined(__linux__) && !defined(__APPLE__)
  error("    !! WARNING !! WARNING !! WARNING !! WARNING !! WARNING !! WARNING !!");
  error("     This is a dummy, since this object only works GNU/Linux and MacOS X!");
  error("    !! WARNING !! WARNING !! WARNING !! WARNING !! WARNING !! WARNING !!");
#endif

  /* init vars */
  global_debug_level = 9; /* high numbers here means see more messages */
  x->x_has_ff = 0;
  x->x_device_open = 0;
  x->x_started = 0;
  x->x_delay = DEFAULT_DELAY;
  for(i=0; i<MAX_DEVICES; ++i) last_execute_time[i] = 0;

  x->x_clock = clock_new(x, (t_method)hidio_read);

  /* create anything outlet used for HID data */ 
  x->x_data_outlet = outlet_new(&x->x_obj, 0);
  x->x_status_outlet = outlet_new(&x->x_obj, 0);
#else /* Max */
	t_hidio *x = (t_hidio *)object_alloc(hidio_class);
	unsigned int i;
	
  /* init vars */
  global_debug_level = 9; /* high numbers here means see more messages */
  x->x_has_ff = 0;
  x->x_device_open = 0;
  x->x_started = 0;
  x->x_delay = DEFAULT_DELAY;
  for(i=0; i<MAX_DEVICES; ++i) last_execute_time[i] = 0;

  x->x_clock = clock_new(x, (method)hidio_read);

  /* create anything outlet used for HID data */ 
  x->x_status_outlet = outlet_new(x, "anything");
  x->x_data_outlet = outlet_new(x, "anything");
#endif

    pthread_mutex_init(&x->x_mutex, 0);
    pthread_cond_init(&x->x_requestcondition, 0);
    pthread_cond_init(&x->x_answercondition, 0);

  x->x_device_number = get_device_number_from_arguments(argc, argv);
  
  x->x_instance = hidio_instance_count;
  hidio_instance_count++;

	x->x_requestcode = REQUEST_NOTHING;
    pthread_create(&x->x_thread, 0, hidio_child, x);
	
  return (x);
}

#ifdef PD
void hidio_setup(void) 
{
	hidio_class = class_new(gensym("hidio"), 
								 (t_newmethod)hidio_new, 
								 (t_method)hidio_free,
								 sizeof(t_hidio),
								 CLASS_DEFAULT,
								 A_GIMME,0);
	
	/* add inlet datatype methods */
	class_addfloat(hidio_class,(t_method) hidio_float);
	class_addbang(hidio_class,(t_method) hidio_read);
/* 	class_addanything(hidio_class,(t_method) hidio_anything); */
	
	/* add inlet message methods */
	class_addmethod(hidio_class,(t_method) hidio_debug,gensym("debug"),A_DEFFLOAT,0);
	class_addmethod(hidio_class,(t_method) hidio_build_device_list,gensym("refresh"),0);
	class_addmethod(hidio_class,(t_method) hidio_print,gensym("print"),0);
	class_addmethod(hidio_class,(t_method) hidio_info,gensym("info"),0);
	class_addmethod(hidio_class,(t_method) hidio_open,gensym("open"),A_GIMME,0);
	class_addmethod(hidio_class,(t_method) hidio_close,gensym("close"),0);
	class_addmethod(hidio_class,(t_method) hidio_poll,gensym("poll"),A_DEFFLOAT,0);
   /* force feedback messages */
	class_addmethod(hidio_class,(t_method) hidio_ff_autocenter,
						 gensym("ff_autocenter"),A_DEFFLOAT,0);
	class_addmethod(hidio_class,(t_method) hidio_ff_gain,gensym("ff_gain"),A_DEFFLOAT,0);
	class_addmethod(hidio_class,(t_method) hidio_ff_motors,gensym("ff_motors"),A_DEFFLOAT,0);
	class_addmethod(hidio_class,(t_method) hidio_ff_continue,gensym("ff_continue"),0);
	class_addmethod(hidio_class,(t_method) hidio_ff_pause,gensym("ff_pause"),0);
	class_addmethod(hidio_class,(t_method) hidio_ff_reset,gensym("ff_reset"),0);
	class_addmethod(hidio_class,(t_method) hidio_ff_stopall,gensym("ff_stopall"),0);
	/* ff tests */
	class_addmethod(hidio_class,(t_method) hidio_ff_fftest,gensym("fftest"),A_DEFFLOAT,0);
	class_addmethod(hidio_class,(t_method) hidio_ff_print,gensym("ff_print"),0);


	post("[hidio] %d.%d, written by Hans-Christoph Steiner <hans@eds.org>",
		 HIDIO_MAJOR_VERSION, HIDIO_MINOR_VERSION);  
	post("\tcompiled on "__DATE__" at "__TIME__ " ");
	
	/* pre-generate often used symbols */
	ps_open = gensym("open");
	ps_device = gensym("device");
	ps_poll = gensym("poll");
	ps_total = gensym("total");
	ps_range = gensym("range");

}
#else /* Max */
static void hidio_notify(t_hidio *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
	if (msg == _sym_free)	// this message is sent when a child object is freeing
	{
		object_detach(gensym("_obex_hidio"), s, x);
		object_unregister(sender); 
	}
}

static void hidio_assist(t_hidio *x, void *b, long m, long a, char *s)
{
	if (m == 2)
	{
		sprintf(s, "hidio outlet");
	}
	else
	{
		switch (a)
		{	
		case 0:
			sprintf(s, "inlet 1");
			break;
		case 1:
			sprintf(s, "inlet 2");
			break;
		}
	}
}

int main()
{
	t_class	*c;
	
	c = class_new("hidio", (method)hidio_new, (method)hidio_free, (short)sizeof(t_hidio), 
		0L, A_GIMME, 0);
	
	/* initialize the common symbols, since we want to use them */
	common_symbols_init();

	/* register the byte offset of obex with the class */
	class_obexoffset_set(c, calcoffset(t_hidio, x_obex));
	
	/* add methods to the class */
	class_addmethod(c, (method)hidio_int, 			"int",	 		A_LONG, 0);  
	class_addmethod(c, (method)hidio_float,			"float", 		A_FLOAT, 0);  
	class_addmethod(c, (method)hidio_read, 			"bang",	 		A_GIMME, 0); 
	
	/* add inlet message methods */
	class_addmethod(c, (method)hidio_debug, "debug",A_DEFFLOAT,0);
	class_addmethod(c, (method)hidio_build_device_list, "refresh",0);
	class_addmethod(c, (method)hidio_print, "print",0);
	class_addmethod(c, (method)hidio_info, "info",0);
	class_addmethod(c, (method)hidio_open, "open",A_GIMME,0);
	class_addmethod(c, (method)hidio_close, "close",0);
	class_addmethod(c, (method)hidio_poll, "poll",A_DEFFLOAT,0);
   /* force feedback messages */
	class_addmethod(c, (method)hidio_ff_autocenter, "ff_autocenter",A_DEFFLOAT,0);
	class_addmethod(c, (method)hidio_ff_gain, "ff_gain",A_DEFFLOAT,0);
	class_addmethod(c, (method)hidio_ff_motors, "ff_motors",A_DEFFLOAT,0);
	class_addmethod(c, (method)hidio_ff_continue, "ff_continue",0);
	class_addmethod(c, (method)hidio_ff_pause, "ff_pause",0);
	class_addmethod(c, (method)hidio_ff_reset, "ff_reset",0);
	class_addmethod(c, (method)hidio_ff_stopall, "ff_stopall",0);
	/* ff tests */
	class_addmethod(c, (method)hidio_ff_fftest, "fftest",A_DEFFLOAT,0);
	class_addmethod(c, (method)hidio_ff_print, "ff_print",0);

	class_addmethod(c, (method)hidio_assist, 		"assist",	 	A_CANT, 0);  

	/* add a notify method, so we get notifications from child objects */
	class_addmethod(c, (method)hidio_notify,		"notify",		A_CANT, 0); 
	// add methods for dumpout and quickref	
	class_addmethod(c, (method)object_obex_dumpout,		"dumpout",		A_CANT, 0); 
	class_addmethod(c, (method)object_obex_quickref,	"quickref",		A_CANT, 0);

	/* we want this class to instantiate inside of the Max UI; ergo CLASS_BOX */
	class_register(CLASS_BOX, c);
	hidio_class = c;

	finder_addclass("Devices", "hidio");
	post("hidio: � 2006 by Olaf Matthes");
	
	/* pre-generate often used symbols */
	ps_open = gensym("open");
	ps_device = gensym("device");
	ps_poll = gensym("poll");
	ps_total = gensym("total");
	ps_range = gensym("range");

	return 0;
}
#endif

