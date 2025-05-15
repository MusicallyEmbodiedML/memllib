#ifndef MAXIPAF_HPP
#define MAXIPAF_HPP

constexpr size_t LOGTABSIZE  = 9;
constexpr size_t LOGTABSIZEINV  = 32 - LOGTABSIZE;
constexpr size_t TABSIZE = (1 << LOGTABSIZE);
constexpr size_t TABRANGE = 3;



typedef struct _tabpoint
{
    float p_y;
    float p_diff;
} t_tabpoint;


// t_tabpoint paf_gauss[TABSIZE];
// t_tabpoint paf_cauchy[TABSIZE];


typedef struct _linenv
{
    double l_current;
    double l_biginc;
    float l_1overn;
    float l_target;
    float l_msectodsptick;
    int l_ticks;
} t_linenv;


constexpr size_t UNITBIT32 = 1572864.f;		/* 3*2^19 -- bit 32 has value 1 */
constexpr float TABFRACSHIFT = (UNITBIT32/TABSIZE);


//little endian
#define HIOFFSET 1                                                              
#define LOWOFFSET 0                                                             

#include <sys/types.h>
#define int32 u_int32_t


#define LINENV_RUN(linenv, current, incr) 		\
    if (linenv.l_ticks > 0)				\
    {							\
    	current = linenv.l_current;			\
    	incr = linenv.l_biginc * linenv.l_1overn;	\
	linenv.l_ticks--;				\
	linenv.l_current += linenv.l_biginc;		\
    }							\
    else						\
    {							\
    	linenv.l_current = current = linenv.l_target;	\
	incr = 0;					\
    }


constexpr float PAFA1 = 4 * (3.14159265/2);
constexpr float PAFA3 = ((64 * (2.5 - 3.14159265)));
constexpr float PAFA5 ((1024 * ((3.14159265/2) - 1.5)));
    /* value of Cauchy distribution at TABRANGE */
constexpr float CAUCHYVAL = (1./ (1. + TABRANGE * TABRANGE));
    /* first derivative of Cauchy distribution at TABRANGE */
constexpr float CAUCHYSLOPE = ((-2. * TABRANGE) * CAUCHYVAL * CAUCHYVAL);
constexpr float ADDSQ = (- CAUCHYSLOPE / (2 * TABRANGE));
constexpr float HALFSINELIM  = (0.997 * TABRANGE);
constexpr float TABRANGERCPR = 1.f/TABRANGE;

class maxiPAFOperator {
public:


    void linenv_init(t_linenv &l)
    {
        l.l_current = l.l_biginc = 0;
        l.l_1overn = l.l_target = l.l_msectodsptick = 0;
        l.l_ticks = 0;
    }


    void init()
    {
        int i;
        const float CAUCHYFAKEAT3  =
            (CAUCHYVAL + ADDSQ * TABRANGE * TABRANGE);
        const float CAUCHYRESIZE = (1./ (1. - CAUCHYFAKEAT3));
        for (i = 0; i <= TABSIZE; i++)
        {
            float f = i * ((float)TABRANGE/(float)TABSIZE);
            float gauss = expf(-f * f);
            float cauchygenuine = 1. / (1. + f * f);
            float cauchyfake = cauchygenuine + ADDSQ * f * f;
            float cauchyrenorm = (cauchyfake - 1.) * CAUCHYRESIZE + 1.;
            if (i != TABSIZE)
            {
                paf_gauss[i].p_y = gauss;
                paf_cauchy[i].p_y = cauchyrenorm;
                /* post("%f", cauchyrenorm); */
            }
            if (i != 0)
            {
                paf_gauss[i-1].p_diff = gauss - paf_gauss[i-1].p_y;
                paf_cauchy[i-1].p_diff = cauchyrenorm - paf_cauchy[i-1].p_y;
            }
        }

        linenv_init(x_freqenv);
        linenv_init(x_cfenv);
        linenv_init(x_bwenv);
        linenv_init(x_ampenv);
        linenv_init(x_vibenv);
        linenv_init(x_vfrenv);
        linenv_init(x_shiftenv);
        x_freqenv.l_target = x_freqenv.l_current = 1.0;
        x_isr = 1.f/44100.f;
        x_held_freq = 1.f;
        x_held_intcar = 0.f;
        x_held_fraccar = 0.f;
        x_held_bwquotient = 0.f;
        x_phase = 0.;
        x_shiftphase = 0.;
        x_vibphase = 0.;
        x_triggerme = 0;
    }

    void linenv_setsr(t_linenv &l, const float sr, const int vecsize)
    {
        l.l_msectodsptick = sr / (1000.f * ((float)vecsize));
        l.l_1overn = 1.f/(float)vecsize;
    }

