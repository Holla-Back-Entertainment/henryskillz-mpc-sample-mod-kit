#include <assert.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "hs_eq2.h"
#include "ladspa.h"
int main(int argc,char **argv)
{
    assert(argc==2);
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    assert(lib);
    LADSPA_Descriptor_Function desc=(LADSPA_Descriptor_Function)dlsym(lib,"ladspa_descriptor");
    assert(desc);
    const LADSPA_Descriptor *d=desc(HS_EQ2_ID);
    assert(d && d->PortCount==EQ_COUNT+4);
    enum {RATE=44100,BLOCK=128,ITER=6000};
    LADSPA_Handle h=d->instantiate(d,RATE);
    assert(h);
    float p[EQ_COUNT]={0}, l[BLOCK],r[BLOCK],ol[BLOCK],orr[BLOCK];
    for (int i=0;i<EQ_BANDS;++i) {
        int o=EQ_FIRST+i*EB_COUNT;
        p[o+EB_FREQ]=1000+10*i;
        p[o+EB_Q]=1;
        p[o+EB_ON]=1;
        p[o+EB_FAMILY]=4;
        p[o+EB_SLOPE]=8;
        p[o+EB_STEREO]=2;
    }
    for (int i=0;i<EQ_COUNT;++i) d->connect_port(h,i+4,p+i);
    d->connect_port(h,0,l);d->connect_port(h,1,r);
    d->connect_port(h,2,ol);d->connect_port(h,3,orr);
    d->activate(h);
    double sum=0;
    clock_t start=clock();
    for (int i=0;i<ITER;++i) {
        for (int j=0;j<BLOCK;++j) {
            l[j]=sinf((i*BLOCK+j)*1.4f)*.1f;
            r[j]=l[j]*.4f;
        }
        d->run(h,BLOCK);
        for (int j=0;j<BLOCK;++j) {
            assert(isfinite(ol[j])&&isfinite(orr[j]));
            sum+=ol[j]*ol[j];
        }
    }
    assert(sum>0);
    printf("24 bands, 96 sections, stereo at 44.1 kHz: %.1fx realtime on virtual MPC CPU\n",
           ((double)BLOCK*ITER/RATE)/((double)(clock()-start)/CLOCKS_PER_SEC));
    d->cleanup(h);
    dlclose(lib);
}
