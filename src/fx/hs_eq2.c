#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ladspa.h"
#include "hs_eq2.h"
#include "hs_eq2_curves.h"

enum { IL, IR, OL, OR, FIRST, PORTS = FIRST+EQ_COUNT, MAX_SECTIONS = 4, TAP = 2048 };
struct section {
    double b0,b1,b2,a1,a2, z1[2],z2[2];
};
struct band {
    struct section s[MAX_SECTIONS];
    int count, type, stereo;
};
struct eq {
    float *port[PORTS];
    float values[EQ_COUNT];
    struct band bands[EQ_BANDS];
    struct band *active[EQ_BANDS];
    unsigned long rate;
    double in_gain, out_gain;
    int active_count, mid_side, parallel, ready;
    float tap[TAP];                 /* last TAP output samples, (L+R)/2, for the editor's spectrum */
    unsigned tap_w;
};

#define BAND_LIST(X) X(1) X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12)                      X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24)
#define B (LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE)
#define D0 (B|LADSPA_HINT_DEFAULT_0)
#define DGAIN (B|LADSPA_HINT_DEFAULT_HIGH)
#define INT0 (D0|LADSPA_HINT_INTEGER)
#define INTMAX (B|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_MAXIMUM)
#define TOGGLE (D0|LADSPA_HINT_TOGGLED)
#define BAND_HINTS(i)     {B|LADSPA_HINT_LOGARITHMIC|LADSPA_HINT_DEFAULT_MIDDLE,20,20000},     {D0,-20,20}, {B|LADSPA_HINT_DEFAULT_1,.3,10.3}, {INTMAX,0,4},     {TOGGLE,0,1}, {TOGGLE,0,1}, {INT0,0,8}, {INT0,0,5}, {INT0,0,8}, {INTMAX,0,2},
static const LADSPA_PortRangeHint hints[PORTS] = {
    /* SampQOL initialises the first LADSPA control from its normalised default.
       DEFAULT_0 becomes the lower bound there. For -36..+12, DEFAULT_HIGH is
       exactly 0 dB: .25 * -36 + .75 * 12. */
    {0}, {0}, {0}, {0}, {DGAIN,-36,12}, {DGAIN,-36,12}, {TOGGLE,0,1}, {TOGGLE,0,1}, {TOGGLE,0,1},
    BAND_LIST(BAND_HINTS)
};
#undef BAND_HINTS
#undef B
#undef D0
#undef DGAIN
#undef INT0
#undef INTMAX
#undef TOGGLE

static const float band_defaults[] = {632.455532f, 0, 1, 4, 0, 0, 0, 0, 0, 2};

static LADSPA_Handle instantiate(const LADSPA_Descriptor *desc, unsigned long rate)
{
    (void)desc;
    if (!rate) return NULL;
    struct eq *s=calloc(1,sizeof *s);
    if (s) s->rate=rate;
    return s;
}

static void connect(LADSPA_Handle handle, unsigned long port, LADSPA_Data *value)
{
    struct eq *s=handle;
    if (port<PORTS) s->port[port]=value;
}

static void activate(LADSPA_Handle handle)
{
    struct eq *s=handle;
    s->ready=0;
    memset(s->bands,0,sizeof s->bands);
}

static inline void coefficients(struct section *b,double b0,double b1,double b2,double a0,double a1,double a2)
{
    b->b0=b0/a0; b->b1=b1/a0; b->b2=b2/a0; b->a1=a1/a0; b->a2=a2/a0;
}

static inline double warp(double hz,unsigned long rate)
{
    return tan(3.14159265358979323846*fmin(hz,.475*rate)/rate);
}

static void cutoff(struct section *b, int high, double w, double q)
{
    if (q == 0) {
        double gain=high ? 1 : w;
        coefficients(b,gain,high ? -gain : gain,0,1+w,w-1,0);
        return;
    }
    double w2=w*w, a=w/q, gain=high ? 1 : w2;
    coefficients(b,gain,high ? -2*gain : 2*gain,gain,1+a+w2,2*(w2-1),1-a+w2);
}