    void setsr(const float sr, const int vecsize)
    {
        x_isr = 1.f/sr;
        linenv_setsr(x_freqenv, sr, vecsize);
        linenv_setsr(x_cfenv, sr, vecsize);
        linenv_setsr(x_bwenv, sr, vecsize);
        linenv_setsr(x_ampenv, sr, vecsize);
        linenv_setsr(x_vibenv, sr, vecsize);
        linenv_setsr(x_vfrenv, sr, vecsize);
        linenv_setsr(x_shiftenv, sr, vecsize);
    }

    inline void linenv_set(t_linenv &l, const float target, const long timdel)
    {
        if (timdel > 0)
        {
            l.l_ticks = ((float)timdel) * l.l_msectodsptick;
            if (!l.l_ticks) l.l_ticks = 1;
            l.l_target = target;
            l.l_biginc = (l.l_target - l.l_current)/l.l_ticks;
        }
        else
        {
            l.l_ticks = 0;
            l.l_current = l.l_target = target;
            l.l_biginc = 0;
        }
    }

    inline void freq(float val, const int time)
    {
        if (val < 1.f) val = 1.f;
        if (val > 10000000.f) val = 1000000.f;
        linenv_set(x_freqenv, val, time);
    }
    
    inline void cf(const float val, const int time)
    {
        linenv_set(x_cfenv, val, time);
    }
    
    inline void bw(const float val, const int time)
    {
        linenv_set(x_bwenv, val, time);
    }
    
    inline void amp(const float val, const int time)
    {
        linenv_set(x_ampenv, val, time);
    }
    
    inline void vib(const float val, const int time)
    {
        linenv_set(x_vibenv, val, time);
    }
    
    inline void vfr(const float val, const int time)
    {
        linenv_set(x_vfrenv, val, time);
    }
    
    inline void shift(const float val, const int time)
    {
        linenv_set(x_shiftenv, val, time);
    }
    
    inline void phase(const float mainphase, const float shiftphase,
        const float vibphase)
    {
        x_phase = mainphase;
        x_shiftphase = shiftphase;
        x_vibphase = vibphase;
        x_triggerme = 1;
    }
    
