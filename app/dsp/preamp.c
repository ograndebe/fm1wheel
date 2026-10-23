/* SPDX-License-Identifier: GPL-3.0-or-later
 * WheelBox preamp/overdrive, after setBfree's b_overdrive/overdrive.c
 * Copyright (C) 2003-2004 Fredrik Kilander, 2008-2018 Robin Gareus,
 * 2012 Will Panther. GPL-2.0-or-later.
 *
 * Port changes: one static instance instead of calloc, no MIDI/config
 * registration, no printf/fflush debug lines, float fabs only. The transfer
 * curve and the 4x oversampling filters are unchanged.
 */
/* Interpolation filter at digital frequency 0.25 */
/* Decimation filter at digital frequency 0.25 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "preamp.h"


/* Decimation filter definition */
static const float aaldef[33] = {
  -0.000000000000000001561469772894539464010892418355247457384394,
  -0.001887878403067588806152343750000000000000000000000000000000,
   0.000000000000000002244913588548488609774461677304202567029279,
   0.003862478304654359817504882812500000000000000000000000000000,
  -0.000000000000000004191197191582164338825335081484269039719948,
  -0.008242466486990451812744140625000000000000000000000000000000,
   0.000000000000000007104016524965676315133203400087325007916661,
   0.015947114676237106323242187500000000000000000000000000000000,
  -0.000000000000000010539920475899652678713341868999009420804214,
  -0.028676560148596763610839843750000000000000000000000000000000,
   0.000000000000000013975825254014241595321155209319385903654620,
   0.050718560814857482910156250000000000000000000000000000000000,
  -0.000000000000000016888644173807447295115186092218095836869907,
  -0.098015904426574707031250000000000000000000000000000000000000,
   0.000000000000000018834927363250816747652222060693816274579149,
   0.315941751003265380859375000000000000000000000000000000000000,
   0.500705778598785400390625000000000000000000000000000000000000,
   0.315941751003265380859375000000000000000000000000000000000000,
   0.000000000000000018834927363250816747652222060693816274579149,
  -0.098015904426574707031250000000000000000000000000000000000000,
  -0.000000000000000016888644173807447295115186092218095836869907,
   0.050718560814857482910156250000000000000000000000000000000000,
   0.000000000000000013975825254014241595321155209319385903654620,
  -0.028676560148596763610839843750000000000000000000000000000000,
  -0.000000000000000010539920475899652678713341868999009420804214,
   0.015947114676237106323242187500000000000000000000000000000000,
   0.000000000000000007104016524965676315133203400087325007916661,
  -0.008242466486990451812744140625000000000000000000000000000000,
  -0.000000000000000004191197191582164338825335081484269039719948,
   0.003862478304654359817504882812500000000000000000000000000000,
   0.000000000000000002244913588548488609774461677304202567029279,
  -0.001887878403067588806152343750000000000000000000000000000000,
  -0.000000000000000001561469772894539464010892418355247457384394
};


/* Weight count for wi[][] above. */
static const int wiLen[4] = {
  9,
  8,
  8,
  8
};

/* Interpolation filter definition */
static const float ipwdef[33] = {
  -0.000000000000000001561469772894539464010892418355247457384394,
  -0.001887878403067588806152343750000000000000000000000000000000,
   0.000000000000000002244913588548488609774461677304202567029279,
   0.003862478304654359817504882812500000000000000000000000000000,
  -0.000000000000000004191197191582164338825335081484269039719948,
  -0.008242466486990451812744140625000000000000000000000000000000,
   0.000000000000000007104016524965676315133203400087325007916661,
   0.015947114676237106323242187500000000000000000000000000000000,
  -0.000000000000000010539920475899652678713341868999009420804214,
  -0.028676560148596763610839843750000000000000000000000000000000,
   0.000000000000000013975825254014241595321155209319385903654620,
   0.050718560814857482910156250000000000000000000000000000000000,
  -0.000000000000000016888644173807447295115186092218095836869907,
  -0.098015904426574707031250000000000000000000000000000000000000,
   0.000000000000000018834927363250816747652222060693816274579149,
   0.315941751003265380859375000000000000000000000000000000000000,
   0.500705778598785400390625000000000000000000000000000000000000,
   0.315941751003265380859375000000000000000000000000000000000000,
   0.000000000000000018834927363250816747652222060693816274579149,
  -0.098015904426574707031250000000000000000000000000000000000000,
  -0.000000000000000016888644173807447295115186092218095836869907,
   0.050718560814857482910156250000000000000000000000000000000000,
   0.000000000000000013975825254014241595321155209319385903654620,
  -0.028676560148596763610839843750000000000000000000000000000000,
  -0.000000000000000010539920475899652678713341868999009420804214,
   0.015947114676237106323242187500000000000000000000000000000000,
   0.000000000000000007104016524965676315133203400087325007916661,
  -0.008242466486990451812744140625000000000000000000000000000000,
  -0.000000000000000004191197191582164338825335081484269039719948,
   0.003862478304654359817504882812500000000000000000000000000000,
   0.000000000000000002244913588548488609774461677304202567029279,
  -0.001887878403067588806152343750000000000000000000000000000000,
  -0.000000000000000001561469772894539464010892418355247457384394
};



