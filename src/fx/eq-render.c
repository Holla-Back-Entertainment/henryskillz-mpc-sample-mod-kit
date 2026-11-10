#include <stdlib.h>
#include <string.h>
#include "ladspa.h"
#include "hs_eq2.h"

extern const LADSPA_Descriptor hs_eq2_descriptor;
int eq_render(unsigned long rate, unsigned long frames, float *l,float *r,float *ol,float *orr,float *v)
{
    const LADSPA_Descriptor *d=&hs_eq2_descriptor;
    LADSPA_Handle h=d->instantiate(d,rate);
    if (!h) return 0;
    for (int i=0;i<EQ_COUNT;++i) d->connect_port(h,4+i,v+i);
    d->activate(h);
    for (unsigned long offset=0;offset<frames;offset+=128) {
        unsigned long n=frames-offset<128 ? frames-offset : 128;
        d->connect_port(h,0,l+offset); d->connect_port(h,1,r+offset);
        d->connect_port(h,2,ol+offset); d->connect_port(h,3,orr+offset);
        d->run(h,n);
    }
    d->cleanup(h);
    return 1;
}