static double bell_factor(int shape,double gain,double q,double f)
{
    /* The measured width changes with bell mode and gain. Interpolate in
       log width, so the correction behaves smoothly between the AU sweeps. */
    static const double gains[]={-20,-12,-6,-3,3,6,12,20};
    int k=0;
    while (k<6 && gain>gains[k+1]) ++k;
    double t=(gain-gains[k])/(gains[k+1]-gains[k]);
    const float *fit=eq_bell[shape];
    static const double frequency[]={0,.0208333,.1041667,.2083333,.3125,.3958333,.475};
    static const double width[]={1,1,.925,.726,.478,.278,.107};
    int j=0;
    while (j<5 && f>frequency[j+1]) ++j;
    double h=(f-frequency[j])/(frequency[j+1]-frequency[j]);
    return exp(log(fit[k])*(1-t)+log(fit[k+1])*t)*(q < .7 ? .985 : 1)
        * (width[j]+h*(width[j+1]-width[j]));
}

static void response_band(struct band *b,const float *v,unsigned long rate)
{
    int type=(int)lroundf(v[EB_TYPE]), stereo=(int)lroundf(v[EB_STEREO]);
    int count=0;
    int n=(int)lroundf(v[EB_SLOPE]);
    if (v[EB_ON]>=.5f && v[EB_BYPASS]<.5f) {
        if (type<=EQ_LPF) count=(n+1)/2;
        else if (fabsf(v[EB_GAIN])>.0001f) count=1;
    }
    if (count!=b->count || type!=b->type || stereo!=b->stereo)
        memset(b->s,0,sizeof b->s);
    b->type=type; b->count=count; b->stereo=stereo;
    if (!count) return;
    double q=v[EB_Q], gain=v[EB_GAIN], w=warp(v[EB_FREQ],rate);
    if (type<=EQ_LPF) {
        int family=(int)lroundf(v[EB_FAMILY]), hp=type==EQ_HPF, mode=hp ? 0 : 1, out=0;
        if (n==1 && !hp && family!=1) {
            /* The AU uses a less aggressive Nyquist zero for its 6 dB LPF. */
            double pole=exp(-1.00054*2*atan(w));
            double b0=(1-pole)/1.222107;
            coefficients(&b->s[0],b0,.222107*b0,0,1,-pole,0);
            return;
        }
        if (n%2) {
            double p=eq_single[mode][family][n];
            cutoff(&b->s[out++],hp,hp ? w/p : w*p,0);
        }
        for (int j=0;j<n/2;++j) {
            const float *p=eq_pair[mode][family][n][j];
            double stage_q=p[1]*(family==0 && j==n/2-1 ? q : 1);
            cutoff(&b->s[out++],hp,hp ? w/p[0] : w*p[0],stage_q);
        }
    } else {
        double w2=w*w, A=pow(10,gain*.025);
        if (type==EQ_BELL) {
            double a=w/(q*bell_factor((int)lroundf(v[EB_BELL]),gain,q,fmin(.475,v[EB_FREQ]/rate)));
            coefficients(&b->s[0],1+a*A+w2,2*(w2-1),1-a*A+w2,
                         1+a/A+w2,2*(w2-1),1-a/A+w2);
        } else {
            double a=sqrt(A)*w/q;
            if (type==EQ_LOW_SHELF) {
                double g=A*w2, d=A+w2;
                coefficients(&b->s[0],A*(1+a+g),2*A*(g-1),A*(1-a+g),
                             d+a,2*(w2-A),d-a);
            } else {
                double g=A+w2, d=1+A*w2;
                coefficients(&b->s[0],A*(g+a),2*A*(w2-A),A*(g-a),
                             d+a,2*(A*w2-1),d-a);
            }
        }
    }
}