struct b_preamp {
  /* Input history buffer */
  float xzb[64];
  /* Input history writer */
  float * xzp;
  /* Input history end sentinel */
  float * xzpe;
  
  /* Negative index access wrap sentinel */
  float * xzwp;
  /* Transfer-function output history buffer */
  float yzb[128];
  /* Transfer-function output writer */
  float * yzp;
  /* Transfer-function output history end sentinel */
  float * yzpe;
  /* Transfer-function output history wrap sentinel */
  float * yzwp;
  /* Zero-filled filter of interpolation length */
  float ipolZeros[33];
  /* Sample-specific runtime interpolation FIRs */
  float wi[4][9];
  /* Decimation filter runtime */
  float aal[33];
  /* Decimation filter end sentinel */
  float * aalEnd;
  size_t ipolFilterLength;
  size_t aalFilterLength;
  /* Zero-filled filter of anti-aliasing length */
  float aalZeros[33];
  /* Clean/overdrive switch */
  int isClean;
  float outputGain;
  
  /* Input gain */
  float inputGain;
  float sagZ;
  float sagFb;
  /* Variables for the inverted and biased transfer function */
  float biasBase;
  /* bias and norm are set in function cfg_biased() */
  float bias;
  float norm;
  /* ovt_biased : One sample memory */
  float adwZ;
  /* ovt_biased : Positive feedback */
  float adwFb;
  
  float adwZ1;
  float adwFb2;
  float adwGfb;
  float adwGfZ;
  float sagZgb;
};
/*  *** END STRUCT *** */





/* Remember to call this first with the filter definitions */
/* ipolDef is the interpolation filter definition */
/* aalDef is the anti-aliasing filter definition */
static void mixFilterWeights (void *pa, const float * ipolDef, const float * aalDef) {
  struct b_preamp *pp = (struct b_preamp *) pa;
  int i;
  float sum = 0.0;
  
  float mix[33];
  
  /* Copy the interpolation filter weights */
  
  for (i = 0; i < 33; i++) {
    mix[i] = ipolDef[i];
    sum += mix[i] < 0 ? -mix[i] : mix[i];
  }
  
  /* Normalize the copy */
  
  for (i = 0; i < 33; i++) {
    mix[i] /= sum;
  }
  
  /* Install in correct sequence in runtime array of weights */
  
  pp->wi[0][0] = mix[0];
  pp->wi[0][1] = mix[4];
  pp->wi[0][2] = mix[8];
  pp->wi[0][3] = mix[12];
  pp->wi[0][4] = mix[16];
  pp->wi[0][5] = mix[20];
  pp->wi[0][6] = mix[24];
  pp->wi[0][7] = mix[28];
  pp->wi[0][8] = mix[32];
  pp->wi[1][0] = mix[3];
  pp->wi[1][1] = mix[7];
  pp->wi[1][2] = mix[11];
  pp->wi[1][3] = mix[15];
  pp->wi[1][4] = mix[19];
  pp->wi[1][5] = mix[23];
  pp->wi[1][6] = mix[27];
  pp->wi[1][7] = mix[31];
  pp->wi[2][0] = mix[2];
  pp->wi[2][1] = mix[6];
  pp->wi[2][2] = mix[10];
  pp->wi[2][3] = mix[14];
  pp->wi[2][4] = mix[18];
  pp->wi[2][5] = mix[22];
  pp->wi[2][6] = mix[26];
  pp->wi[2][7] = mix[30];
  pp->wi[3][0] = mix[1];
  pp->wi[3][1] = mix[5];
  pp->wi[3][2] = mix[9];
  pp->wi[3][3] = mix[13];
  pp->wi[3][4] = mix[17];
  pp->wi[3][5] = mix[21];
  pp->wi[3][6] = mix[25];
  pp->wi[3][7] = mix[29];
  
  /* Copy the anti-aliasing filter definition */
  
  sum = 0.0;
  for (i = 0; i < 33; i++) {
    mix[i] = aalDef[i];
    sum += mix[i] < 0 ? -mix[i] : mix[i];
  }
  
  /* Normalize the weights to unit gain and install */
  
  for (i = 0; i < 33; i++) {
    pp->aal[i] = mix[i] / sum;
  }
} /* preFilterCompile */