    inline void play(float *out1, int n, float freqval, const float cfval, const float bwval, 
        const float vibval,
        const float vfrval,
        const bool x_cauchy=false)
    {
        // float freqinc;
        // float cfval, cfinc;
        // float bwval, bwinc;
        // float ampval, ampinc;
        // float vibval, vibinc;
        // float vfrval, vfrinc;
        float shiftval, shiftinc;
        float bwquotient, bwqincr;
        double  ub32 = UNITBIT32;
        double phase = x_phase + ub32;
        double phasehack;
        volatile double *phasehackp = &phasehack;
        double shiftphase = x_shiftphase + ub32;
        volatile int32 *hackptr = ((int32 *)(phasehackp)) + HIOFFSET, hackval;
        volatile int32 *lowptr = ((int32 *)(phasehackp)) + LOWOFFSET, lowbits;
        float held_freq = x_held_freq;
        float held_intcar = x_held_intcar;
        float held_fraccar = x_held_fraccar;
        float held_bwquotient = x_held_bwquotient;
        float sinvib, vibphase;
        t_tabpoint *paf_table = (x_cauchy ? paf_cauchy : paf_gauss);
        *phasehackp = ub32;
        hackval = *hackptr;
    
            /* fractional part of shift phase */
        *phasehackp = shiftphase;
        *hackptr = hackval;
        shiftphase = *phasehackp;
    
        /* propagate line envelopes */
        // LINENV_RUN(x_freqenv, freqval, freqinc);
        // LINENV_RUN(x_cfenv, cfval, cfinc);
        // LINENV_RUN(x_bwenv, bwval, bwinc);
        // LINENV_RUN(x_ampenv, ampval, ampinc);
        // LINENV_RUN(x_vibenv, vibval, vibinc);
        // LINENV_RUN(x_vfrenv, vfrval, vfrinc);
        LINENV_RUN(x_shiftenv, shiftval, shiftinc);
    
        /* fake line envelope for quotient of bw and frequency */
        bwquotient = bwval/freqval;
        bwqincr = (((float)(x_bwenv.l_current))/
            ((float)(x_freqenv.l_current)) - bwquotient) *
            x_freqenv.l_1overn;
    
        /* run the vibrato oscillator */
        
        *phasehackp = ub32 + (x_vibphase + n * x_isr * vfrval);
        *hackptr = hackval;
        vibphase = (x_vibphase = *phasehackp - ub32);
        if (vibphase > 0.5)
            sinvib = 1.0f - 16.0f * (0.75f-vibphase) * (0.75f - vibphase);
        else sinvib = -1.0f + 16.0f * (0.25f-vibphase) * (0.25f - vibphase);
        freqval = freqval * (1.0f + vibval * sinvib);
    
        shiftval *= x_isr;
        shiftinc *= x_isr;
        
        /* if phase or amplitude is zero, load in new params */
        // if (ampval == 0 || phase == ub32 || x_triggerme)
        if (phase == ub32 || x_triggerme)
        {
            float cf_over_freq = cfval/freqval;
            held_freq = freqval * x_isr;
            held_intcar = (float)((int)cf_over_freq);
            held_fraccar = cf_over_freq - held_intcar;
            held_bwquotient = bwquotient;
            x_triggerme = 0;
        }
        while (n--)
        {
            double newphase = phase + held_freq;
            double carphase1, carphase2, fracnewphase;
            float fphase, fcarphase1, fcarphase2, carrier;
            float g, g2, g3, cosine1, cosine2, halfsine;
            t_tabpoint *p;
                /* put new phase into 64-bit memory location.  Bash upper
                32 bits to get fractional part (plus "ub32").  */
    
            *phasehackp = newphase;
            *hackptr = hackval;
            newphase = *phasehackp;
            fracnewphase = newphase-ub32;
            fphase = 2.0f * ((float)(fracnewphase)) - 1.0f;
            if (newphase < phase)
            {
                float cf_over_freq = cfval/freqval;
                held_freq = freqval * x_isr;
                held_intcar = (float)((int)cf_over_freq);
                held_fraccar = cf_over_freq - held_intcar;
                held_bwquotient = bwquotient;
            }
            phase = newphase;
            *phasehackp = fracnewphase * held_intcar + shiftphase;
            *hackptr = hackval;
            carphase1 = *phasehackp;
            fcarphase1 = carphase1 - ub32;
            *phasehackp = carphase1 + fracnewphase;
            *hackptr = hackval;
            carphase2 = *phasehackp;
            fcarphase2 = carphase2 - ub32;
                
            shiftphase += shiftval;
        
            if (fcarphase1 > 0.5f)  g = fcarphase1 - 0.75f;
            else g = 0.25f - fcarphase1;
            g2 = g * g;
            g3 = g * g2;
            cosine1 = g * PAFA1 + g3 * PAFA3 + g2 * g3 * PAFA5;

            if (fcarphase2 > 0.5f)  g = fcarphase2 - 0.75f;
            else g = 0.25f - fcarphase2;
            g2 = g * g;
            g3 = g * g2;
            cosine2 = g * PAFA1 + g3 * PAFA3 + g2 * g3 * PAFA5;
        
            carrier = cosine1 + held_fraccar * (cosine2-cosine1);
    
            // ampval += ampinc;
            bwquotient += bwqincr;
    
            /* printf("bwquotient %f\n", bwquotient); */
    
            halfsine = held_bwquotient * (1.0f - fphase * fphase);
            if (halfsine >= HALFSINELIM)
                halfsine = HALFSINELIM;
    
    #if 0
            shape = halfsine * halfsine;
        mod = ampval * carrier *
            (1 - bluntval * shape) / (1 + (1 - bluntval) * shape);
    #endif
    #if 0
            shape = halfsine * halfsine;
        mod = ampval * carrier *
            exp(-shape);
    #endif
            halfsine *= TABRANGERCPR;
    
                /* Get table index for "halfsine".  Bash upper
                32 bits to get fractional part (plus "ub32").  Also grab
                fractional part as a fixed-point number to use as table
                address later. */
    
            *phasehackp = halfsine + ub32;
            lowbits = *lowptr;
    
                    /* now shift again so that the fractional table address
                appears in the low 32 bits, bash again, and extract this as
                a floating point number from 0 to 1. */
            *phasehackp = halfsine + TABFRACSHIFT;
            *hackptr = hackval;
            const float tabfrac = *phasehackp - ub32;
    
            p = paf_table + (lowbits >> LOGTABSIZEINV);
            // const float mod = ampval * carrier * (p->p_y + tabfrac * p->p_diff);
            const float mod = carrier * (p->p_y + tabfrac * p->p_diff);
            
            *out1++ = mod;
        }
        x_phase = phase - ub32;
        x_shiftphase = shiftphase - ub32;
        x_held_freq = held_freq;
        x_held_intcar = held_intcar;
        x_held_fraccar = held_fraccar;
        x_held_bwquotient = held_bwquotient;
    }
        


private:

    //todo: could be static and shared between operators
    t_tabpoint paf_gauss[TABSIZE];
    t_tabpoint paf_cauchy[TABSIZE];

    t_linenv x_freqenv;
    t_linenv x_cfenv;
    t_linenv x_bwenv;
    t_linenv x_ampenv;
    t_linenv x_vibenv;
    t_linenv x_vfrenv;
    t_linenv x_shiftenv;
    float x_isr;
    float x_held_freq;
    float x_held_intcar;
    float x_held_fraccar;
    float x_held_bwquotient;
    double x_phase;
    double x_shiftphase;
    double x_vibphase;
    int x_triggerme;
    // int x_cauchy;


};
    

#endif // MAXIPAF_HPP