static float value(const struct eq *s,int i)
{
    const LADSPA_PortRangeHint *h=&hints[FIRST+i];
    const float *p=s->port[FIRST+i];
    return p&&isfinite(*p) ? fminf(h->UpperBound,fmaxf(h->LowerBound,*p))
                           : i<EQ_FIRST ? 0 : band_defaults[(i-EQ_FIRST)%EB_COUNT];
}

/* The editor overlay draws the curve of the instance being edited: the one whose controls changed
 * last. A newly loaded or re-activated instance only claims the display when nobody owns it, so
 * adding a second Para EQ no longer swaps the editor onto it (audit, 10 Nov 2026).
 * ponytail: an automated second instance still takes over while it moves; key by instance if needed.
 * published_seq is a seqlock: odd while the audio thread copies, so readers can retry. */
static float published[EQ_COUNT];
static unsigned long published_rate;
static unsigned published_seq;
static const struct eq *owner;

static void publish(const struct eq *s)
{
    owner=s;
    __atomic_add_fetch(&published_seq,1,__ATOMIC_ACQ_REL);
    memcpy(published,s->values,sizeof published);
    published_rate=s->rate;
    __atomic_add_fetch(&published_seq,1,__ATOMIC_RELEASE);
}

static void cleanup(LADSPA_Handle handle)
{
    if (owner==handle) owner=NULL;   /* the next instance to run takes the display */
    free(handle);
}

/* Settings of the last-changed instance, or NULL before any instance has run. */
const float *hs_eq2_values(unsigned *seq)
{
    unsigned n=__atomic_load_n(&published_seq,__ATOMIC_ACQUIRE);
    if (seq) *seq=n;
    return n ? published : NULL;
}

static double complex band_h(const struct band *b,double complex z1)
{
    double complex h=1;
    for (int i=0;i<b->count;++i) {
        const struct section *q=&b->s[i];
        h*=(q->b0+q->b1*z1+q->b2*z1*z1)/(1+q->a1*z1+q->a2*z1*z1);
    }
    return h;
}

/* Magnitude in dB at `points` log-spaced frequencies from 20 Hz to 20 kHz, computed from the
 * same biquads the audio runs. which: 0 = left/mid total, 1 = right/side total,
 * 2+i = band i on its own. Returns 0 when nothing has been published yet. */
int hs_eq2_response(int which,int points,float *db)
{
    if (points<2 || which<0 || which>=2+EQ_BANDS) return 0;
    float v[EQ_COUNT];  /* one consistent copy: retry while the audio thread is mid-publish */
    unsigned long rate=0;
    for (int tries=0;;++tries) {
        unsigned a=__atomic_load_n(&published_seq,__ATOMIC_ACQUIRE);
        if (!a) return 0;
        memcpy(v,published,sizeof v);
        rate=published_rate;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (!(a&1) && a==__atomic_load_n(&published_seq,__ATOMIC_RELAXED)) break;
        if (tries>1000) return 0;
    }
    static struct band bands[EQ_BANDS];  // ponytail: UI thread only
    for (int i=0;i<EQ_BANDS;++i) {
        memset(&bands[i],0,sizeof bands[i]);
        response_band(&bands[i],v+EQ_FIRST+i*EB_COUNT,rate);
    }
    int parallel=v[EQ_PARALLEL]>=.5f;
    for (int k=0;k<points;++k) {
        double hz=20*pow(1000,(double)k/(points-1));
        double complex z1=cexp(-I*2*M_PI*fmin(hz,.4999*rate)/rate);
        double complex h;
        if (which>=2) h=band_h(&bands[which-2],z1);
        else {
            double complex series=1, bells=0;
            for (int i=0;i<EQ_BANDS;++i) {
                const struct band *b=&bands[i];
                if (!b->count || b->stereo==(which==0 ? 1 : 0)) continue;
                double complex x=band_h(b,z1);
                if (parallel && b->type==EQ_BELL) bells+=x-1;
                else series*=x;
            }
            h=series*(1+bells);
        }
        double m=cabs(h);
        db[k]=(float)(20*log10(m>1e-6 ? m : 1e-6));  /* EQ shape only; in/out gain is on the Global page */
    }
    return 1;
}

