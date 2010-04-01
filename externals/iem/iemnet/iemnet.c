/* iemnet
 *  copyright (c) 2010 IOhannes m zm�lnig, IEM
 */

//#define DEBUG

#include "iemnet.h"

void iemnet__addrout(t_outlet*status_outlet, t_outlet*address_outlet, 
		     long address, unsigned short port) {

  static t_atom addr[5];
  static int firsttime=1;

  if(firsttime) {
    int i=0;
    for(i=0; i<5; i++)SETFLOAT(addr+i, 0);
    firsttime=0;
  }

  addr[0].a_w.w_float = (address & 0xFF000000)>>24;
  addr[1].a_w.w_float = (address & 0x0FF0000)>>16;
  addr[2].a_w.w_float = (address & 0x0FF00)>>8;
  addr[3].a_w.w_float = (address & 0x0FF);
  addr[4].a_w.w_float = port;

  if(status_outlet )outlet_anything(status_outlet , gensym("address"), 5, addr);
  if(address_outlet)outlet_list    (address_outlet, gensym("list"   ), 5, addr);
}

void iemnet__numconnout(t_outlet*status_outlet, t_outlet*numcon_outlet, int numconnections) {
  t_atom atom[1];
  SETFLOAT(atom, numconnections);

  if(status_outlet)outlet_anything(status_outlet , gensym("connections"), 1, atom);
  if(numcon_outlet)outlet_float   (numcon_outlet, numconnections);
}

void iemnet__socketout(t_outlet*status_outlet, t_outlet*socket_outlet, int socketfd) {
  t_atom atom[1];
  SETFLOAT(atom, socketfd);

  if(status_outlet)outlet_anything(status_outlet , gensym("socket"), 1, atom);
  if(socket_outlet)outlet_float   (socket_outlet, socketfd);
}


void iemnet__streamout(t_outlet*outlet, int argc, t_atom*argv) {
  if(NULL==outlet)return;
  while(argc-->0) {
    outlet_list(outlet, gensym("list"), 1, argv);
    argv++;
  }

}


#ifdef _MSC_VER
void tcpclient_setup(void);
void tcpserver_setup(void);
void tcpsend_setup(void);
#endif



IEMNET_EXTERN void iemnet_setup(void) {
#ifdef _MSC_VER
  tcpclient_setup();
  tcpserver_setup();
  tcpserver_setup();
#endif
}