float * overdrive (void *pa, const float * inBuf, float * outBuf, size_t buflen)
{
  struct b_preamp *pp = (struct b_preamp *) pa;
  const float * xp = inBuf;
  float * yp = outBuf;
  int i;
  size_t n;
  
  for (n = 0; n < buflen; n++) {
    float xin;
    float u = 0.0;
    float v;
    float y = 0.0;
    
    /* Place the next input sample in the input history. */
    if (++(pp->xzp) == pp->xzpe) {
      pp->xzp = pp->xzb;
    }
    
    xin = pp->inputGain * (*xp++);
    pp->sagZ = (pp->sagFb * pp->sagZ) + (xin < 0 ? -xin : xin);
    pp->bias = pp->biasBase - (pp->sagZgb * pp->sagZ);
    pp->norm = 1.0 - (1.0 / (1.0 + (pp->bias * pp->bias)));
    *(pp->xzp) = xin;
    
    /* Check the input history wrap sentinel */
    if (pp->xzwp <= pp->xzp) {
      for (i = 0; i < 4; i++) {
        
        /* wp is ptr to interpol. filter weights for this sample */
        float * wp = &(pp->wi[i][0]);
        
        /* wpe is FIR weight end sentinel */
        float * wpe = wp + wiLen[i];
        
        /* xr is ptr to samples in input history */
        float * xr = pp->xzp;
        
        /* Apply convolution */
        while (wp < wpe) {
          u += ((*wp++) * (*xr--));
        }
      }
    }
    else {
      /* Wrapping code */
      for (i = 0; i < 4; i++) {
        
        /* Interpolation weights for this sample */
        float * wp = &(pp->wi[i][0]);
        /* Weight end sentinel */
        float * wpe = wp + wiLen[i];
        /* Input history read pointer */
        float * xr = pp->xzp;
        
        while (pp->xzb <= xr) {
          u += ((*wp++) * (*xr--));
        }
        
        xr = &(pp->xzb[63]);
        
        while (wp < wpe) {
          u += ((*wp++) * (*xr--));
        }
      }
    }
    
    /* Apply transfer function */
    /* v = T (u); */
    /* Adaptive linear-non-linear transfer function */
    /* Global negative feedback */
    u -= (pp->adwGfb * pp->adwGfZ);
    {
      float temp = u - pp->adwZ;
      pp->adwZ = u + (pp->adwZ * pp->adwFb);
      u = temp;
    }
    if (u < 0.0) {
      float x2 = u - pp->bias;
      v = (1.0 / (1.0 + (x2 * x2))) - 1.0 + pp->norm;
    } else {
      float x2 = u + pp->bias;
      v = 1.0 - pp->norm - (1.0 / (1.0 + (x2 * x2)));
    }
    {
      float temp = v + (pp->adwFb2 * pp->adwZ1);
      v = temp - pp->adwZ1;
      pp->adwZ1 = temp;
    }
    /* Global negative feedback */
    pp->adwGfZ = v;
    
    /* Put transferred sample in output history. */
    if (++pp->yzp == pp->yzpe) {
      pp->yzp = pp->yzb;
    }
    *(pp->yzp) = v;
    
    /* Decimation */
    if (pp->yzwp <= pp->yzp) {
      /* No-wrap code */
      /* wp points to weights in the decimation FIR */
      float * wp = pp->aal;
      float * yr = pp->yzp;
      
      /* Convolve with decimation filter. */
      while (wp < pp->aalEnd) {
        y += ((*wp++) * (*yr--));
      }
    }
    else {
      /* Wrap code */
      float * wp = pp->aal;
      float * yr = pp->yzp;
      
      while (pp->yzb <= yr) {
        y += ((*wp++) * (*yr--));
      }
      
      yr = &(pp->yzb[127]);
      
      while (wp < pp->aalEnd) {
        y += ((*wp++) * (*yr--));
      }
    }
    
    *yp++ = pp->outputGain * y;
  }
  /* End of for-loop over input buffer */
  return outBuf;
} /* overdrive */