static void configure(struct eq *s)
{
    float globals[EQ_FIRST];
    for (int i=0;i<EQ_FIRST;++i) globals[i]=value(s,i);
    int first_run=!s->ready;
    int globals_changed=!s->ready || memcmp(s->values,globals,sizeof globals);
    if (globals_changed) {
        memcpy(s->values,globals,sizeof globals);
        s->in_gain=pow(10,globals[EQ_INPUT]*.05);
        s->out_gain=pow(10,globals[EQ_OUTPUT]*.05);
        /* EQ_BYPASS stays a port so old projects load, but is ignored: the one Bypass is SampQOL's B3. */
        int mid_side=globals[EQ_MIDSIDE]>=.5f, parallel=globals[EQ_PARALLEL]>=.5f;
        if (mid_side!=s->mid_side || parallel!=s->parallel)
            for (int i=0;i<EQ_BANDS;++i) s->bands[i].count=0;
        if (mid_side!=s->mid_side || parallel!=s->parallel) s->ready=0;
        s->mid_side=mid_side; s->parallel=parallel;
    }
    int changed=globals_changed;
    s->active_count=0;
    for (int i=0;i<EQ_BANDS;++i) {
        int first=EQ_FIRST+i*EB_COUNT;
        float v[EB_COUNT];
        for (int k=0;k<EB_COUNT;++k) v[k]=value(s,first+k);
        struct band *b=&s->bands[i];
        if (!s->ready || memcmp(s->values+first,v,sizeof v)) {
            memcpy(s->values+first,v,sizeof v);
            response_band(b,v,s->rate);
            changed=1;
        }
        if (b->count) s->active[s->active_count++]=b;
    }
    s->ready=1;
    if ((changed && (!first_run || owner==s)) || !owner) publish(s);  /* !owner: owner was deleted, take over */
}

static inline double process(struct band *b,double x,int c)
{
    for (int i=0;i<b->count;++i) {
        struct section *s=&b->s[i];
        double y=s->b0*x+s->z1[c];
        s->z1[c]=s->b1*x-s->a1*y+s->z2[c];
        s->z2[c]=s->b2*x-s->a2*y;
        x=y;
    }
    return x;
}

static void run(LADSPA_Handle handle,unsigned long frames)
{
    struct eq *s=handle;
    float *il=s->port[IL], *ir=s->port[IR], *ol=s->port[OL], *orr=s->port[OR];
    if (!il||!ir||!ol||!orr) return;
    configure(s);
    if (!s->active_count && s->in_gain*s->out_gain==1) {
        for (unsigned long i=0;i<frames;++i) {
            float l=il[i],r=ir[i]; ol[i]=l; orr[i]=r;
            s->tap[s->tap_w++&(TAP-1)]=.5f*(l+r);
        }
        return;
    }
    for (unsigned long i=0;i<frames;++i) {
        double l=il[i]*s->in_gain,r=ir[i]*s->in_gain;
        if (s->mid_side) {
            double a=l; l=.5*(a+r); r=.5*(a-r);
        }
        if (s->parallel) {
            /* The AU keeps pass filters and shelves in series. Bell bands
               sum their individual changes against the filtered dry signal. */
            for (int j=0;j<s->active_count;++j) {
                struct band *b=s->active[j];
                if (b->type==EQ_BELL) continue;
                if (b->stereo!=1) l=process(b,l,0);
                if (b->stereo!=0) r=process(b,r,1);
            }
            double dl=l,dr=r;
            for (int j=0;j<s->active_count;++j) {
                struct band *b=s->active[j];
                if (b->type!=EQ_BELL) continue;
                if (b->stereo!=1) l+=process(b,dl,0)-dl;
                if (b->stereo!=0) r+=process(b,dr,1)-dr;
            }
        } else {
            for (int j=0;j<s->active_count;++j) {
                struct band *b=s->active[j];
                if (b->stereo!=1) l=process(b,l,0);
                if (b->stereo!=0) r=process(b,r,1);
            }
        }
        if (s->mid_side) {
            double a=l; l=a+r; r=a-r;
        }
        ol[i]=(float)(s->out_gain*l); orr[i]=(float)(s->out_gain*r);
        s->tap[s->tap_w++&(TAP-1)]=.5f*(ol[i]+orr[i]);
    }
    for (int j=0;j<s->active_count;++j) {
        struct band *b=s->active[j];
        for (int i=0;i<b->count;++i)
            for (int c=0;c<2;++c) {
                if (!(fabs(b->s[i].z1[c]) >= 1.e-30 && isfinite(b->s[i].z1[c]))) b->s[i].z1[c]=0;
                if (!(fabs(b->s[i].z2[c]) >= 1.e-30 && isfinite(b->s[i].z2[c]))) b->s[i].z2[c]=0;
            }
    }
}

