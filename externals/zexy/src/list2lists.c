/******************************************************
 *
 * zexy - implementation file
 *
 * copyleft (c) IOhannes m zm�lnig
 *
 *   1999:forum::f�r::uml�ute:2004
 *
 *   institute of electronic music and acoustics (iem)
 *
 ******************************************************
 *
 * license: GNU General Public License v.2
 *
 ******************************************************/

/* 2305:forum::f�r::uml�ute:2001 */


#include "zexy.h"

//#define DEBUG

#ifdef DEBUG
# define DEBUGFUN(x) x
#else
# define DEBUGFUN(x)
#endif


/* ------------------------- list2lists ------------------------------- */

/* split a list into several sublists given by their lenghts */

static t_class *list2lists_class;


typedef struct _list2lists
{
  t_object       x_obj;
  t_outlet      *x_outlet;

  int x_n;

  t_inlet*x_lengin;
  int     x_lcount;
  t_int  *x_length;
} t_list2lists;



static void list2lists_list2(t_list2lists*x,t_symbol*s, int argc, t_atom*argv)
{
  if(x->x_length!=0) {
    freebytes(x->x_length, sizeof(t_atom)*x->x_lcount);
  }
  x->x_lcount=0;
  x->x_length=0;

  DEBUGFUN(post("list of length %d", argc));

  if(argc>0) {
    int i;
    x->x_lcount=argc;
    x->x_length=(t_int*)getbytes((x->x_lcount)*sizeof(t_int));
    for(i=0; i<argc; i++) {
      x->x_length[i]=atom_getint(argv+i);
    }
  }

  DEBUGFUN(post("list2: %d %x", x->x_lcount, x->x_length));
}

static void list2lists_output(t_list2lists*x, int argc, t_atom*argv) 
{
  t_outlet*out=x->x_obj.ob_outlet;
  if(argc<=0)
    outlet_bang(out);
  else
    outlet_list(out, &s_list, argc, argv);
}

static void list2lists_list(t_list2lists *x, t_symbol *s, int argc, t_atom *argv)
{
  int i;
  int offset=0;
  
  if(x->x_lcount<1) {
    outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
    return;
  }

  for(i=0; i<x->x_lcount; i++) {
    int len=x->x_length[i];
    if(len>argc) {
      list2lists_output(x, argc, argv);
      return;
    }
    list2lists_output(x, len, argv);
    argv+=len;
    argc-=len;
  }
}

static void list2lists_free(t_list2lists *x)
{ 
  if(x->x_length) {
    freebytes(x->x_length, x->x_lcount*sizeof(int));
    x->x_length=0;
    x->x_lcount=0;
  }
  inlet_free(x->x_lengin);

}

static void *list2lists_new(t_symbol *s, int argc, t_atom *argv)
{
  t_list2lists *x = (t_list2lists *)pd_new(list2lists_class);
  ZEXY_USEVAR(s);

  outlet_new(&x->x_obj, 0);
  x->x_lengin=inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("list"), gensym("lst2"));

  x->x_lcount=0;
  x->x_length=0;

  list2lists_list2(x, gensym("list"), argc, argv);

  return (x);
}


static void list2lists_help(t_list2lists*x)
{
  post("\n%c list2lists\t\t:: split lists into multiple sublists based on matches", HEARTSYMBOL);
}

void list2lists_setup(void)
{
  list2lists_class = class_new(gensym("list2lists"), (t_newmethod)list2lists_new, 
                             (t_method)list2lists_free, sizeof(t_list2lists), 0, A_GIMME, 0);
  class_addlist    (list2lists_class, list2lists_list);
  class_addmethod  (list2lists_class, (t_method)list2lists_list2, gensym("lst2"), A_GIMME, 0);

  class_addmethod(list2lists_class, (t_method)list2lists_help, gensym("help"), A_NULL);
  zexy_register("list2lists");
}