/* Adapter function */
float * preamp (void * pa,
                float * inBuf,
                float * outBuf,
                size_t bufLengthSamples) {
  struct b_preamp *pp = (struct b_preamp *) pa;
  if (pp->isClean) {
    memcpy(outBuf, inBuf, bufLengthSamples*sizeof(float));
  }
  else {
    overdrive (pa, inBuf, outBuf, bufLengthSamples);
  }
  
  return outBuf;
}


/* The transfer-curve setters from the original, without the debug prints. */
void cfg_biased(void *pa, float new_bias)
{
    struct b_preamp *p = (struct b_preamp *)pa;
    if (0.0f < new_bias) {
        p->biasBase = new_bias;
        p->bias = p->biasBase;
        p->norm = 1.0f - (1.0f / (1.0f + (p->bias * p->bias)));
    }
}

void fctl_biased(void *pa, float u)
{
    float v = 0.0f + ((0.7f - 0.0f) * (u * u));
    cfg_biased(pa, v);
}

void ctl_biased_fat(void *pa, unsigned char uc)
{
    struct b_preamp *p = (struct b_preamp *)pa;
    if (uc < 64) {
        if (uc < 32) {
            p->adwFb = 0.5821f;
            p->adwFb2 = 0.999f + ((0.5821f - 0.999f) * (((float)uc) / 31.0f));
        } else {
            p->adwFb = 0.5821f + ((0.999f - 0.5821f) * (((float)(uc - 32)) / 31.0f));
            p->adwFb2 = 0.5821f;
        }
    } else {
        p->adwFb = 0.999f;
        p->adwFb2 = 0.5821f + ((0.999f - 0.5821f) * (((float)(uc - 64)) / 63.0f));
    }
}

void fctl_biased_fat(void *pa, float f)
{
    ctl_biased_fat(pa, (unsigned char)(f * 127.0f));
}


static struct b_preamp pp;

void preamp_init(void)
{
    memset(&pp, 0, sizeof pp);
    pp.xzp = &pp.xzb[0];
    pp.xzpe = &pp.xzb[64];
    pp.xzwp = &pp.xzb[9];
    pp.yzp = &pp.yzb[0];
    pp.yzpe = &pp.yzb[128];
    pp.yzwp = &pp.yzb[33];
    pp.aalEnd = &pp.aal[33];
    pp.ipolFilterLength = 33;
    pp.aalFilterLength = 33;
    pp.isClean = 1;
    pp.outputGain = 0.8795f;
    pp.inputGain = 3.5675f;
    pp.sagZ = 0.0f;
    pp.sagFb = 0.991f;
    pp.biasBase = 0.5347f;
    pp.adwZ = 0.0f;
    pp.adwFb = 0.5821f;
    pp.adwZ1 = 0.0f;
    pp.adwFb2 = 0.999f;
    pp.adwGfb = -0.6214f;
    pp.adwGfZ = 0.0f;
    pp.sagZgb = 0.0094f;
    mixFilterWeights(&pp, ipwdef, aaldef);
    cfg_biased(&pp, 0.5347f);
}

void preamp_set_drive(float u)
{
    if (u <= 0.0f) {
        pp.isClean = 1;
    } else {
        pp.isClean = 0;
        fctl_biased(&pp, u > 1.0f ? 1.0f : u);
    }
}

void preamp_set_character(float u)
{
    fctl_biased_fat(&pp, u < 0 ? 0 : u > 1 ? 1 : u);
}

void preamp_run(float *buf, int n)
{
    preamp(&pp, buf, buf, (size_t)n);
}
