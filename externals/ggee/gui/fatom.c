#include <m_pd.h>
#include "g_canvas.h"
#include <ggee.h>

#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif

#include "fatom.h"


static t_class *fatom_class;




void fatom_setup() {
  post("fatom setup");
    fatom_class = class_new(gensym("fatom"), (t_newmethod)fatom_new, 0,
				sizeof(t_fatom),0,A_DEFSYM,0);

  fatom_setup_common(fatom_class);
}