#ifdef HS_MPC_LADSPA_RECALL   /* the MPC build only: it carries hs_registry.c; the CLAP build and the tools do not */
/* Editor spectrum: copies the newest n (<= TAP) output samples of an instance, oldest first, and
   returns how many samples it has written so far. Read from the UI thread while audio writes; a torn
   sample or two only shows as a flicker. */
int hs_ladspa_lock_live(const void *h);   /* hs_registry.c */
void hs_ladspa_unlock(void);
unsigned hs_eq2_tap(const void *handle,float *dst,unsigned n,unsigned long *rate)
{
    const struct eq *s=handle;
    if (rate) *rate=0;
    if (!hs_ladspa_lock_live(s)) return 0;   /* closed or freed: no fresh frame */
    if (n>TAP) n=TAP;
    unsigned w=__atomic_load_n(&s->tap_w,__ATOMIC_RELAXED);
    for (unsigned i=0;i<n;++i) dst[i]=s->tap[(w-n+i)&(TAP-1)];
    if (rate) *rate=s->rate;
    hs_ladspa_unlock();
    return w;
}
#endif

#define AI (LADSPA_PORT_INPUT|LADSPA_PORT_AUDIO)
#define AO (LADSPA_PORT_OUTPUT|LADSPA_PORT_AUDIO)
#define C (LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL)
#define BAND_TYPES(i) C,C,C,C,C,C,C,C,C,C,
static const LADSPA_PortDescriptor types[] = {AI,AI,AO,AO,C,C,C,C,C,BAND_LIST(BAND_TYPES)};
#undef BAND_TYPES
#define BAND_NAMES(i) "Band " #i " Frequency (Hz)","Band " #i " Gain (dB)","Band " #i " Q",     "Band " #i " Type","Band " #i " On","Band " #i " Bypass","Band " #i " Bell Shape",     "Band " #i " Filter Type","Band " #i " Rolloff","Band " #i " Stereo Position",
static const char *const names[] = {
    "Input L","Input R","Output L","Output R","Input Gain (dB)","Output Gain (dB)",
    "Mid/Side","Parallel","Bypass", BAND_LIST(BAND_NAMES)
};
#undef BAND_NAMES
#undef BAND_LIST
_Static_assert(sizeof names/sizeof *names==PORTS && sizeof types/sizeof *types==PORTS, "EQ2 port count");
LADSPA_Descriptor hs_eq2_descriptor = {
    .UniqueID=HS_EQ2_ID,.Label="hs_para_eq",.Name="HS Para EQ",.Maker="Henry Skillz",.Copyright="Prototype",
    .Properties=LADSPA_PROPERTY_HARD_RT_CAPABLE,.PortCount=PORTS,.PortDescriptors=types,
    .PortNames=names,.PortRangeHints=hints,.instantiate=instantiate,.connect_port=connect,
    .activate=activate,.run=run,.cleanup=cleanup
};
