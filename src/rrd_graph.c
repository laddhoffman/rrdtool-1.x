/****************************************************************************
 * RRDtool 1.1.x  Copyright Tobias Oetiker, 1997 - 2002
 ****************************************************************************
 * rrd__graph.c  make creates ne rrds
 ****************************************************************************/

#if 0
#include "rrd_tool.h"
#endif

#include <sys/stat.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "rrd_graph.h"
#include "rrd_graph_helper.h"

/* some constant definitions */


#ifndef RRD_DEFAULT_FONT
#ifdef WIN32
#define RRD_DEFAULT_FONT "c:/winnt/fonts/COUR.TTF"
#else
#define RRD_DEFAULT_FONT "/usr/share/fonts/truetype/openoffice/ariosor.ttf" 
/* #define RRD_DEFAULT_FONT "/usr/share/fonts/truetype/Arial.ttf" */
#endif
#endif


text_prop_t text_prop[] = {   
     { 10.0, RRD_DEFAULT_FONT }, /* default */
     { 12.0, RRD_DEFAULT_FONT }, /* title */
     { 8.0,  RRD_DEFAULT_FONT },  /* axis */
     { 10.0, RRD_DEFAULT_FONT },  /* unit */
     { 10.0, RRD_DEFAULT_FONT }  /* legend */
};

xlab_t xlab[] = {
    {0,        TMT_SECOND,30, TMT_MINUTE,5,  TMT_MINUTE,5,         0,"%H:%M"},
    {2,        TMT_MINUTE,1,  TMT_MINUTE,5,  TMT_MINUTE,5,         0,"%H:%M"},
    {5,        TMT_MINUTE,2,  TMT_MINUTE,10, TMT_MINUTE,10,        0,"%H:%M"},
    {10,       TMT_MINUTE,5,  TMT_MINUTE,20, TMT_MINUTE,20,        0,"%H:%M"},
    {30,       TMT_MINUTE,10, TMT_HOUR,1,    TMT_HOUR,1,           0,"%H:%M"},
    {60,       TMT_MINUTE,30, TMT_HOUR,2,    TMT_HOUR,2,           0,"%H:%M"},
    {180,      TMT_HOUR,1,    TMT_HOUR,6,    TMT_HOUR,6,           0,"%H:%M"},
    /*{300,      TMT_HOUR,3,    TMT_HOUR,12,   TMT_HOUR,12,    12*3600,"%a %p"},  this looks silly*/
    {600,      TMT_HOUR,6,    TMT_DAY,1,     TMT_DAY,1,      24*3600,"%a"},
    {1800,     TMT_HOUR,12,   TMT_DAY,1,     TMT_DAY,2,      24*3600,"%a"},
    {3600,     TMT_DAY,1,     TMT_WEEK,1,     TMT_WEEK,1,    7*24*3600,"Week %V"},
    {3*3600,   TMT_WEEK,1,      TMT_MONTH,1,     TMT_WEEK,2,    7*24*3600,"Week %V"},
    {6*3600,   TMT_MONTH,1,   TMT_MONTH,1,   TMT_MONTH,1, 30*24*3600,"%b"},
    {48*3600,  TMT_MONTH,1,   TMT_MONTH,3,   TMT_MONTH,3, 30*24*3600,"%b"},
    {10*24*3600, TMT_YEAR,1,  TMT_YEAR,1,    TMT_YEAR,1, 365*24*3600,"%y"},
    {-1,TMT_MONTH,0,TMT_MONTH,0,TMT_MONTH,0,0,""}
};

/* sensible logarithmic y label intervals ...
   the first element of each row defines the possible starting points on the
   y axis ... the other specify the */

double yloglab[][12]= {{ 1e9, 1,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0 },
		       {  1e3, 1,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0 },
		       {  1e1, 1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
		       /* {  1e1, 1,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0 }, */
		       {  1e1, 1,  2.5,  5,  7.5,  0,  0,  0,  0,  0,  0,  0 },
		       {  1e1, 1,  2,  4,  6,  8,  0,  0,  0,  0,  0,  0 },
		       {  1e1, 1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0 },
		       {  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 }};

/* sensible y label intervals ...*/

ylab_t ylab[]= {
    {0.1, {1,2, 5,10}},
    {0.2, {1,5,10,20}},
    {0.5, {1,2, 4,10}},
    {1.0,   {1,2, 5,10}},
    {2.0,   {1,5,10,20}},
    {5.0,   {1,2, 4,10}},
    {10.0,  {1,2, 5,10}},
    {20.0,  {1,5,10,20}},
    {50.0,  {1,2, 4,10}},
    {100.0, {1,2, 5,10}},
    {200.0, {1,5,10,20}},
    {500.0, {1,2, 4,10}},
    {0.0,   {0,0,0,0}}};


gfx_color_t graph_col[] =   /* default colors */
{    0xFFFFFFFF,   /* canvas     */
     0xF0F0F0FF,   /* background */
     0xD0D0D0FF,   /* shade A    */
     0xA0A0A0FF,   /* shade B    */
     0x909090FF,   /* grid       */
     0xE05050FF,   /* major grid */
     0x000000FF,   /* font       */ 
     0x000000FF,   /* frame      */
     0xFF0000FF  /* arrow      */
};


/* #define DEBUG */

#ifdef DEBUG
# define DPRINT(x)    (void)(printf x, printf("\n"))
#else
# define DPRINT(x)
#endif


/* initialize with xtr(im,0); */
int
xtr(image_desc_t *im,time_t mytime){
    static double pixie;
    if (mytime==0){
	pixie = (double) im->xsize / (double)(im->end - im->start);
	return im->xorigin;
    }
    return (int)((double)im->xorigin 
		 + pixie * ( mytime - im->start ) );
}

/* translate data values into y coordinates */
int
ytr(image_desc_t *im, double value){
    static double pixie;
    double yval;
    if (isnan(value)){
      if(!im->logarithmic)
	pixie = (double) im->ysize / (im->maxval - im->minval);
      else 
	pixie = (double) im->ysize / (log10(im->maxval) - log10(im->minval));
      yval = im->yorigin;
    } else if(!im->logarithmic) {
      yval = im->yorigin - pixie * (value - im->minval) + 0.5;
    } else {
      if (value < im->minval) {
	yval = im->yorigin;
      } else {
	yval = im->yorigin - pixie * (log10(value) - log10(im->minval)) + 0.5;
      }
    }
    /* make sure we don't return anything too unreasonable. GD lib can
       get terribly slow when drawing lines outside its scope. This is 
       especially problematic in connection with the rigid option */
    if (! im->rigid) {
      return (int)yval;
    } else if ((int)yval > im->yorigin) {
      return im->yorigin+2;
    } else if ((int) yval < im->yorigin - im->ysize){
      return im->yorigin - im->ysize - 2;
    } else {
      return (int)yval;
    } 
}



/* conversion function for symbolic entry names */


#define conv_if(VV,VVV) \
   if (strcmp(#VV, string) == 0) return VVV ;

enum gf_en gf_conv(char *string){
    
    conv_if(PRINT,GF_PRINT)
    conv_if(GPRINT,GF_GPRINT)
    conv_if(COMMENT,GF_COMMENT)
    conv_if(HRULE,GF_HRULE)
    conv_if(VRULE,GF_VRULE)
    conv_if(LINE,GF_LINE)
    conv_if(AREA,GF_AREA)
    conv_if(STACK,GF_STACK)
    conv_if(TICK,GF_TICK)
    conv_if(DEF,GF_DEF)
    conv_if(CDEF,GF_CDEF)
    conv_if(VDEF,GF_VDEF)
    conv_if(PART,GF_PART)
    
    return (-1);
}

enum gfx_if_en if_conv(char *string){
    
    conv_if(PNG,IF_PNG)
    conv_if(SVG,IF_SVG)

    return (-1);
}

enum tmt_en tmt_conv(char *string){

    conv_if(SECOND,TMT_SECOND)
    conv_if(MINUTE,TMT_MINUTE)
    conv_if(HOUR,TMT_HOUR)
    conv_if(DAY,TMT_DAY)
    conv_if(WEEK,TMT_WEEK)
    conv_if(MONTH,TMT_MONTH)
    conv_if(YEAR,TMT_YEAR)
    return (-1);
}

enum grc_en grc_conv(char *string){

    conv_if(BACK,GRC_BACK)
    conv_if(CANVAS,GRC_CANVAS)
    conv_if(SHADEA,GRC_SHADEA)
    conv_if(SHADEB,GRC_SHADEB)
    conv_if(GRID,GRC_GRID)
    conv_if(MGRID,GRC_MGRID)
    conv_if(FONT,GRC_FONT)
    conv_if(FRAME,GRC_FRAME)
    conv_if(ARROW,GRC_ARROW)

    return -1;	
}

enum text_prop_en text_prop_conv(char *string){
      
    conv_if(DEFAULT,TEXT_PROP_DEFAULT)
    conv_if(TITLE,TEXT_PROP_TITLE)
    conv_if(AXIS,TEXT_PROP_AXIS)
    conv_if(UNIT,TEXT_PROP_UNIT)
    conv_if(LEGEND,TEXT_PROP_LEGEND)
    return -1;
}


#undef conv_if



int
im_free(image_desc_t *im)
{
    long i,ii;
    if (im == NULL) return 0;
    for(i=0;i<im->gdes_c;i++){
      if (im->gdes[i].data_first){
	/* careful here, because a single pointer can occur several times */
	  free (im->gdes[i].data);
	  if (im->gdes[i].ds_namv){
	      for (ii=0;ii<im->gdes[i].ds_cnt;ii++)
		  free(im->gdes[i].ds_namv[ii]);
	      free(im->gdes[i].ds_namv);
	  }
      }
      free (im->gdes[i].p_data);
      free (im->gdes[i].rpnp);
    }
    free(im->gdes);
    gfx_destroy(im->canvas);
    return 0;
}

/* find SI magnitude symbol for the given number*/
void
auto_scale(
	   image_desc_t *im,   /* image description */
	   double *value,
	   char **symb_ptr,
	   double *magfact
	   )
{
	
    char *symbol[] = {"a", /* 10e-18 Atto */
		      "f", /* 10e-15 Femto */
		      "p", /* 10e-12 Pico */
		      "n", /* 10e-9  Nano */
		      "u", /* 10e-6  Micro */
		      "m", /* 10e-3  Milli */
		      " ", /* Base */
		      "k", /* 10e3   Kilo */
		      "M", /* 10e6   Mega */
		      "G", /* 10e9   Giga */
		      "T", /* 10e12  Tera */
		      "P", /* 10e15  Peta */
		      "E"};/* 10e18  Exa */

    int symbcenter = 6;
    int sindex;  

    if (*value == 0.0 || isnan(*value) ) {
	sindex = 0;
	*magfact = 1.0;
    } else {
	sindex = floor(log(fabs(*value))/log((double)im->base)); 
	*magfact = pow((double)im->base, (double)sindex);
	(*value) /= (*magfact);
    }
    if ( sindex <= symbcenter && sindex >= -symbcenter) {
	(*symb_ptr) = symbol[sindex+symbcenter];
    }
    else {
	(*symb_ptr) = "?";
    }
}


/* find SI magnitude symbol for the numbers on the y-axis*/
void 
si_unit(
    image_desc_t *im   /* image description */
)
{

    char symbol[] = {'a', /* 10e-18 Atto */ 
		     'f', /* 10e-15 Femto */
		     'p', /* 10e-12 Pico */
		     'n', /* 10e-9  Nano */
		     'u', /* 10e-6  Micro */
		     'm', /* 10e-3  Milli */
		     ' ', /* Base */
		     'k', /* 10e3   Kilo */
		     'M', /* 10e6   Mega */
		     'G', /* 10e9   Giga */
		     'T', /* 10e12  Tera */
		     'P', /* 10e15  Peta */
		     'E'};/* 10e18  Exa */

    int   symbcenter = 6;
    double digits;  
    
    if (im->unitsexponent != 9999) {
	/* unitsexponent = 9, 6, 3, 0, -3, -6, -9, etc */
        digits = floor(im->unitsexponent / 3);
    } else {
        digits = floor( log( max( fabs(im->minval),fabs(im->maxval)))/log((double)im->base)); 
    }
    im->magfact = pow((double)im->base , digits);

#ifdef DEBUG
    printf("digits %6.3f  im->magfact %6.3f\n",digits,im->magfact);
#endif

    if ( ((digits+symbcenter) < sizeof(symbol)) &&
		    ((digits+symbcenter) >= 0) )
        im->symbol = symbol[(int)digits+symbcenter];
    else
	im->symbol = ' ';
 }

/*  move min and max values around to become sensible */

void 
expand_range(image_desc_t *im)
{
    double sensiblevalues[] ={1000.0,900.0,800.0,750.0,700.0,
			      600.0,500.0,400.0,300.0,250.0,
			      200.0,125.0,100.0,90.0,80.0,
			      75.0,70.0,60.0,50.0,40.0,30.0,
			      25.0,20.0,10.0,9.0,8.0,
			      7.0,6.0,5.0,4.0,3.5,3.0,
			      2.5,2.0,1.8,1.5,1.2,1.0,
			      0.8,0.7,0.6,0.5,0.4,0.3,0.2,0.1,0.0,-1};
    
    double scaled_min,scaled_max;  
    double adj;
    int i;
    

    
#ifdef DEBUG
    printf("Min: %6.2f Max: %6.2f MagFactor: %6.2f\n",
	   im->minval,im->maxval,im->magfact);
#endif

    if (isnan(im->ygridstep)){
	if(im->extra_flags & ALTAUTOSCALE) {
	    /* measure the amplitude of the function. Make sure that
	       graph boundaries are slightly higher then max/min vals
	       so we can see amplitude on the graph */
	      double delt, fact;

	      delt = im->maxval - im->minval;
	      adj = delt * 0.1;
	      fact = 2.0 * pow(10.0,
		    floor(log10(max(fabs(im->minval), fabs(im->maxval)))) - 2);
	      if (delt < fact) {
		adj = (fact - delt) * 0.55;
#ifdef DEBUG
	      printf("Min: %6.2f Max: %6.2f delt: %6.2f fact: %6.2f adj: %6.2f\n", im->minval, im->maxval, delt, fact, adj);
#endif
	      }
	      im->minval -= adj;
	      im->maxval += adj;
	}
	else if(im->extra_flags & ALTAUTOSCALE_MAX) {
	    /* measure the amplitude of the function. Make sure that
	       graph boundaries are slightly higher than max vals
	       so we can see amplitude on the graph */
	      adj = (im->maxval - im->minval) * 0.1;
	      im->maxval += adj;
	}
	else {
	    scaled_min = im->minval / im->magfact;
	    scaled_max = im->maxval / im->magfact;
	    
	    for (i=1; sensiblevalues[i] > 0; i++){
		if (sensiblevalues[i-1]>=scaled_min &&
		    sensiblevalues[i]<=scaled_min)	
		    im->minval = sensiblevalues[i]*(im->magfact);
		
		if (-sensiblevalues[i-1]<=scaled_min &&
		-sensiblevalues[i]>=scaled_min)
		    im->minval = -sensiblevalues[i-1]*(im->magfact);
		
		if (sensiblevalues[i-1] >= scaled_max &&
		    sensiblevalues[i] <= scaled_max)
		    im->maxval = sensiblevalues[i-1]*(im->magfact);
		
		if (-sensiblevalues[i-1]<=scaled_max &&
		    -sensiblevalues[i] >=scaled_max)
		    im->maxval = -sensiblevalues[i]*(im->magfact);
	    }
	}
    } else {
	/* adjust min and max to the grid definition if there is one */
	im->minval = (double)im->ylabfact * im->ygridstep * 
	    floor(im->minval / ((double)im->ylabfact * im->ygridstep));
	im->maxval = (double)im->ylabfact * im->ygridstep * 
	    ceil(im->maxval /( (double)im->ylabfact * im->ygridstep));
    }
    
#ifdef DEBUG
    fprintf(stderr,"SCALED Min: %6.2f Max: %6.2f Factor: %6.2f\n",
	   im->minval,im->maxval,im->magfact);
#endif
}

    
/* reduce data reimplementation by Alex */

void
reduce_data(
    enum cf_en     cf,         /* which consolidation function ?*/
    unsigned long  cur_step,   /* step the data currently is in */
    time_t         *start,     /* start, end and step as requested ... */
    time_t         *end,       /* ... by the application will be   ... */
    unsigned long  *step,      /* ... adjusted to represent reality    */
    unsigned long  *ds_cnt,    /* number of data sources in file */
    rrd_value_t    **data)     /* two dimensional array containing the data */
{
    int i,reduce_factor = ceil((double)(*step) / (double)cur_step);
    unsigned long col,dst_row,row_cnt,start_offset,end_offset,skiprows=0;
    rrd_value_t    *srcptr,*dstptr;

    (*step) = cur_step*reduce_factor; /* set new step size for reduced data */
    dstptr = *data;
    srcptr = *data;
    row_cnt = ((*end)-(*start))/cur_step;

#ifdef DEBUG
#define DEBUG_REDUCE
#endif
#ifdef DEBUG_REDUCE
printf("Reducing %lu rows with factor %i time %lu to %lu, step %lu\n",
			row_cnt,reduce_factor,*start,*end,cur_step);
for (col=0;col<row_cnt;col++) {
    printf("time %10lu: ",*start+(col+1)*cur_step);
    for (i=0;i<*ds_cnt;i++)
	printf(" %8.2e",srcptr[*ds_cnt*col+i]);
    printf("\n");
}
#endif

    /* We have to combine [reduce_factor] rows of the source
    ** into one row for the destination.  Doing this we also
    ** need to take care to combine the correct rows.  First
    ** alter the start and end time so that they are multiples
    ** of the new step time.  We cannot reduce the amount of
    ** time so we have to move the end towards the future and
    ** the start towards the past.
    */
    end_offset = (*end) % (*step);
    start_offset = (*start) % (*step);

    /* If there is a start offset (which cannot be more than
    ** one destination row), skip the appropriate number of
    ** source rows and one destination row.  The appropriate
    ** number is what we do know (start_offset/cur_step) of
    ** the new interval (*step/cur_step aka reduce_factor).
    */
#ifdef DEBUG_REDUCE
printf("start_offset: %lu  end_offset: %lu\n",start_offset,end_offset);
printf("row_cnt before:  %lu\n",row_cnt);
#endif
    if (start_offset) {
	(*start) = (*start)-start_offset;
	skiprows=reduce_factor-start_offset/cur_step;
	srcptr+=skiprows* *ds_cnt;
        for (col=0;col<(*ds_cnt);col++) *dstptr++ = DNAN;
	row_cnt-=skiprows;
    }
#ifdef DEBUG_REDUCE
printf("row_cnt between: %lu\n",row_cnt);
#endif

    /* At the end we have some rows that are not going to be
    ** used, the amount is end_offset/cur_step
    */
    if (end_offset) {
	(*end) = (*end)-end_offset+(*step);
	skiprows = end_offset/cur_step;
	row_cnt-=skiprows;
    }
#ifdef DEBUG_REDUCE
printf("row_cnt after:   %lu\n",row_cnt);
#endif

/* Sanity check: row_cnt should be multiple of reduce_factor */
/* if this gets triggered, something is REALLY WRONG ... we die immediately */

    if (row_cnt%reduce_factor) {
	printf("SANITY CHECK: %lu rows cannot be reduced by %i \n",
				row_cnt,reduce_factor);
	printf("BUG in reduce_data()\n");
	exit(1);
    }

    /* Now combine reduce_factor intervals at a time
    ** into one interval for the destination.
    */

    for (dst_row=0;row_cnt>=reduce_factor;dst_row++) {
	for (col=0;col<(*ds_cnt);col++) {
	    rrd_value_t newval=DNAN;
	    unsigned long validval=0;

	    for (i=0;i<reduce_factor;i++) {
		if (isnan(srcptr[i*(*ds_cnt)+col])) {
		    continue;
		}
		validval++;
		if (isnan(newval)) newval = srcptr[i*(*ds_cnt)+col];
		else {
		    switch (cf) {
			case CF_HWPREDICT:
			case CF_DEVSEASONAL:
			case CF_DEVPREDICT:
			case CF_SEASONAL:
			case CF_AVERAGE:
			    newval += srcptr[i*(*ds_cnt)+col];
			    break;
			case CF_MINIMUM:
			    newval = min (newval,srcptr[i*(*ds_cnt)+col]);
			    break;
			case CF_FAILURES: 
			/* an interval contains a failure if any subintervals contained a failure */
			case CF_MAXIMUM:
			    newval = max (newval,srcptr[i*(*ds_cnt)+col]);
			    break;
			case CF_LAST:
			    newval = srcptr[i*(*ds_cnt)+col];
			    break;
		    }
		}
	    }
	    if (validval == 0){newval = DNAN;} else{
		switch (cf) {
		    case CF_HWPREDICT:
    	    case CF_DEVSEASONAL:
		    case CF_DEVPREDICT:
		    case CF_SEASONAL:
		    case CF_AVERAGE:                
		       newval /= validval;
			break;
		    case CF_MINIMUM:
		    case CF_FAILURES:
 		    case CF_MAXIMUM:
		    case CF_LAST:
			break;
		}
	    }
	    *dstptr++=newval;
	}
	srcptr+=(*ds_cnt)*reduce_factor;
	row_cnt-=reduce_factor;
    }
    /* If we had to alter the endtime, we didn't have enough
    ** source rows to fill the last row. Fill it with NaN.
    */
    if (end_offset) for (col=0;col<(*ds_cnt);col++) *dstptr++ = DNAN;
#ifdef DEBUG_REDUCE
    row_cnt = ((*end)-(*start))/ *step;
    srcptr = *data;
    printf("Done reducing. Currently %lu rows, time %lu to %lu, step %lu\n",
				row_cnt,*start,*end,*step);
for (col=0;col<row_cnt;col++) {
    printf("time %10lu: ",*start+(col+1)*(*step));
    for (i=0;i<*ds_cnt;i++)
	printf(" %8.2e",srcptr[*ds_cnt*col+i]);
    printf("\n");
}
#endif
}


/* get the data required for the graphs from the 
   relevant rrds ... */

int
data_fetch( image_desc_t *im )
{
    int       i,ii;
    int skip;
    /* pull the data from the log files ... */
    for (i=0;i<im->gdes_c;i++){
	/* only GF_DEF elements fetch data */
	if (im->gdes[i].gf != GF_DEF) 
	    continue;

	skip=0;
	/* do we have it already ?*/
	for (ii=0;ii<i;ii++){
	    if (im->gdes[ii].gf != GF_DEF) 
		continue;
	    if((strcmp(im->gdes[i].rrd,im->gdes[ii].rrd) == 0)
		&& (im->gdes[i].cf == im->gdes[ii].cf)){
		/* OK the data it is here already ... 
		 * we just copy the header portion */
		im->gdes[i].start = im->gdes[ii].start;
		im->gdes[i].end = im->gdes[ii].end;
		im->gdes[i].step = im->gdes[ii].step;
		im->gdes[i].ds_cnt = im->gdes[ii].ds_cnt;
		im->gdes[i].ds_namv = im->gdes[ii].ds_namv;		
		im->gdes[i].data = im->gdes[ii].data;
		im->gdes[i].data_first = 0;
		skip=1;
	    }
	    if (skip) 
		break;
	}
	if (! skip) {
	    unsigned long  ft_step = im->gdes[i].step ;
	    
	    if((rrd_fetch_fn(im->gdes[i].rrd,
			     im->gdes[i].cf,
			     &im->gdes[i].start,
			     &im->gdes[i].end,
			     &ft_step,
			     &im->gdes[i].ds_cnt,
			     &im->gdes[i].ds_namv,
			     &im->gdes[i].data)) == -1){		
		return -1;
	    }
	    im->gdes[i].data_first = 1;	    
	
	    if (ft_step < im->gdes[i].step) {
		reduce_data(im->gdes[i].cf,
			    ft_step,
			    &im->gdes[i].start,
			    &im->gdes[i].end,
			    &im->gdes[i].step,
			    &im->gdes[i].ds_cnt,
			    &im->gdes[i].data);
	    } else {
		im->gdes[i].step = ft_step;
	    }
	}
	
        /* lets see if the required data source is realy there */
	for(ii=0;ii<im->gdes[i].ds_cnt;ii++){
	    if(strcmp(im->gdes[i].ds_namv[ii],im->gdes[i].ds_nam) == 0){
		im->gdes[i].ds=ii; }
	}
	if (im->gdes[i].ds== -1){
	    rrd_set_error("No DS called '%s' in '%s'",
			  im->gdes[i].ds_nam,im->gdes[i].rrd);
	    return -1; 
	}
	
    }
    return 0;
}

/* evaluate the expressions in the CDEF functions */

/*************************************************************
 * CDEF stuff 
 *************************************************************/

long
find_var_wrapper(void *arg1, char *key)
{
   return find_var((image_desc_t *) arg1, key);
}

/* find gdes containing var*/
long
find_var(image_desc_t *im, char *key){
    long ii;
    for(ii=0;ii<im->gdes_c-1;ii++){
	if((im->gdes[ii].gf == GF_DEF 
	    || im->gdes[ii].gf == GF_VDEF
	    || im->gdes[ii].gf == GF_CDEF) 
	   && (strcmp(im->gdes[ii].vname,key) == 0)){
	    return ii; 
	}	   
    }	    	    
    return -1;
}

/* find the largest common denominator for all the numbers
   in the 0 terminated num array */
long
lcd(long *num){
    long rest;
    int i;
    for (i=0;num[i+1]!=0;i++){
	do { 
	    rest=num[i] % num[i+1];
	    num[i]=num[i+1]; num[i+1]=rest;
	} while (rest!=0);
	num[i+1] = num[i];
    }
/*    return i==0?num[i]:num[i-1]; */
      return num[i];
}

/* run the rpn calculator on all the VDEF and CDEF arguments */
int
data_calc( image_desc_t *im){

    int       gdi;
    int       dataidx;
    long      *steparray, rpi;
    int       stepcnt;
    time_t    now;
    rpnstack_t rpnstack;

    rpnstack_init(&rpnstack);

    for (gdi=0;gdi<im->gdes_c;gdi++){
	/* Look for GF_VDEF and GF_CDEF in the same loop,
	 * so CDEFs can use VDEFs and vice versa
	 */
	switch (im->gdes[gdi].gf) {
	    case GF_VDEF:
		/* A VDEF has no DS.  This also signals other parts
		 * of rrdtool that this is a VDEF value, not a CDEF.
		 */
		im->gdes[gdi].ds_cnt = 0;
		if (vdef_calc(im,gdi)) {
		    rrd_set_error("Error processing VDEF '%s'"
			,im->gdes[gdi].vname
			);
		    rpnstack_free(&rpnstack);
		    return -1;
		}
		break;
	    case GF_CDEF:
		im->gdes[gdi].ds_cnt = 1;
		im->gdes[gdi].ds = 0;
		im->gdes[gdi].data_first = 1;
		im->gdes[gdi].start = 0;
		im->gdes[gdi].end = 0;
		steparray=NULL;
		stepcnt = 0;
		dataidx=-1;

		/* Find the variables in the expression.
		 * - VDEF variables are substituted by their values
		 *   and the opcode is changed into OP_NUMBER.
		 * - CDEF variables are analized for their step size,
		 *   the lowest common denominator of all the step
		 *   sizes of the data sources involved is calculated
		 *   and the resulting number is the step size for the
		 *   resulting data source.
		 */
		for(rpi=0;im->gdes[gdi].rpnp[rpi].op != OP_END;rpi++){
		    if(im->gdes[gdi].rpnp[rpi].op == OP_VARIABLE){
			long ptr = im->gdes[gdi].rpnp[rpi].ptr;
			if (im->gdes[ptr].ds_cnt == 0) {
#if 0
printf("DEBUG: inside CDEF '%s' processing VDEF '%s'\n",
	im->gdes[gdi].vname,
	im->gdes[ptr].vname);
printf("DEBUG: value from vdef is %f\n",im->gdes[ptr].vf.val);
#endif
			    im->gdes[gdi].rpnp[rpi].val = im->gdes[ptr].vf.val;
			    im->gdes[gdi].rpnp[rpi].op  = OP_NUMBER;
			} else {
			    if ((steparray = rrd_realloc(steparray, (++stepcnt+1)*sizeof(*steparray)))==NULL){
				rrd_set_error("realloc steparray");
				rpnstack_free(&rpnstack);
				return -1;
			    };

			    steparray[stepcnt-1] = im->gdes[ptr].step;

			    /* adjust start and end of cdef (gdi) so
			     * that it runs from the latest start point
			     * to the earliest endpoint of any of the
			     * rras involved (ptr)
			     */
			    if(im->gdes[gdi].start < im->gdes[ptr].start)
				im->gdes[gdi].start = im->gdes[ptr].start;

			    if(im->gdes[gdi].end == 0 ||
					im->gdes[gdi].end > im->gdes[ptr].end)
				im->gdes[gdi].end = im->gdes[ptr].end;
		
			    /* store pointer to the first element of
			     * the rra providing data for variable,
			     * further save step size and data source
			     * count of this rra
			     */ 
                            im->gdes[gdi].rpnp[rpi].data =  im->gdes[ptr].data + im->gdes[ptr].ds;
			    im->gdes[gdi].rpnp[rpi].step = im->gdes[ptr].step;
			    im->gdes[gdi].rpnp[rpi].ds_cnt = im->gdes[ptr].ds_cnt;

			    /* backoff the *.data ptr; this is done so
			     * rpncalc() function doesn't have to treat
			     * the first case differently
			     */
			} /* if ds_cnt != 0 */
		    } /* if OP_VARIABLE */
		} /* loop through all rpi */

                /* move the data pointers to the correct period */
                for(rpi=0;im->gdes[gdi].rpnp[rpi].op != OP_END;rpi++){
                    if(im->gdes[gdi].rpnp[rpi].op == OP_VARIABLE){
                        long ptr = im->gdes[gdi].rpnp[rpi].ptr;
                        if(im->gdes[gdi].start > im->gdes[ptr].start) {
                            im->gdes[gdi].rpnp[rpi].data += im->gdes[gdi].rpnp[rpi].ds_cnt;
                        }
                     }
                }
        

		if(steparray == NULL){
		    rrd_set_error("rpn expressions without DEF"
				" or CDEF variables are not supported");
		    rpnstack_free(&rpnstack);
		    return -1;    
		}
		steparray[stepcnt]=0;
		/* Now find the resulting step.  All steps in all
		 * used RRAs have to be visited
		 */
		im->gdes[gdi].step = lcd(steparray);
		free(steparray);
		if((im->gdes[gdi].data = malloc((
				(im->gdes[gdi].end-im->gdes[gdi].start) 
				    / im->gdes[gdi].step)
				    * sizeof(double)))==NULL){
		    rrd_set_error("malloc im->gdes[gdi].data");
		    rpnstack_free(&rpnstack);
		    return -1;
		}
	
		/* Step through the new cdef results array and
		 * calculate the values
		 */
		for (now = im->gdes[gdi].start + im->gdes[gdi].step;
				now<=im->gdes[gdi].end;
				now += im->gdes[gdi].step)
		{
		    rpnp_t  *rpnp = im -> gdes[gdi].rpnp;

		    /* 3rd arg of rpn_calc is for OP_VARIABLE lookups;
		     * in this case we are advancing by timesteps;
		     * we use the fact that time_t is a synonym for long
		     */
		    if (rpn_calc(rpnp,&rpnstack,(long) now, 
				im->gdes[gdi].data,++dataidx) == -1) {
			/* rpn_calc sets the error string */
			rpnstack_free(&rpnstack); 
			return -1;
		    } 
		} /* enumerate over time steps within a CDEF */
		break;
	    default:
		continue;
	}
    } /* enumerate over CDEFs */
    rpnstack_free(&rpnstack);
    return 0;
}

/* massage data so, that we get one value for each x coordinate in the graph */
int
data_proc( image_desc_t *im ){
    long i,ii;
    double pixstep = (double)(im->end-im->start)
	/(double)im->xsize; /* how much time 
			       passes in one pixel */
    double paintval;
    double minval=DNAN,maxval=DNAN;
    
    unsigned long gr_time;    

    /* memory for the processed data */
    for(i=0;i<im->gdes_c;i++){
      if((im->gdes[i].gf==GF_LINE) ||
	 (im->gdes[i].gf==GF_AREA) ||
	 (im->gdes[i].gf==GF_TICK) ||
	 (im->gdes[i].gf==GF_STACK)){
	if((im->gdes[i].p_data = malloc((im->xsize +1)
					* sizeof(rrd_value_t)))==NULL){
	  rrd_set_error("malloc data_proc");
	  return -1;
	}
      }
    }
    
    for(i=0;i<im->xsize;i++){
	long vidx;
	gr_time = im->start+pixstep*i; /* time of the 
					  current step */
	paintval=0.0;
	
	for(ii=0;ii<im->gdes_c;ii++){
	  double value;
	    switch(im->gdes[ii].gf){
	    case GF_LINE:
	    case GF_AREA:
		case GF_TICK:
		paintval = 0.0;
	    case GF_STACK:
		vidx = im->gdes[ii].vidx;

		value =
		    im->gdes[vidx].data[
			((unsigned long)floor(
	(double)(gr_time-im->gdes[vidx].start) / im->gdes[vidx].step
					     )
                        )	*im->gdes[vidx].ds_cnt
				+im->gdes[vidx].ds];

		if (! isnan(value)) {
		  paintval += value;
		  im->gdes[ii].p_data[i] = paintval;
		  /* GF_TICK: the data values are not relevant for min and max */
		  if (finite(paintval) && im->gdes[ii].gf != GF_TICK ){
  		   if (isnan(minval) || paintval <  minval)
		     minval = paintval;
		   if (isnan(maxval) || paintval >  maxval)
		     maxval = paintval;
		  }
		} else {
		  im->gdes[ii].p_data[i] = DNAN;
		}
		break;
	    case GF_PRINT:
	    case GF_GPRINT:
	    case GF_COMMENT:
	    case GF_HRULE:
	    case GF_VRULE:
	    case GF_DEF:	       
	    case GF_CDEF:
	    case GF_VDEF:
	    case GF_PART:
		break;
	    }
	}
    }

    /* if min or max have not been asigned a value this is because
       there was no data in the graph ... this is not good ...
       lets set these to dummy values then ... */

    if (isnan(minval)) minval = 0.0;
    if (isnan(maxval)) maxval = 1.0;
    
    /* adjust min and max values */
    if (isnan(im->minval) 
	|| ((!im->logarithmic && !im->rigid) /* don't adjust low-end with log scale */
	    && im->minval > minval))
	im->minval = minval;
    if (isnan(im->maxval) 
	|| (!im->rigid 
	    && im->maxval < maxval)){
	if (im->logarithmic)
	    im->maxval = maxval * 1.1;
	else
	    im->maxval = maxval;
    }
    /* make sure min and max are not equal */
    if (im->minval == im->maxval) {
      im->maxval *= 1.01; 
      if (! im->logarithmic) {
	im->minval *= 0.99;
      }
      
      /* make sure min and max are not both zero */
      if (im->maxval == 0.0) {
	    im->maxval = 1.0;
      }
        
    }
    return 0;
}



/* identify the point where the first gridline, label ... gets placed */

time_t
find_first_time(
    time_t   start, /* what is the initial time */
    enum tmt_en baseint,  /* what is the basic interval */
    long     basestep /* how many if these do we jump a time */
    )
{
    struct tm tm;
    tm = *localtime(&start);
    switch(baseint){
    case TMT_SECOND:
	tm.tm_sec -= tm.tm_sec % basestep; break;
    case TMT_MINUTE: 
	tm.tm_sec=0;
	tm.tm_min -= tm.tm_min % basestep; 
	break;
    case TMT_HOUR:
	tm.tm_sec=0;
	tm.tm_min = 0;
	tm.tm_hour -= tm.tm_hour % basestep; break;
    case TMT_DAY:
	/* we do NOT look at the basestep for this ... */
	tm.tm_sec=0;
	tm.tm_min = 0;
	tm.tm_hour = 0; break;
    case TMT_WEEK:
	/* we do NOT look at the basestep for this ... */
	tm.tm_sec=0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday -= tm.tm_wday -1;	/* -1 because we want the monday */
	if (tm.tm_wday==0) tm.tm_mday -= 7; /* we want the *previous* monday */
	break;
    case TMT_MONTH:
	tm.tm_sec=0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday = 1;
	tm.tm_mon -= tm.tm_mon % basestep; break;

    case TMT_YEAR:
	tm.tm_sec=0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday = 1;
	tm.tm_mon = 0;
	tm.tm_year -= (tm.tm_year+1900) % basestep;
	
    }
    return mktime(&tm);
}
/* identify the point where the next gridline, label ... gets placed */
time_t 
find_next_time(
    time_t   current, /* what is the initial time */
    enum tmt_en baseint,  /* what is the basic interval */
    long     basestep /* how many if these do we jump a time */
    )
{
    struct tm tm;
    time_t madetime;
    tm = *localtime(&current);
    do {
	switch(baseint){
	case TMT_SECOND:
	    tm.tm_sec += basestep; break;
	case TMT_MINUTE: 
	    tm.tm_min += basestep; break;
	case TMT_HOUR:
	    tm.tm_hour += basestep; break;
	case TMT_DAY:
	    tm.tm_mday += basestep; break;
	case TMT_WEEK:
	    tm.tm_mday += 7*basestep; break;
	case TMT_MONTH:
	    tm.tm_mon += basestep; break;
	case TMT_YEAR:
	    tm.tm_year += basestep;	
	}
	madetime = mktime(&tm);
    } while (madetime == -1); /* this is necessary to skip impssible times
				 like the daylight saving time skips */
    return madetime;
	  
}


/* calculate values required for PRINT and GPRINT functions */

int
print_calc(image_desc_t *im, char ***prdata) 
{
    long i,ii,validsteps;
    double printval;
    time_t printtime;
    int graphelement = 0;
    long vidx;
    int max_ii;	
    double magfact = -1;
    char *si_symb = "";
    char *percent_s;
    int prlines = 1;
    if (im->imginfo) prlines++;
    for(i=0;i<im->gdes_c;i++){
	switch(im->gdes[i].gf){
	case GF_PRINT:
	    prlines++;
	    if(((*prdata) = rrd_realloc((*prdata),prlines*sizeof(char *)))==NULL){
		rrd_set_error("realloc prdata");
		return 0;
	    }
	case GF_GPRINT:
	    /* PRINT and GPRINT can now print VDEF generated values.
	     * There's no need to do any calculations on them as these
	     * calculations were already made.
	     */
	    vidx = im->gdes[i].vidx;
	    if (im->gdes[vidx].gf==GF_VDEF) { /* simply use vals */
		printval = im->gdes[vidx].vf.val;
		printtime = im->gdes[vidx].vf.when;
	    } else { /* need to calculate max,min,avg etcetera */
		max_ii =((im->gdes[vidx].end 
			- im->gdes[vidx].start)
			/ im->gdes[vidx].step
			* im->gdes[vidx].ds_cnt);
		printval = DNAN;
		validsteps = 0;
		for(	ii=im->gdes[vidx].ds;
			ii < max_ii;
			ii+=im->gdes[vidx].ds_cnt){
		    if (! finite(im->gdes[vidx].data[ii]))
			continue;
		    if (isnan(printval)){
			printval = im->gdes[vidx].data[ii];
			validsteps++;
			continue;
		    }

		    switch (im->gdes[i].cf){
			case CF_HWPREDICT:
			case CF_DEVPREDICT:
			case CF_DEVSEASONAL:
			case CF_SEASONAL:
			case CF_AVERAGE:
			    validsteps++;
			    printval += im->gdes[vidx].data[ii];
			    break;
			case CF_MINIMUM:
			    printval = min( printval, im->gdes[vidx].data[ii]);
			    break;
			case CF_FAILURES:
			case CF_MAXIMUM:
			    printval = max( printval, im->gdes[vidx].data[ii]);
			    break;
			case CF_LAST:
			    printval = im->gdes[vidx].data[ii];
		    }
		}
		if (im->gdes[i].cf==CF_AVERAGE || im->gdes[i].cf > CF_LAST) {
		    if (validsteps > 1) {
			printval = (printval / validsteps);
		    }
		}
	    } /* prepare printval */

	    if (!strcmp(im->gdes[i].format,"%c")) { /* VDEF time print */
		if (im->gdes[i].gf == GF_PRINT){
		    (*prdata)[prlines-2] = malloc((FMT_LEG_LEN+2)*sizeof(char));
		    sprintf((*prdata)[prlines-2],"%s (%lu)",
					ctime(&printtime),printtime);
		    (*prdata)[prlines-1] = NULL;
		} else {
		    sprintf(im->gdes[i].legend,"%s (%lu)",
					ctime(&printtime),printtime);
		    graphelement = 1;
		}
	    } else {
	    if ((percent_s = strstr(im->gdes[i].format,"%S")) != NULL) {
		/* Magfact is set to -1 upon entry to print_calc.  If it
		 * is still less than 0, then we need to run auto_scale.
		 * Otherwise, put the value into the correct units.  If
		 * the value is 0, then do not set the symbol or magnification
		 * so next the calculation will be performed again. */
		if (magfact < 0.0) {
		    auto_scale(im,&printval,&si_symb,&magfact);
		    if (printval == 0.0)
			magfact = -1.0;
		} else {
		    printval /= magfact;
		}
		*(++percent_s) = 's';
	    } else if (strstr(im->gdes[i].format,"%s") != NULL) {
		auto_scale(im,&printval,&si_symb,&magfact);
	    }

	    if (im->gdes[i].gf == GF_PRINT){
		(*prdata)[prlines-2] = malloc((FMT_LEG_LEN+2)*sizeof(char));
		if (bad_format(im->gdes[i].format)) {
			rrd_set_error("bad format for [G]PRINT in '%s'", im->gdes[i].format);
			return -1;
		}
#ifdef HAVE_SNPRINTF
		snprintf((*prdata)[prlines-2],FMT_LEG_LEN,im->gdes[i].format,printval,si_symb);
#else
		sprintf((*prdata)[prlines-2],im->gdes[i].format,printval,si_symb);
#endif
		(*prdata)[prlines-1] = NULL;
	    } else {
		/* GF_GPRINT */

		if (bad_format(im->gdes[i].format)) {
			rrd_set_error("bad format for [G]PRINT in '%s'", im->gdes[i].format);
			return -1;
		}
#ifdef HAVE_SNPRINTF
		snprintf(im->gdes[i].legend,FMT_LEG_LEN-2,im->gdes[i].format,printval,si_symb);
#else
		sprintf(im->gdes[i].legend,im->gdes[i].format,printval,si_symb);
#endif
		graphelement = 1;
	    }
	    }
	    break;
	case GF_LINE:
	case GF_AREA:
	case GF_TICK:
	case GF_STACK:
	case GF_HRULE:
	case GF_VRULE:
	    graphelement = 1;
	    break;
        case GF_COMMENT:
	case GF_DEF:
	case GF_CDEF:	    
	case GF_VDEF:	    
	case GF_PART:
	    break;
	}
    }
    return graphelement;
}


/* place legends with color spots */
int
leg_place(image_desc_t *im)
{
    /* graph labels */
    int   interleg = im->text_prop[TEXT_PROP_LEGEND].size*2.0;
    int   box =im->text_prop[TEXT_PROP_LEGEND].size*1.5;
    int   border = im->text_prop[TEXT_PROP_LEGEND].size*2.0;
    int   fill=0, fill_last;
    int   leg_c = 0;
    int   leg_x = border, leg_y = im->yimg;
    int   leg_cc;
    int   glue = 0;
    int   i,ii, mark = 0;
    char  prt_fctn; /*special printfunctions */
    int  *legspace;

  if( !(im->extra_flags & NOLEGEND) ) {
    if ((legspace = malloc(im->gdes_c*sizeof(int)))==NULL){
       rrd_set_error("malloc for legspace");
       return -1;
    }

    for(i=0;i<im->gdes_c;i++){
	fill_last = fill;

	leg_cc = strlen(im->gdes[i].legend);
	
	/* is there a controle code ant the end of the legend string ? */ 
	if (leg_cc >= 2 && im->gdes[i].legend[leg_cc-2] == '\\') {
	    prt_fctn = im->gdes[i].legend[leg_cc-1];
	    leg_cc -= 2;
	    im->gdes[i].legend[leg_cc] = '\0';
	} else {
	    prt_fctn = '\0';
	}
        /* remove exess space */
        while (prt_fctn=='g' && 
	       leg_cc > 0 && 
	       im->gdes[i].legend[leg_cc-1]==' '){
	   leg_cc--;
	   im->gdes[i].legend[leg_cc]='\0';
	}
	if (leg_cc != 0 ){
	   legspace[i]=(prt_fctn=='g' ? 0 : interleg);
	   
	   if (fill > 0){ 
 	       /* no interleg space if string ends in \g */
	       fill += legspace[i];
	    }
	    if (im->gdes[i].gf != GF_GPRINT && 
		im->gdes[i].gf != GF_COMMENT) { 
		fill += box; 	   
	    }
	   fill += gfx_get_text_width(im->canvas, fill+border,
				      im->text_prop[TEXT_PROP_LEGEND].font,
				      im->text_prop[TEXT_PROP_LEGEND].size,
				      im->tabwidth,
				      im->gdes[i].legend);
	    leg_c++;
	} else {
	   legspace[i]=0;
	}
        /* who said there was a special tag ... ?*/
	if (prt_fctn=='g') {    
	   prt_fctn = '\0';
	}
	if (prt_fctn == '\0') {
	    if (i == im->gdes_c -1 ) prt_fctn ='l';
	    
	    /* is it time to place the legends ? */
	    if (fill > im->ximg - 2*border){
		if (leg_c > 1) {
		    /* go back one */
		    i--; 
		    fill = fill_last;
		    leg_c--;
		    prt_fctn = 'j';
		} else {
		    prt_fctn = 'l';
		}
		
	    }
	}


	if (prt_fctn != '\0'){
	    leg_x = border;
	    if (leg_c >= 2 && prt_fctn == 'j') {
		glue = (im->ximg - fill - 2* border) / (leg_c-1);
	    } else {
		glue = 0;
	    }
	    if (prt_fctn =='c') leg_x =  (im->ximg - fill) / 2.0;
	    if (prt_fctn =='r') leg_x =  im->ximg - fill - border;

	    for(ii=mark;ii<=i;ii++){
		if(im->gdes[ii].legend[0]=='\0')
		    continue;
		im->gdes[ii].leg_x = leg_x;
		im->gdes[ii].leg_y = leg_y;
	        leg_x += 
		 gfx_get_text_width(im->canvas, leg_x,
				      im->text_prop[TEXT_PROP_LEGEND].font,
				      im->text_prop[TEXT_PROP_LEGEND].size,
				      im->tabwidth,
				      im->gdes[ii].legend) 
		   + legspace[ii]
		   + glue;
		if (im->gdes[ii].gf != GF_GPRINT && 
		    im->gdes[ii].gf != GF_COMMENT) 
		    leg_x += box; 	   
	    }	    
	    leg_y = leg_y + im->text_prop[TEXT_PROP_LEGEND].size*1.2;
	    if (prt_fctn == 's') leg_y -=  im->text_prop[TEXT_PROP_LEGEND].size*1.2;	   
	    fill = 0;
	    leg_c = 0;
	    mark = ii;
	}	   
    }
    im->yimg = leg_y;
    free(legspace);
  }
  return 0;
}

/* create a grid on the graph. it determines what to do
   from the values of xsize, start and end */

/* the xaxis labels are determined from the number of seconds per pixel
   in the requested graph */



int
horizontal_grid(image_desc_t   *im)
{
    double   range;
    double   scaledrange;
    int      pixel,i;
    int      sgrid,egrid;
    double   gridstep;
    double   scaledstep;
    char     graph_label[100];
    double   X0,X1,Y0;
    int      labfact,gridind;
    int      decimals, fractionals;
    char     labfmt[64];

    labfact=2;
    gridind=-1;
    range =  im->maxval - im->minval;
    scaledrange = range / im->magfact;

	/* does the scale of this graph make it impossible to put lines
	   on it? If so, give up. */
	if (isnan(scaledrange)) {
		return 0;
	}

    /* find grid spaceing */
    pixel=1;
    if(isnan(im->ygridstep)){
	if(im->extra_flags & ALTYGRID) {
	    /* find the value with max number of digits. Get number of digits */
	    decimals = ceil(log10(max(fabs(im->maxval), fabs(im->minval))));
	    if(decimals <= 0) /* everything is small. make place for zero */
		decimals = 1;
	    
	    fractionals = floor(log10(range));
	    if(fractionals < 0) /* small amplitude. */
		sprintf(labfmt, "%%%d.%df", decimals - fractionals + 1, -fractionals + 1);
	    else
		sprintf(labfmt, "%%%d.1f", decimals + 1);
	    gridstep = pow((double)10, (double)fractionals);
	    if(gridstep == 0) /* range is one -> 0.1 is reasonable scale */
		gridstep = 0.1;
	    /* should have at least 5 lines but no more then 15 */
	    if(range/gridstep < 5)
                gridstep /= 10;
	    if(range/gridstep > 15)
                gridstep *= 10;
	    if(range/gridstep > 5) {
		labfact = 1;
		if(range/gridstep > 8)
		    labfact = 2;
	    }
	    else {
		gridstep /= 5;
		labfact = 5;
	    }
	}
	else {
	    for(i=0;ylab[i].grid > 0;i++){
		pixel = im->ysize / (scaledrange / ylab[i].grid);
		if (gridind == -1 && pixel > 5) {
		    gridind = i;
		    break;
		}
	    }
	    
	    for(i=0; i<4;i++) {
	       if (pixel * ylab[gridind].lfac[i] >=  2 * im->text_prop[TEXT_PROP_AXIS].size) {
		  labfact =  ylab[gridind].lfac[i];
		  break;
	       }		          
	    } 
	    
	    gridstep = ylab[gridind].grid * im->magfact;
	}
    } else {
	gridstep = im->ygridstep;
	labfact = im->ylabfact;
    }
    
   X0=im->xorigin;
   X1=im->xorigin+im->xsize;
   
    sgrid = (int)( im->minval / gridstep - 1);
    egrid = (int)( im->maxval / gridstep + 1);
    scaledstep = gridstep/im->magfact;
    for (i = sgrid; i <= egrid; i++){
       Y0=ytr(im,gridstep*i);
       if ( Y0 >= im->yorigin-im->ysize
	         && Y0 <= im->yorigin){       
	    if(i % labfact == 0){		
		if (i==0 || im->symbol == ' ') {
		    if(scaledstep < 1){
			if(im->extra_flags & ALTYGRID) {
			    sprintf(graph_label,labfmt,scaledstep*i);
			}
			else {
			    sprintf(graph_label,"%4.1f",scaledstep*i);
			}
		    } else {
			sprintf(graph_label,"%4.0f",scaledstep*i);
		    }
		}else {
		    if(scaledstep < 1){
			sprintf(graph_label,"%4.1f %c",scaledstep*i, im->symbol);
		    } else {
			sprintf(graph_label,"%4.0f %c",scaledstep*i, im->symbol);
		    }
		}

	       gfx_new_text ( im->canvas,
			      X0-im->text_prop[TEXT_PROP_AXIS].size/1.5, Y0,
			      im->graph_col[GRC_FONT],
			      im->text_prop[TEXT_PROP_AXIS].font,
			      im->text_prop[TEXT_PROP_AXIS].size,
			      im->tabwidth, 0.0, GFX_H_RIGHT, GFX_V_CENTER,
			      graph_label );
	       gfx_new_line ( im->canvas,
			      X0-2,Y0,
			      X1+2,Y0,
			      MGRIDWIDTH, im->graph_col[GRC_MGRID] );	       
	       
	    } else {		
	       gfx_new_line ( im->canvas,
			      X0-1,Y0,
			      X1+1,Y0,
			      GRIDWIDTH, im->graph_col[GRC_GRID] );	       
	       
	    }	    
	}	
    } 
    return 1;
}

/* logaritmic horizontal grid */
int
horizontal_log_grid(image_desc_t   *im)   
{
    double   pixpex;
    int      ii,i;
    int      minoridx=0, majoridx=0;
    char     graph_label[100];
    double   X0,X1,Y0;   
    double   value, pixperstep, minstep;

    /* find grid spaceing */
    pixpex= (double)im->ysize / (log10(im->maxval) - log10(im->minval));

	if (isnan(pixpex)) {
		return 0;
	}

    for(i=0;yloglab[i][0] > 0;i++){
	minstep = log10(yloglab[i][0]);
	for(ii=1;yloglab[i][ii+1] > 0;ii++){
	    if(yloglab[i][ii+2]==0){
		minstep = log10(yloglab[i][ii+1])-log10(yloglab[i][ii]);
		break;
	    }
	}
	pixperstep = pixpex * minstep;
	if(pixperstep > 5){minoridx = i;}
       if(pixperstep > 2 *  im->text_prop[TEXT_PROP_LEGEND].size){majoridx = i;}
    }
   
   X0=im->xorigin;
   X1=im->xorigin+im->xsize;
    /* paint minor grid */
    for (value = pow((double)10, log10(im->minval) 
			  - fmod(log10(im->minval),log10(yloglab[minoridx][0])));
	 value  <= im->maxval;
	 value *= yloglab[minoridx][0]){
	if (value < im->minval) continue;
	i=0;	
	while(yloglab[minoridx][++i] > 0){	    
	   Y0 = ytr(im,value * yloglab[minoridx][i]);
	   if (Y0 <= im->yorigin - im->ysize) break;
	   gfx_new_line ( im->canvas,
			  X0-1,Y0,
			  X1+1,Y0,
			  GRIDWIDTH, im->graph_col[GRC_GRID] );
	}
    }

    /* paint major grid and labels*/
    for (value = pow((double)10, log10(im->minval) 
			  - fmod(log10(im->minval),log10(yloglab[majoridx][0])));
	 value <= im->maxval;
	 value *= yloglab[majoridx][0]){
	if (value < im->minval) continue;
	i=0;	
	while(yloglab[majoridx][++i] > 0){	    
	   Y0 = ytr(im,value * yloglab[majoridx][i]);    
	   if (Y0 <= im->yorigin - im->ysize) break;
	   gfx_new_line ( im->canvas,
			  X0-2,Y0,
			  X1+2,Y0,
			  MGRIDWIDTH, im->graph_col[GRC_MGRID] );
	   
	   sprintf(graph_label,"%3.0e",value * yloglab[majoridx][i]);
	   gfx_new_text ( im->canvas,
			  X0-im->text_prop[TEXT_PROP_AXIS].size/1.5, Y0,
			  im->graph_col[GRC_FONT],
			  im->text_prop[TEXT_PROP_AXIS].font,
			  im->text_prop[TEXT_PROP_AXIS].size,
			  im->tabwidth,0.0, GFX_H_RIGHT, GFX_V_CENTER,
			  graph_label );
	} 
    }
	return 1;
}


void
vertical_grid(
    image_desc_t   *im )
{   
    int xlab_sel;		/* which sort of label and grid ? */
    time_t ti, tilab;
    long factor;
    char graph_label[100];
    double X0,Y0,Y1; /* points for filled graph and more*/
   

    /* the type of time grid is determined by finding
       the number of seconds per pixel in the graph */
    
    
    if(im->xlab_user.minsec == -1){
	factor=(im->end - im->start)/im->xsize;
	xlab_sel=0;
	while ( xlab[xlab_sel+1].minsec != -1 
		&& xlab[xlab_sel+1].minsec <= factor){ xlab_sel++; }
	im->xlab_user.gridtm = xlab[xlab_sel].gridtm;
	im->xlab_user.gridst = xlab[xlab_sel].gridst;
	im->xlab_user.mgridtm = xlab[xlab_sel].mgridtm;
	im->xlab_user.mgridst = xlab[xlab_sel].mgridst;
	im->xlab_user.labtm = xlab[xlab_sel].labtm;
	im->xlab_user.labst = xlab[xlab_sel].labst;
	im->xlab_user.precis = xlab[xlab_sel].precis;
	im->xlab_user.stst = xlab[xlab_sel].stst;
    }
    
    /* y coords are the same for every line ... */
    Y0 = im->yorigin;
    Y1 = im->yorigin-im->ysize;
   

    /* paint the minor grid */
    for(ti = find_first_time(im->start,
			    im->xlab_user.gridtm,
			    im->xlab_user.gridst);
	ti < im->end; 
	ti = find_next_time(ti,im->xlab_user.gridtm,im->xlab_user.gridst)
	){
	/* are we inside the graph ? */
	if (ti < im->start || ti > im->end) continue;
       X0 = xtr(im,ti);       
       gfx_new_line(im->canvas,X0,Y0+1, X0,Y1-1,GRIDWIDTH, im->graph_col[GRC_GRID]);
       
    }

    /* paint the major grid */
    for(ti = find_first_time(im->start,
			    im->xlab_user.mgridtm,
			    im->xlab_user.mgridst);
	ti < im->end; 
	ti = find_next_time(ti,im->xlab_user.mgridtm,im->xlab_user.mgridst)
	){
	/* are we inside the graph ? */
	if (ti < im->start || ti > im->end) continue;
       X0 = xtr(im,ti);
       gfx_new_line(im->canvas,X0,Y0+2, X0,Y1-2,MGRIDWIDTH, im->graph_col[GRC_MGRID]);
       
    }
    /* paint the labels below the graph */
    for(ti = find_first_time(im->start,
			    im->xlab_user.labtm,
			    im->xlab_user.labst);
	ti <= im->end; 
	ti = find_next_time(ti,im->xlab_user.labtm,im->xlab_user.labst)
	){
        tilab= ti + im->xlab_user.precis/2; /* correct time for the label */
	/* are we inside the graph ? */
	if (ti < im->start || ti > im->end) continue;

#if HAVE_STRFTIME
	strftime(graph_label,99,im->xlab_user.stst,localtime(&tilab));
#else
# error "your libc has no strftime I guess we'll abort the exercise here."
#endif
       gfx_new_text ( im->canvas,
		      xtr(im,tilab), Y0+im->text_prop[TEXT_PROP_AXIS].size/1.5,
		      im->graph_col[GRC_FONT],
		      im->text_prop[TEXT_PROP_AXIS].font,
		      im->text_prop[TEXT_PROP_AXIS].size,
		      im->tabwidth, 0.0, GFX_H_CENTER, GFX_V_TOP,
		      graph_label );
       
    }

}


void 
axis_paint(
   image_desc_t   *im
	   )
{   
    /* draw x and y axis */
    gfx_new_line ( im->canvas, im->xorigin+im->xsize,im->yorigin,
		      im->xorigin+im->xsize,im->yorigin-im->ysize,
		      GRIDWIDTH, im->graph_col[GRC_GRID]);
       
       gfx_new_line ( im->canvas, im->xorigin,im->yorigin-im->ysize,
		         im->xorigin+im->xsize,im->yorigin-im->ysize,
		         GRIDWIDTH, im->graph_col[GRC_GRID]);
   
       gfx_new_line ( im->canvas, im->xorigin-4,im->yorigin,
		         im->xorigin+im->xsize+4,im->yorigin,
		         MGRIDWIDTH, im->graph_col[GRC_GRID]);
   
       gfx_new_line ( im->canvas, im->xorigin,im->yorigin+4,
		         im->xorigin,im->yorigin-im->ysize-4,
		         MGRIDWIDTH, im->graph_col[GRC_GRID]);
   
    
    /* arrow for X axis direction */
    gfx_new_area ( im->canvas, 
		   im->xorigin+im->xsize+3,  im->yorigin-3,
		   im->xorigin+im->xsize+3,  im->yorigin+4,
		   im->xorigin+im->xsize+8,  im->yorigin+0.5, /* LINEOFFSET */
		   im->graph_col[GRC_ARROW]);
   
   
   
}

void
grid_paint(image_desc_t   *im)
{   
    long i;
    int res=0;
    double X0,Y0; /* points for filled graph and more*/
    gfx_node_t *node;

    /* draw 3d border */
    node = gfx_new_area (im->canvas, 0,im->yimg,
                                 2,im->yimg-2,
                                 2,2,im->graph_col[GRC_SHADEA]);
    gfx_add_point( node , im->ximg - 2, 2 );
    gfx_add_point( node , im->ximg, 0 );
    gfx_add_point( node , 0,0 );
/*    gfx_add_point( node , 0,im->yimg ); */
   
    node =  gfx_new_area (im->canvas, 2,im->yimg-2,
                                  im->ximg-2,im->yimg-2,
                                  im->ximg - 2, 2,
                                 im->graph_col[GRC_SHADEB]);
    gfx_add_point( node ,   im->ximg,0);
    gfx_add_point( node ,   im->ximg,im->yimg);
    gfx_add_point( node ,   0,im->yimg);
/*    gfx_add_point( node , 0,im->yimg ); */
   
   
    if (im->draw_x_grid == 1 )
      vertical_grid(im);
    
    if (im->draw_y_grid == 1){
	if(im->logarithmic){
		res = horizontal_log_grid(im);
	} else {
		res = horizontal_grid(im);
	}

	/* dont draw horizontal grid if there is no min and max val */
	if (! res ) {
	  char *nodata = "No Data found";
	   gfx_new_text(im->canvas,im->ximg/2, (2*im->yorigin-im->ysize) / 2,
			im->graph_col[GRC_FONT],
			im->text_prop[TEXT_PROP_AXIS].font,
			im->text_prop[TEXT_PROP_AXIS].size,
			im->tabwidth, 0.0, GFX_H_CENTER, GFX_V_CENTER,
			nodata );	   
	}
    }

    /* yaxis description */
	if (im->canvas->imgformat != IF_PNG) {
	    gfx_new_text( im->canvas,
			  7, (im->yorigin - im->ysize/2),
			  im->graph_col[GRC_FONT],
			  im->text_prop[TEXT_PROP_AXIS].font,
			  im->text_prop[TEXT_PROP_AXIS].size, im->tabwidth, 270.0,
			  GFX_H_CENTER, GFX_V_CENTER,
			  im->ylegend);
	} else {
	    /* horrible hack until we can actually print vertically */
	    {
		int n;
		int l=strlen(im->ylegend);
		char s[2];
		for (n=0;n<strlen(im->ylegend);n++) {
		    s[0]=im->ylegend[n];
		    s[1]='\0';
		    gfx_new_text(im->canvas,7,im->text_prop[TEXT_PROP_AXIS].size*(l-n),
			im->graph_col[GRC_FONT],
			im->text_prop[TEXT_PROP_AXIS].font,
			im->text_prop[TEXT_PROP_AXIS].size, im->tabwidth, 270.0,
			GFX_H_CENTER, GFX_V_CENTER,
			s);
		}
	    }
	}
   
    /* graph title */
    gfx_new_text( im->canvas,
		  im->ximg/2, im->text_prop[TEXT_PROP_TITLE].size,
		  im->graph_col[GRC_FONT],
		  im->text_prop[TEXT_PROP_TITLE].font,
		  im->text_prop[TEXT_PROP_TITLE].size, im->tabwidth, 0.0,
		  GFX_H_CENTER, GFX_V_CENTER,
		  im->title);

    /* graph labels */
    if( !(im->extra_flags & NOLEGEND) ) {
      for(i=0;i<im->gdes_c;i++){
	if(im->gdes[i].legend[0] =='\0')
	    continue;
	 
	/* im->gdes[i].leg_y is the bottom of the legend */
		X0 = im->gdes[i].leg_x;
		Y0 = im->gdes[i].leg_y;
		/* Box needed? */
		if (	   im->gdes[i].gf != GF_GPRINT
			&& im->gdes[i].gf != GF_COMMENT) {
		    int boxH, boxV;

		    boxH = gfx_get_text_width(im->canvas, 0,
				im->text_prop[TEXT_PROP_AXIS].font,
				im->text_prop[TEXT_PROP_AXIS].size,
				im->tabwidth,"M") * 1.25;
		    boxV = boxH;

		    node = gfx_new_area(im->canvas,
				X0,Y0-boxV,
				X0,Y0,
				X0+boxH,Y0,
				im->gdes[i].col);
		    gfx_add_point ( node, X0+boxH, Y0-boxV );
		    node = gfx_new_line(im->canvas,
				X0,Y0-boxV, X0,Y0,
				1,0x000000FF);
		    gfx_add_point(node,X0+boxH,Y0);
		    gfx_add_point(node,X0+boxH,Y0-boxV);
		    gfx_close_path(node);
		    X0 += boxH / 1.25 * 2;
		}
		gfx_new_text ( im->canvas, X0, Y0,
				   im->graph_col[GRC_FONT],
				   im->text_prop[TEXT_PROP_AXIS].font,
				   im->text_prop[TEXT_PROP_AXIS].size,
				   im->tabwidth,0.0, GFX_H_LEFT, GFX_V_BOTTOM,
				   im->gdes[i].legend );
	      }
	   }
	}


/*****************************************************
 * lazy check make sure we rely need to create this graph
 *****************************************************/

int lazy_check(image_desc_t *im){
    FILE *fd = NULL;
	int size = 1;
    struct stat  imgstat;
    
    if (im->lazy == 0) return 0; /* no lazy option */
    if (stat(im->graphfile,&imgstat) != 0) 
      return 0; /* can't stat */
    /* one pixel in the existing graph is more then what we would
       change here ... */
    if (time(NULL) - imgstat.st_mtime > 
	(im->end - im->start) / im->xsize) 
      return 0;
    if ((fd = fopen(im->graphfile,"rb")) == NULL) 
      return 0; /* the file does not exist */
    switch (im->canvas->imgformat) {
    case IF_PNG:
	   size = PngSize(fd,&(im->ximg),&(im->yimg));
	   break;
    default:
	   size = 1;
    }
    fclose(fd);
    return size;
}

void
pie_part(image_desc_t *im, gfx_color_t color,
	    double PieCenterX, double PieCenterY, double Radius,
	    double startangle, double endangle)
{
    gfx_node_t *node;
    double angle;
    double step=M_PI/50; /* Number of iterations for the circle;
			 ** 10 is definitely too low, more than
			 ** 50 seems to be overkill
			 */

    /* Strange but true: we have to work clockwise or else
    ** anti aliasing nor transparency don't work.
    **
    ** This test is here to make sure we do it right, also
    ** this makes the for...next loop more easy to implement.
    ** The return will occur if the user enters a negative number
    ** (which shouldn't be done according to the specs) or if the
    ** programmers do something wrong (which, as we all know, never
    ** happens anyway :)
    */
    if (endangle<startangle) return;

    /* Hidden feature: Radius decreases each full circle */
    angle=startangle;
    while (angle>=2*M_PI) {
	angle  -= 2*M_PI;
	Radius *= 0.8;
    }

    node=gfx_new_area(im->canvas,
		PieCenterX+sin(startangle)*Radius,
		PieCenterY-cos(startangle)*Radius,
		PieCenterX,
		PieCenterY,
		PieCenterX+sin(endangle)*Radius,
		PieCenterY-cos(endangle)*Radius,
		color);
    for (angle=endangle;angle-startangle>=step;angle-=step) {
	gfx_add_point(node,
		PieCenterX+sin(angle)*Radius,
		PieCenterY-cos(angle)*Radius );
    }
}

int
graph_size_location(image_desc_t *im, int elements, int piechart )
{
    /* The actual size of the image to draw is determined from
    ** several sources.  The size given on the command line is
    ** the graph area but we need more as we have to draw labels
    ** and other things outside the graph area
    */

    /* +-+-------------------------------------------+
    ** |l|.................title.....................|
    ** |e+--+-------------------------------+--------+
    ** |b| b|                               |        |
    ** |a| a|                               |  pie   |
    ** |l| l|          main graph area      | chart  |
    ** |.| .|                               |  area  |
    ** |t| y|                               |        |
    ** |r+--+-------------------------------+--------+
    ** |e|  | x-axis labels                 |        |
    ** |v+--+-------------------------------+--------+
    ** | |..............legends......................|
    ** +-+-------------------------------------------+
    */
    int Xvertical=0,	Yvertical=0,
	Xtitle   =0,	Ytitle   =0,
	Xylabel  =0,	Yylabel  =0,
	Xmain    =0,	Ymain    =0,
	Xpie     =0,	Ypie     =0,
	Xxlabel  =0,	Yxlabel  =0,
#if 0
	Xlegend  =0,	Ylegend  =0,
#endif
	Xspacing =10,	Yspacing =10;

    if (im->ylegend[0] != '\0') {
	Xvertical = im->text_prop[TEXT_PROP_LEGEND].size *2;
	Yvertical = im->text_prop[TEXT_PROP_LEGEND].size * (strlen(im->ylegend)+1);
    }

    if (im->title[0] != '\0') {
	/* The title is placed "inbetween" two text lines so it
	** automatically has some vertical spacing.  The horizontal
	** spacing is added here, on each side.
	*/
	Xtitle = gfx_get_text_width(im->canvas, 0,
		im->text_prop[TEXT_PROP_TITLE].font,
		im->text_prop[TEXT_PROP_TITLE].size,
		im->tabwidth,
		im->title) + 2*Xspacing;
	Ytitle = im->text_prop[TEXT_PROP_TITLE].size*2;
    }

    if (elements) {
	Xmain=im->xsize;
	Ymain=im->ysize;
	if (im->draw_x_grid) {
	    Xxlabel=Xmain;
	    Yxlabel=im->text_prop[TEXT_PROP_LEGEND].size *2;
	}
	if (im->draw_y_grid) {
	    Xylabel=im->text_prop[TEXT_PROP_LEGEND].size *6;
	    Yylabel=Ymain;
	}
    }

    if (piechart) {
	im->piesize=im->xsize<im->ysize?im->xsize:im->ysize;
	Xpie=im->piesize;
	Ypie=im->piesize;
    }

    /* Now calculate the total size.  Insert some spacing where
       desired.  im->xorigin and im->yorigin need to correspond
       with the lower left corner of the main graph area or, if
       this one is not set, the imaginary box surrounding the
       pie chart area. */

    /* The legend width cannot yet be determined, as a result we
    ** have problems adjusting the image to it.  For now, we just
    ** forget about it at all; the legend will have to fit in the
    ** size already allocated.
    */
    im->ximg = Xylabel + Xmain + Xpie + Xspacing;
    if (Xmain) im->ximg += Xspacing;
    if (Xpie) im->ximg += Xspacing;
    im->xorigin = Xspacing + Xylabel;
    if (Xtitle > im->ximg) im->ximg = Xtitle;
    if (Xvertical) {
	im->ximg += Xvertical;
	im->xorigin += Xvertical;
    }
    xtr(im,0);

    /* The vertical size is interesting... we need to compare
    ** the sum of {Ytitle, Ymain, Yxlabel, Ylegend} with Yvertical
    ** however we need to know {Ytitle+Ymain+Yxlabel} in order to
    ** start even thinking about Ylegend.
    **
    ** Do it in three portions: First calculate the inner part,
    ** then do the legend, then adjust the total height of the img.
    */

    /* reserve space for main and/or pie */
    im->yimg = Ymain + Yxlabel;
    if (im->yimg < Ypie) im->yimg = Ypie;
    im->yorigin = im->yimg - Yxlabel;
    /* reserve space for the title *or* some padding above the graph */
    if (Ytitle) {
	im->yimg += Ytitle;
	im->yorigin += Ytitle;
    } else {
	im->yimg += Yspacing;
	im->yorigin += Yspacing;
    }
    /* reserve space for padding below the graph */
    im->yimg += Yspacing;
    ytr(im,DNAN);

    /* Determine where to place the legends onto the image.
    ** Adjust im->yimg to match the space requirements.
    */
    if(leg_place(im)==-1)
	return -1;

    /* last of three steps: check total height of image */
    if (im->yimg < Yvertical) im->yimg = Yvertical;

#if 0
    if (Xlegend > im->ximg) {
	im->ximg = Xlegend;
	/* reposition Pie */
#endif

    /* The pie is placed in the upper right hand corner,
    ** just below the title (if any) and with sufficient
    ** padding.
    */
    if (elements) {
	im->pie_x = im->ximg - Xspacing - Xpie/2;
        im->pie_y = im->yorigin-Ymain+Ypie/2;
    } else {
	im->pie_x = im->ximg/2;
        im->pie_y = im->yorigin-Ypie/2;
    }

    return 0;
}

/* draw that picture thing ... */
int
graph_paint(image_desc_t *im, char ***calcpr)
{
  int i,ii;
  int lazy =     lazy_check(im);
  int piechart = 0;
  double PieStart=0.0;
  FILE  *fo;
  gfx_node_t *node;
  
  double areazero = 0.0;
  enum gf_en stack_gf = GF_PRINT;
  graph_desc_t *lastgdes = NULL;    
  
  /* if we are lazy and there is nothing to PRINT ... quit now */
  if (lazy && im->prt_c==0) return 0;
  
  /* pull the data from the rrd files ... */
  
  if(data_fetch(im)==-1)
    return -1;
  
  /* evaluate VDEF and CDEF operations ... */
  if(data_calc(im)==-1)
    return -1;
  
  /* check if we need to draw a piechart */
  for(i=0;i<im->gdes_c;i++){
    if (im->gdes[i].gf == GF_PART) {
      piechart=1;
      break;
    }
  }

  /* calculate and PRINT and GPRINT definitions. We have to do it at
   * this point because it will affect the length of the legends
   * if there are no graph elements we stop here ... 
   * if we are lazy, try to quit ... 
   */
  i=print_calc(im,calcpr);
  if(i<0) return -1;
  if(((i==0)&&(piechart==0)) || lazy) return 0;

  /* If there's only the pie chart to draw, signal this */
  if (i==0) piechart=2;
  
  /* get actual drawing data and find min and max values*/
  if(data_proc(im)==-1)
    return -1;
  
  if(!im->logarithmic){si_unit(im);}        /* identify si magnitude Kilo, Mega Giga ? */
  
  if(!im->rigid && ! im->logarithmic)
    expand_range(im);   /* make sure the upper and lower limit are
                           sensible values */

/**************************************************************
 *** Calculating sizes and locations became a bit confusing ***
 *** so I moved this into a separate function.              ***
 **************************************************************/
  if(graph_size_location(im,i,piechart)==-1)
    return -1;

  /* the actual graph is created by going through the individual
     graph elements and then drawing them */
  
  node=gfx_new_area ( im->canvas,
                      0, 0,
                      im->ximg, 0,
                      im->ximg, im->yimg,
                      im->graph_col[GRC_BACK]);

  gfx_add_point(node,0, im->yimg);

  if (piechart != 2) {
    node=gfx_new_area ( im->canvas,
                      im->xorigin,             im->yorigin, 
                      im->xorigin + im->xsize, im->yorigin,
                      im->xorigin + im->xsize, im->yorigin-im->ysize,
                      im->graph_col[GRC_CANVAS]);
  
    gfx_add_point(node,im->xorigin, im->yorigin - im->ysize);

    if (im->minval > 0.0)
      areazero = im->minval;
    if (im->maxval < 0.0)
      areazero = im->maxval;
  
    axis_paint(im);
  }

  if (piechart) {
    pie_part(im,im->graph_col[GRC_CANVAS],im->pie_x,im->pie_y,im->piesize*0.5,0,2*M_PI);
  }

  for(i=0;i<im->gdes_c;i++){
    switch(im->gdes[i].gf){
    case GF_CDEF:
    case GF_VDEF:
    case GF_DEF:
    case GF_PRINT:
    case GF_GPRINT:
    case GF_COMMENT:
    case GF_HRULE:
    case GF_VRULE:
      break;
    case GF_TICK:
      for (ii = 0; ii < im->xsize; ii++)
        {
          if (!isnan(im->gdes[i].p_data[ii]) && 
              im->gdes[i].p_data[ii] > 0.0)
            { 
              /* generate a tick */
              gfx_new_line(im->canvas, im -> xorigin + ii, 
                           im -> yorigin - (im -> gdes[i].yrule * im -> ysize),
                           im -> xorigin + ii, 
                           im -> yorigin,
                           1.0,
                           im -> gdes[i].col );
            }
        }
      break;
    case GF_LINE:
    case GF_AREA:
      stack_gf = im->gdes[i].gf;
    case GF_STACK:          
      /* fix data points at oo and -oo */
      for(ii=0;ii<im->xsize;ii++){
        if (isinf(im->gdes[i].p_data[ii])){
          if (im->gdes[i].p_data[ii] > 0) {
            im->gdes[i].p_data[ii] = im->maxval ;
          } else {
            im->gdes[i].p_data[ii] = im->minval ;
          }                 
          
        }
      } /* for */
      
      if (im->gdes[i].col != 0x0){               
        /* GF_LINE and friend */
        if(stack_gf == GF_LINE ){
          node = NULL;
          for(ii=1;ii<im->xsize;ii++){
            if ( ! isnan(im->gdes[i].p_data[ii-1])
                 && ! isnan(im->gdes[i].p_data[ii])){
              if (node == NULL){
                node = gfx_new_line(im->canvas,
                                    ii-1+im->xorigin,ytr(im,im->gdes[i].p_data[ii-1]),
                                    ii+im->xorigin,ytr(im,im->gdes[i].p_data[ii]),
                                    im->gdes[i].linewidth,
                                    im->gdes[i].col);
              } else {
                gfx_add_point(node,ii+im->xorigin,ytr(im,im->gdes[i].p_data[ii]));
              }
            } else {
              node = NULL;
            }
          }
        } else {
          int area_start=-1;
          node = NULL;
          for(ii=1;ii<im->xsize;ii++){
            /* open an area */
            if ( ! isnan(im->gdes[i].p_data[ii-1])
                 && ! isnan(im->gdes[i].p_data[ii])){
              if (node == NULL){
                float ybase = 0.0;
                if (im->gdes[i].gf == GF_STACK) {
                  ybase = ytr(im,lastgdes->p_data[ii-1]);
                } else {
                  ybase =  ytr(im,areazero);
                }
                area_start = ii-1;
                node = gfx_new_area(im->canvas,
                                    ii-1+im->xorigin,ybase,
                                    ii-1+im->xorigin,ytr(im,im->gdes[i].p_data[ii-1]),
                                    ii+im->xorigin,ytr(im,im->gdes[i].p_data[ii]),
                                    im->gdes[i].col
                                    );
              } else {
                gfx_add_point(node,ii+im->xorigin,ytr(im,im->gdes[i].p_data[ii]));
              }
            }

            if ( node != NULL && (ii+1==im->xsize || isnan(im->gdes[i].p_data[ii]) )){
              /* GF_AREA STACK type*/
              if (im->gdes[i].gf == GF_STACK ) {
                int iii;
                for (iii=ii-1;iii>area_start;iii--){
                  gfx_add_point(node,iii+im->xorigin,ytr(im,lastgdes->p_data[iii]));
                }
              } else {
                gfx_add_point(node,ii+im->xorigin,ytr(im,areazero));
              };
              node=NULL;
            };
          }             
        } /* else GF_LINE */
      } /* if color != 0x0 */
      /* make sure we do not run into trouble when stacking on NaN */
      for(ii=0;ii<im->xsize;ii++){
        if (isnan(im->gdes[i].p_data[ii])) {
          double ybase = 0.0;
          if (lastgdes) {
            ybase = ytr(im,lastgdes->p_data[ii-1]);
          };
          if (isnan(ybase) || !lastgdes ){
            ybase =  ytr(im,areazero);
          }
          im->gdes[i].p_data[ii] = ybase;
        }
      } 
      lastgdes = &(im->gdes[i]);                         
      break;
    case GF_PART:
      if(isnan(im->gdes[i].yrule)) /* fetch variable */
	im->gdes[i].yrule = im->gdes[im->gdes[i].vidx].vf.val;
     
      if (finite(im->gdes[i].yrule)) {	/* even the fetched var can be NaN */
	pie_part(im,im->gdes[i].col,
		im->pie_x,im->pie_y,im->piesize*0.4,
		M_PI*2.0*PieStart/100.0,
		M_PI*2.0*(PieStart+im->gdes[i].yrule)/100.0);
	PieStart += im->gdes[i].yrule;
      }
      break;
    } /* switch */
  }
  if (piechart==2) {
    im->draw_x_grid=0;
    im->draw_y_grid=0;
  }
  /* grid_paint also does the text */
  grid_paint(im);
  
  /* the RULES are the last thing to paint ... */
  for(i=0;i<im->gdes_c;i++){    
    
    switch(im->gdes[i].gf){
    case GF_HRULE:
      if(isnan(im->gdes[i].yrule)) { /* fetch variable */
        im->gdes[i].yrule = im->gdes[im->gdes[i].vidx].vf.val;
      };
      if(im->gdes[i].yrule >= im->minval
         && im->gdes[i].yrule <= im->maxval)
        gfx_new_line(im->canvas,
                     im->xorigin,ytr(im,im->gdes[i].yrule),
                     im->xorigin+im->xsize,ytr(im,im->gdes[i].yrule),
                     1.0,im->gdes[i].col); 
      break;
    case GF_VRULE:
      if(im->gdes[i].xrule == 0) { /* fetch variable */
        im->gdes[i].xrule = im->gdes[im->gdes[i].vidx].vf.when;
      };
      if(im->gdes[i].xrule >= im->start
         && im->gdes[i].xrule <= im->end)
        gfx_new_line(im->canvas,
                     xtr(im,im->gdes[i].xrule),im->yorigin,
                     xtr(im,im->gdes[i].xrule),im->yorigin-im->ysize,
                     1.0,im->gdes[i].col); 
      break;
    default:
      break;
    }
  }

  
  if (strcmp(im->graphfile,"-")==0) {
#ifdef WIN32
    /* Change translation mode for stdout to BINARY */
    _setmode( _fileno( stdout ), O_BINARY );
#endif
    fo = stdout;
  } else {
    if ((fo = fopen(im->graphfile,"wb")) == NULL) {
      rrd_set_error("Opening '%s' for write: %s",im->graphfile,
                    strerror(errno));
      return (-1);
    }
  }
  gfx_render (im->canvas,im->ximg,im->yimg,0x0,fo);
  if (strcmp(im->graphfile,"-") != 0)
    fclose(fo);
  return 0;
}


/*****************************************************
 * graph stuff 
 *****************************************************/

int
gdes_alloc(image_desc_t *im){

    long def_step = (im->end-im->start)/im->xsize;
    
    if (im->step > def_step) /* step can be increassed ... no decreassed */
      def_step = im->step;

    im->gdes_c++;
    
    if ((im->gdes = (graph_desc_t *) rrd_realloc(im->gdes, (im->gdes_c)
					   * sizeof(graph_desc_t)))==NULL){
	rrd_set_error("realloc graph_descs");
	return -1;
    }


    im->gdes[im->gdes_c-1].step=def_step; 
    im->gdes[im->gdes_c-1].start=im->start; 
    im->gdes[im->gdes_c-1].end=im->end; 
    im->gdes[im->gdes_c-1].vname[0]='\0'; 
    im->gdes[im->gdes_c-1].data=NULL;
    im->gdes[im->gdes_c-1].ds_namv=NULL;
    im->gdes[im->gdes_c-1].data_first=0;
    im->gdes[im->gdes_c-1].p_data=NULL;
    im->gdes[im->gdes_c-1].rpnp=NULL;
    im->gdes[im->gdes_c-1].col = 0x0;
    im->gdes[im->gdes_c-1].legend[0]='\0';
    im->gdes[im->gdes_c-1].rrd[0]='\0';
    im->gdes[im->gdes_c-1].ds=-1;    
    im->gdes[im->gdes_c-1].p_data=NULL;    
    return 0;
}

/* copies input untill the first unescaped colon is found
   or until input ends. backslashes have to be escaped as well */
int
scan_for_col(char *input, int len, char *output)
{
    int inp,outp=0;
    for (inp=0; 
	 inp < len &&
	   input[inp] != ':' &&
	   input[inp] != '\0';
	 inp++){
      if (input[inp] == '\\' &&
	  input[inp+1] != '\0' && 
	  (input[inp+1] == '\\' ||
	   input[inp+1] == ':')){
	output[outp++] = input[++inp];
      }
      else {
	output[outp++] = input[inp];
      }
    }
    output[outp] = '\0';
    return inp;
}

/* Some surgery done on this function, it became ridiculously big.
** Things moved:
** - initializing     now in rrd_graph_init()
** - options parsing  now in rrd_graph_options()
** - script parsing   now in rrd_graph_script()
*/
int 
rrd_graph(int argc, char **argv, char ***prdata, int *xsize, int *ysize)
{
    image_desc_t   im;

    rrd_graph_init(&im);

    rrd_graph_options(argc,argv,&im);
    if (rrd_test_error()) return -1;
    
    if (strlen(argv[optind])>=MAXPATH) {
	rrd_set_error("filename (including path) too long");
	return -1;
    }
    strncpy(im.graphfile,argv[optind],MAXPATH-1);
    im.graphfile[MAXPATH-1]='\0';

    rrd_graph_script(argc,argv,&im);
    if (rrd_test_error()) return -1;

    /* Everything is now read and the actual work can start */

    (*prdata)=NULL;
    if (graph_paint(&im,prdata)==-1){
	im_free(&im);
	return -1;
    }

    /* The image is generated and needs to be output.
    ** Also, if needed, print a line with information about the image.
    */

    *xsize=im.ximg;
    *ysize=im.yimg;
    if (im.imginfo) {
	char *filename;
	if (!(*prdata)) {
	    /* maybe prdata is not allocated yet ... lets do it now */
	    if ((*prdata = calloc(2,sizeof(char *)))==NULL) {
		rrd_set_error("malloc imginfo");
		return -1; 
	    };
	}
	if(((*prdata)[0] = malloc((strlen(im.imginfo)+200+strlen(im.graphfile))*sizeof(char)))
	 ==NULL){
	    rrd_set_error("malloc imginfo");
	    return -1;
	}
	filename=im.graphfile+strlen(im.graphfile);
	while(filename > im.graphfile) {
	    if (*(filename-1)=='/' || *(filename-1)=='\\' ) break;
	    filename--;
	}

	sprintf((*prdata)[0],im.imginfo,filename,(long)(im.canvas->zoom*im.ximg),(long)(im.canvas->zoom*im.yimg));
    }
    im_free(&im);
    return 0;
}

void
rrd_graph_init(image_desc_t *im)
{
    int i;

    im->xlab_user.minsec = -1;
    im->ximg=0;
    im->yimg=0;
    im->xsize = 400;
    im->ysize = 100;
    im->step = 0;
    im->ylegend[0] = '\0';
    im->title[0] = '\0';
    im->minval = DNAN;
    im->maxval = DNAN;    
    im->unitsexponent= 9999;
    im->extra_flags= 0;
    im->rigid = 0;
    im->imginfo = NULL;
    im->lazy = 0;
    im->logarithmic = 0;
    im->ygridstep = DNAN;
    im->draw_x_grid = 1;
    im->draw_y_grid = 1;
    im->base = 1000;
    im->prt_c = 0;
    im->gdes_c = 0;
    im->gdes = NULL;
    im->canvas = gfx_new_canvas();

    for(i=0;i<DIM(graph_col);i++)
        im->graph_col[i]=graph_col[i];

    for(i=0;i<DIM(text_prop);i++){        
      im->text_prop[i].size = text_prop[i].size;
      im->text_prop[i].font = text_prop[i].font;
    }
}

void
rrd_graph_options(int argc, char *argv[],image_desc_t *im)
{
    int			stroff;    
    char		*parsetime_error = NULL;
    char		scan_gtm[12],scan_mtm[12],scan_ltm[12],col_nam[12];
    time_t		start_tmp=0,end_tmp=0;
    long		long_tmp;
    struct time_value	start_tv, end_tv;
    gfx_color_t         color;

    parsetime("end-24h", &start_tv);
    parsetime("now", &end_tv);

    while (1){
	static struct option long_options[] =
	{
	    {"start",      required_argument, 0,  's'},
	    {"end",        required_argument, 0,  'e'},
	    {"x-grid",     required_argument, 0,  'x'},
	    {"y-grid",     required_argument, 0,  'y'},
	    {"vertical-label",required_argument,0,'v'},
	    {"width",      required_argument, 0,  'w'},
	    {"height",     required_argument, 0,  'h'},
	    {"interlaced", no_argument,       0,  'i'},
	    {"upper-limit",required_argument, 0,  'u'},
	    {"lower-limit",required_argument, 0,  'l'},
	    {"rigid",      no_argument,       0,  'r'},
	    {"base",       required_argument, 0,  'b'},
	    {"logarithmic",no_argument,       0,  'o'},
	    {"color",      required_argument, 0,  'c'},
            {"font",       required_argument, 0,  'n'},
	    {"title",      required_argument, 0,  't'},
	    {"imginfo",    required_argument, 0,  'f'},
	    {"imgformat",  required_argument, 0,  'a'},
	    {"lazy",       no_argument,       0,  'z'},
            {"zoom",       required_argument, 0,  'm'},
	    {"no-legend",  no_argument,       0,  'g'},
	    {"alt-y-grid", no_argument,       0,   257 },
	    {"alt-autoscale", no_argument,    0,   258 },
	    {"alt-autoscale-max", no_argument,    0,   259 },
	    {"units-exponent",required_argument, 0,  260},
	    {"step",       required_argument, 0,   261},
	    {0,0,0,0}};
	int option_index = 0;
	int opt;


	opt = getopt_long(argc, argv, 
			  "s:e:x:y:v:w:h:iu:l:rb:oc:n:m:t:f:a:z:g",
			  long_options, &option_index);

	if (opt == EOF)
	    break;
	
	switch(opt) {
	case 257:
	    im->extra_flags |= ALTYGRID;
	    break;
	case 258:
	    im->extra_flags |= ALTAUTOSCALE;
	    break;
	case 259:
	    im->extra_flags |= ALTAUTOSCALE_MAX;
	    break;
	case 'g':
	    im->extra_flags |= NOLEGEND;
	    break;
	case 260:
	    im->unitsexponent = atoi(optarg);
	    break;
	case 261:
	    im->step =  atoi(optarg);
	    break;
	case 's':
	    if ((parsetime_error = parsetime(optarg, &start_tv))) {
	        rrd_set_error( "start time: %s", parsetime_error );
		return;
	    }
	    break;
	case 'e':
	    if ((parsetime_error = parsetime(optarg, &end_tv))) {
	        rrd_set_error( "end time: %s", parsetime_error );
		return;
	    }
	    break;
	case 'x':
	    if(strcmp(optarg,"none") == 0){
	      im->draw_x_grid=0;
	      break;
	    };
	        
	    if(sscanf(optarg,
		      "%10[A-Z]:%ld:%10[A-Z]:%ld:%10[A-Z]:%ld:%ld:%n",
		      scan_gtm,
		      &im->xlab_user.gridst,
		      scan_mtm,
		      &im->xlab_user.mgridst,
		      scan_ltm,
		      &im->xlab_user.labst,
		      &im->xlab_user.precis,
		      &stroff) == 7 && stroff != 0){
                strncpy(im->xlab_form, optarg+stroff, sizeof(im->xlab_form) - 1);
		if((im->xlab_user.gridtm = tmt_conv(scan_gtm)) == -1){
		    rrd_set_error("unknown keyword %s",scan_gtm);
		    return;
		} else if ((im->xlab_user.mgridtm = tmt_conv(scan_mtm)) == -1){
		    rrd_set_error("unknown keyword %s",scan_mtm);
		    return;
		} else if ((im->xlab_user.labtm = tmt_conv(scan_ltm)) == -1){
		    rrd_set_error("unknown keyword %s",scan_ltm);
		    return;
		} 
		im->xlab_user.minsec = 1;
		im->xlab_user.stst = im->xlab_form;
	    } else {
		rrd_set_error("invalid x-grid format");
		return;
	    }
	    break;
	case 'y':

	    if(strcmp(optarg,"none") == 0){
	      im->draw_y_grid=0;
	      break;
	    };

	    if(sscanf(optarg,
		      "%lf:%d",
		      &im->ygridstep,
		      &im->ylabfact) == 2) {
		if(im->ygridstep<=0){
		    rrd_set_error("grid step must be > 0");
		    return;
		} else if (im->ylabfact < 1){
		    rrd_set_error("label factor must be > 0");
		    return;
		} 
	    } else {
		rrd_set_error("invalid y-grid format");
		return;
	    }
	    break;
	case 'v':
	    strncpy(im->ylegend,optarg,150);
	    im->ylegend[150]='\0';
	    break;
	case 'u':
	    im->maxval = atof(optarg);
	    break;
	case 'l':
	    im->minval = atof(optarg);
	    break;
	case 'b':
	    im->base = atol(optarg);
	    if(im->base != 1024 && im->base != 1000 ){
		rrd_set_error("the only sensible value for base apart from 1000 is 1024");
		return;
	    }
	    break;
	case 'w':
	    long_tmp = atol(optarg);
	    if (long_tmp < 10) {
		rrd_set_error("width below 10 pixels");
		return;
	    }
	    im->xsize = long_tmp;
	    break;
	case 'h':
	    long_tmp = atol(optarg);
	    if (long_tmp < 10) {
		rrd_set_error("height below 10 pixels");
		return;
	    }
	    im->ysize = long_tmp;
	    break;
	case 'i':
	    im->canvas->interlaced = 1;
	    break;
	case 'r':
	    im->rigid = 1;
	    break;
	case 'f':
	    im->imginfo = optarg;
	    break;
    	case 'a':
	    if((im->canvas->imgformat = if_conv(optarg)) == -1) {
		rrd_set_error("unsupported graphics format '%s'",optarg);
		return;
	    }
	    break;
	case 'z':
	    im->lazy = 1;
	    break;
	case 'o':
	    im->logarithmic = 1;
	    if (isnan(im->minval))
		im->minval=1;
	    break;
        case 'c':
            if(sscanf(optarg,
                      "%10[A-Z]#%8lx",
                      col_nam,&color) == 2){
                int ci;
                if((ci=grc_conv(col_nam)) != -1){
                    im->graph_col[ci]=color;
                }  else {
                  rrd_set_error("invalid color name '%s'",col_nam);
                }
            } else {
                rrd_set_error("invalid color def format");
                return;
            }
            break;        
        case 'n':{
			/* originally this used char *prop = "" and
			** char *font = "dummy" however this results
			** in a SEG fault, at least on RH7.1
			**
			** The current implementation isn't proper
			** either, font is never freed and prop uses
			** a fixed width string
			*/
	    char prop[100];
	    double size = 1;
	    char *font;

	    font=malloc(255);
	    if(sscanf(optarg,
				"%10[A-Z]:%lf:%s",
				prop,&size,font) == 3){
		int sindex;
		if((sindex=text_prop_conv(prop)) != -1){
		    im->text_prop[sindex].size=size;              
		    im->text_prop[sindex].font=font;
		    if (sindex==0) { /* the default */
			im->text_prop[TEXT_PROP_TITLE].size=size;
			im->text_prop[TEXT_PROP_TITLE].font=font;
			im->text_prop[TEXT_PROP_AXIS].size=size;
			im->text_prop[TEXT_PROP_AXIS].font=font;
			im->text_prop[TEXT_PROP_UNIT].size=size;
			im->text_prop[TEXT_PROP_UNIT].font=font;
			im->text_prop[TEXT_PROP_LEGEND].size=size;
			im->text_prop[TEXT_PROP_LEGEND].font=font;
		    }
		} else {
		    rrd_set_error("invalid fonttag '%s'",prop);
		    return;
		}
	    } else {
		rrd_set_error("invalid text property format");
		return;
	    }
	    break;          
	}
        case 'm':
	    im->canvas->zoom = atof(optarg);
	    if (im->canvas->zoom <= 0.0) {
		rrd_set_error("zoom factor must be > 0");
		return;
	    }
          break;
	case 't':
	    strncpy(im->title,optarg,150);
	    im->title[150]='\0';
	    break;

	case '?':
            if (optopt != 0)
                rrd_set_error("unknown option '%c'", optopt);
            else
                rrd_set_error("unknown option '%s'",argv[optind-1]);
            return;
	}
    }
    
    if (optind >= argc) {
       rrd_set_error("missing filename");
       return;
    }

    if (im->logarithmic == 1 && (im->minval <= 0 || isnan(im->minval))){
	rrd_set_error("for a logarithmic yaxis you must specify a lower-limit > 0");	
	return;
    }

    if (proc_start_end(&start_tv,&end_tv,&start_tmp,&end_tmp) == -1){
	/* error string is set in parsetime.c */
	return;
    }  
    
    if (start_tmp < 3600*24*365*10){
	rrd_set_error("the first entry to fetch should be after 1980 (%ld)",start_tmp);
	return;
    }
    
    if (end_tmp < start_tmp) {
	rrd_set_error("start (%ld) should be less than end (%ld)", 
	       start_tmp, end_tmp);
	return;
    }
    
    im->start = start_tmp;
    im->end = end_tmp;
}

void
rrd_graph_script(int argc, char *argv[], image_desc_t *im)
{
    int		i;
    char	symname[100];
    int		linepass = 0; /* stack must follow LINE*, AREA or STACK */    

    for (i=optind+1;i<argc;i++) {
	int		argstart=0;
	int		strstart=0;
	graph_desc_t	*gdp;
	char		*line;
	char		funcname[10],vname[MAX_VNAME_LEN+1],sep[1];
	double		d;
	double          linewidth;
	int		j,k,l,m;

	/* Each command is one element from *argv[], we call this "line".
	**
	** Each command defines the most current gdes inside struct im.
	** In stead of typing "im->gdes[im->gdes_c-1]" we use "gdp".
	*/
	gdes_alloc(im);
	gdp=&im->gdes[im->gdes_c-1];
	line=argv[i];

	/* function:newvname=string[:ds-name:CF]	for xDEF
	** function:vname[#color[:string]]		for LINEx,AREA,STACK
	** function:vname#color[:num[:string]]		for TICK
	** function:vname-or-num#color[:string]		for xRULE,PART
	** function:vname:CF:string			for xPRINT
	** function:string				for COMMENT
	*/
	argstart=0;

	sscanf(line, "%10[A-Z0-9]:%n", funcname,&argstart);
	if (argstart==0) {
	    rrd_set_error("Cannot parse function in line: %s",line);
	    im_free(im);
	    return;
	}
        if(sscanf(funcname,"LINE%lf",&linewidth)){
                im->gdes[im->gdes_c-1].gf = GF_LINE;
                im->gdes[im->gdes_c-1].linewidth = linewidth;
        } else {
  	  if ((gdp->gf=gf_conv(funcname))==-1) {
	      rrd_set_error("'%s' is not a valid function name",funcname);
	      im_free(im);
	      return;
	  }
        }

	/* If the error string is set, we exit at the end of the switch */
	switch (gdp->gf) {
	    case GF_COMMENT:
		if (rrd_graph_legend(gdp,&line[argstart])==0)
		    rrd_set_error("Cannot parse comment in line: %s",line);
		break;
	    case GF_PART:
	    case GF_VRULE:
	    case GF_HRULE:
		j=k=l=m=0;
		sscanf(&line[argstart], "%lf%n#%n", &d, &j, &k);
		sscanf(&line[argstart], DEF_NAM_FMT "%n#%n", vname, &l, &m);
		if (k+m==0) {
		    rrd_set_error("Cannot parse name or num in line: %s",line);
		    break;
		}
		if (j!=0) {
		    gdp->xrule=d;
		    gdp->yrule=d;
		    argstart+=j;
		} else if (!rrd_graph_check_vname(im,vname,line)) {
		    gdp->xrule=0;
		    gdp->yrule=DNAN;
		    argstart+=l;
		} else break; /* exit due to wrong vname */
		if ((j=rrd_graph_color(im,&line[argstart],line,0))==0) break;
		argstart+=j;
		if (strlen(&line[argstart])!=0) {
		    if (rrd_graph_legend(gdp,&line[++argstart])==0)
			rrd_set_error("Cannot parse comment in line: %s",line);
		}
		break;
	    case GF_STACK:
		if (linepass==0) {
		    rrd_set_error("STACK must follow another graphing element");
		    break;
		}
	    case GF_LINE:
	    case GF_AREA:
	    case GF_TICK:
		j=k=0;
		linepass=1;
		sscanf(&line[argstart],DEF_NAM_FMT"%n%1[#:]%n",vname,&j,sep,&k);
		if (j+1!=k)
		    rrd_set_error("Cannot parse vname in line: %s",line);
		else if (rrd_graph_check_vname(im,vname,line))
		    rrd_set_error("Undefined vname '%s' in line: %s",line);
		else
		    k=rrd_graph_color(im,&line[argstart],line,1);
		if (rrd_test_error()) break;
		argstart=argstart+j+k;
		if ((strlen(&line[argstart])!=0)&&(gdp->gf==GF_TICK)) {
		    j=0;
		    sscanf(&line[argstart], ":%lf%n", &gdp->yrule,&j);
		    argstart+=j;
		}
		if (strlen(&line[argstart])!=0)
		    if (rrd_graph_legend(gdp,&line[++argstart])==0)
			rrd_set_error("Cannot parse legend in line: %s",line);
		break;
	    case GF_PRINT:
		im->prt_c++;
	    case GF_GPRINT:
		j=0;
		sscanf(&line[argstart], DEF_NAM_FMT ":%n",gdp->vname,&j);
		if (j==0) {
		    rrd_set_error("Cannot parse vname in line: '%s'",line);
		    break;
		}
		argstart+=j;
		if (rrd_graph_check_vname(im,gdp->vname,line)) return;
		j=0;
		sscanf(&line[argstart], CF_NAM_FMT ":%n",symname,&j);

		k=(j!=0)?rrd_graph_check_CF(im,symname,line):1;
#define VIDX im->gdes[gdp->vidx]
		switch (k) {
		    case -1: /* looks CF but is not really CF */
			if (VIDX.gf == GF_VDEF) rrd_clear_error();
			break;
		    case  0: /* CF present and correct */
			if (VIDX.gf == GF_VDEF)
			    rrd_set_error("Don't use CF when printing VDEF");
			argstart+=j;
			break;
		    case  1: /* CF not present */
			if (VIDX.gf == GF_VDEF) rrd_clear_error();
			else rrd_set_error("Printing DEF or CDEF needs CF");
			break;
		    default:
			rrd_set_error("Oops, bug in GPRINT scanning");
		}
#undef VIDX
		if (rrd_test_error()) break;

		if (strlen(&line[argstart])!=0) {
		    if (rrd_graph_legend(gdp,&line[argstart])==0)
			rrd_set_error("Cannot parse legend in line: %s",line);
		} else rrd_set_error("No legend in (G)PRINT line: %s",line);
		strcpy(gdp->format, gdp->legend);
		break;
	    case GF_DEF:
	    case GF_VDEF:
	    case GF_CDEF:
		j=0;
		sscanf(&line[argstart], DEF_NAM_FMT "=%n",gdp->vname,&j);
		if (j==0) {
		    rrd_set_error("Could not parse line: %s",line);
		    break;
		}
		if (find_var(im,gdp->vname)!=-1) {
		    rrd_set_error("Variable '%s' in line '%s' already in use\n",
							gdp->vname,line);
		    break;
		}
		argstart+=j;
		switch (gdp->gf) {
		    case GF_DEF:
			argstart+=scan_for_col(&line[argstart],MAXPATH,gdp->rrd);
			j=k=0;
			sscanf(&line[argstart],
				":" DS_NAM_FMT ":" CF_NAM_FMT "%n%*s%n",
				gdp->ds_nam, symname, &j, &k);
			if ((j==0)||(k!=0)) {
			    rrd_set_error("Cannot parse DS or CF in '%s'",line);
			    break;
			}
			rrd_graph_check_CF(im,symname,line);
			break;
		    case GF_VDEF:
			j=0;
			sscanf(&line[argstart],DEF_NAM_FMT ",%n",vname,&j);
			if (j==0) {
			    rrd_set_error("Cannot parse vname in line '%s'",line);
			    break;
			}
			argstart+=j;
			if (rrd_graph_check_vname(im,vname,line)) return;
			if (       im->gdes[gdp->vidx].gf != GF_DEF
				&& im->gdes[gdp->vidx].gf != GF_CDEF) {
			    rrd_set_error("variable '%s' not DEF nor "
				"CDEF in VDEF '%s'", vname,gdp->vname);
			    break;
			}
			vdef_parse(gdp,&line[argstart+strstart]);
			break;
		    case GF_CDEF:
			if (strstr(&line[argstart],":")!=NULL) {
			    rrd_set_error("Error in RPN, line: %s",line);
			    break;
			}
			if ((gdp->rpnp = rpn_parse(
						(void *)im,
						&line[argstart],
						&find_var_wrapper)
				)==NULL)
			    rrd_set_error("invalid rpn expression in: %s",line);
			break;
		    default: break;
		}
		break;
	    default: rrd_set_error("Big oops");
	}
	if (rrd_test_error()) {
	    im_free(im);
	    return;
	}
    }

    if (im->gdes_c==0){
	rrd_set_error("can't make a graph without contents");
	im_free(im); /* ??? is this set ??? */
	return; 
    }
}
int
rrd_graph_check_vname(image_desc_t *im, char *varname, char *err)
{
    if ((im->gdes[im->gdes_c-1].vidx=find_var(im,varname))==-1) {
	rrd_set_error("Unknown variable '%s' in %s",varname,err);
	return -1;
    }
    return 0;
}
int
rrd_graph_color(image_desc_t *im, char *var, char *err, int optional)
{
    char *color;
    graph_desc_t *gdp=&im->gdes[im->gdes_c-1];

    color=strstr(var,"#");
    if (color==NULL) {
	if (optional==0) {
	    rrd_set_error("Found no color in %s",err);
	    return 0;
	}
	return 0;
    } else {
	int n=0;
	char *rest;
	gfx_color_t    col;

	rest=strstr(color,":");
	if (rest!=NULL)
	    n=rest-color;
	else
	    n=strlen(color);

	switch (n) {
	    case 7:
		sscanf(color,"#%6lx%n",&col,&n);
                col = (col << 8) + 0xff /* shift left by 8 */;
		if (n!=7) rrd_set_error("Color problem in %s",err);
		break;
	    case 9:
		sscanf(color,"#%8lx%n",&col,&n);
		if (n==9) break;
	    default:
		rrd_set_error("Color problem in %s",err);
	}
	if (rrd_test_error()) return 0;
	gdp->col = col;
	return n;
    }
}
int
rrd_graph_check_CF(image_desc_t *im, char *symname, char *err)
{
    if ((im->gdes[im->gdes_c-1].cf=cf_conv(symname))==-1) {
	rrd_set_error("Unknown CF '%s' in %s",symname,err);
	return -1;
    }
    return 0;
}
int
rrd_graph_legend(graph_desc_t *gdp, char *line)
{
    int i;

    i=scan_for_col(line,FMT_LEG_LEN,gdp->legend);

    return (strlen(&line[i])==0);
}


int bad_format(char *fmt) {
	char *ptr;
	int n=0;

	ptr = fmt;
	while (*ptr != '\0') {
		if (*ptr == '%') {ptr++;
			if (*ptr == '\0') return 1;
			while ((*ptr >= '0' && *ptr <= '9') || *ptr == '.') { 
				ptr++;
			}
			if (*ptr == '\0') return 1;
			if (*ptr == 'l') {
				ptr++;
				n++;
				if (*ptr == '\0') return 1;
				if (*ptr == 'e' || *ptr == 'f') { 
					ptr++; 
					} else { return 1; }
			}
			else if (*ptr == 's' || *ptr == 'S' || *ptr == '%') { ++ptr; }
			else { return 1; }
		} else {
			++ptr;
		}
	}
	return (n!=1);
}
int
vdef_parse(gdes,str)
struct graph_desc_t *gdes;
char *str;
{
    /* A VDEF currently is either "func" or "param,func"
     * so the parsing is rather simple.  Change if needed.
     */
    double	param;
    char	func[30];
    int		n;
    
    n=0;
    sscanf(str,"%le,%29[A-Z]%n",&param,func,&n);
    if (n==strlen(str)) { /* matched */
	;
    } else {
	n=0;
	sscanf(str,"%29[A-Z]%n",func,&n);
	if (n==strlen(str)) { /* matched */
	    param=DNAN;
	} else {
	    rrd_set_error("Unknown function string '%s' in VDEF '%s'"
		,str
		,gdes->vname
		);
	    return -1;
	}
    }
    if		(!strcmp("PERCENT",func)) gdes->vf.op = VDEF_PERCENT;
    else if	(!strcmp("MAXIMUM",func)) gdes->vf.op = VDEF_MAXIMUM;
    else if	(!strcmp("AVERAGE",func)) gdes->vf.op = VDEF_AVERAGE;
    else if	(!strcmp("MINIMUM",func)) gdes->vf.op = VDEF_MINIMUM;
    else if	(!strcmp("TOTAL",  func)) gdes->vf.op = VDEF_TOTAL;
    else if	(!strcmp("FIRST",  func)) gdes->vf.op = VDEF_FIRST;
    else if	(!strcmp("LAST",   func)) gdes->vf.op = VDEF_LAST;
    else {
	rrd_set_error("Unknown function '%s' in VDEF '%s'\n"
	    ,func
	    ,gdes->vname
	    );
	return -1;
    };

    switch (gdes->vf.op) {
	case VDEF_PERCENT:
	    if (isnan(param)) { /* no parameter given */
		rrd_set_error("Function '%s' needs parameter in VDEF '%s'\n"
		    ,func
		    ,gdes->vname
		    );
		return -1;
	    };
	    if (param>=0.0 && param<=100.0) {
		gdes->vf.param = param;
		gdes->vf.val   = DNAN;	/* undefined */
		gdes->vf.when  = 0;	/* undefined */
	    } else {
		rrd_set_error("Parameter '%f' out of range in VDEF '%s'\n"
		    ,param
		    ,gdes->vname
		    );
		return -1;
	    };
	    break;
	case VDEF_MAXIMUM:
	case VDEF_AVERAGE:
	case VDEF_MINIMUM:
	case VDEF_TOTAL:
	case VDEF_FIRST:
	case VDEF_LAST:
	    if (isnan(param)) {
		gdes->vf.param = DNAN;
		gdes->vf.val   = DNAN;
		gdes->vf.when  = 0;
	    } else {
		rrd_set_error("Function '%s' needs no parameter in VDEF '%s'\n"
		    ,func
		    ,gdes->vname
		    );
		return -1;
	    };
	    break;
    };
    return 0;
}
int
vdef_calc(im,gdi)
image_desc_t *im;
int gdi;
{
    graph_desc_t	*src,*dst;
    rrd_value_t		*data;
    long		step,steps;

    dst = &im->gdes[gdi];
    src = &im->gdes[dst->vidx];
    data = src->data + src->ds;
    steps = (src->end - src->start) / src->step;

#if 0
printf("DEBUG: start == %lu, end == %lu, %lu steps\n"
    ,src->start
    ,src->end
    ,steps
    );
#endif

    switch (dst->vf.op) {
	case VDEF_PERCENT: {
		rrd_value_t *	array;
		int		field;


		if ((array = malloc(steps*sizeof(double)))==NULL) {
		    rrd_set_error("malloc VDEV_PERCENT");
		    return -1;
		}
		for (step=0;step < steps; step++) {
		    array[step]=data[step*src->ds_cnt];
		}
		qsort(array,step,sizeof(double),vdef_percent_compar);

		field = (steps-1)*dst->vf.param/100;
		dst->vf.val  = array[field];
		dst->vf.when = 0;	/* no time component */
#if 0
for(step=0;step<steps;step++)
printf("DEBUG: %3li:%10.2f %c\n",step,array[step],step==field?'*':' ');
#endif
	    }
	    break;
	case VDEF_MAXIMUM:
	    step=0;
	    while (step != steps && isnan(data[step*src->ds_cnt])) step++;
	    if (step == steps) {
		dst->vf.val  = DNAN;
		dst->vf.when = 0;
	    } else {
		dst->vf.val  = data[step*src->ds_cnt];
		dst->vf.when = src->start + (step+1)*src->step;
	    }
	    while (step != steps) {
		if (finite(data[step*src->ds_cnt])) {
		    if (data[step*src->ds_cnt] > dst->vf.val) {
			dst->vf.val  = data[step*src->ds_cnt];
			dst->vf.when = src->start + (step+1)*src->step;
		    }
		}
		step++;
	    }
	    break;
	case VDEF_TOTAL:
	case VDEF_AVERAGE: {
	    int cnt=0;
	    double sum=0.0;
	    for (step=0;step<steps;step++) {
		if (finite(data[step*src->ds_cnt])) {
		    sum += data[step*src->ds_cnt];
		    cnt ++;
		};
	    }
	    if (cnt) {
		if (dst->vf.op == VDEF_TOTAL) {
		    dst->vf.val  = sum*src->step;
		    dst->vf.when = cnt*src->step;	/* not really "when" */
		} else {
		    dst->vf.val = sum/cnt;
		    dst->vf.when = 0;	/* no time component */
		};
	    } else {
		dst->vf.val  = DNAN;
		dst->vf.when = 0;
	    }
	    }
	    break;
	case VDEF_MINIMUM:
	    step=0;
	    while (step != steps && isnan(data[step*src->ds_cnt])) step++;
	    if (step == steps) {
		dst->vf.val  = DNAN;
		dst->vf.when = 0;
	    } else {
		dst->vf.val  = data[step*src->ds_cnt];
		dst->vf.when = src->start + (step+1)*src->step;
	    }
	    while (step != steps) {
		if (finite(data[step*src->ds_cnt])) {
		    if (data[step*src->ds_cnt] < dst->vf.val) {
			dst->vf.val  = data[step*src->ds_cnt];
			dst->vf.when = src->start + (step+1)*src->step;
		    }
		}
		step++;
	    }
	    break;
	case VDEF_FIRST:
	    /* The time value returned here is one step before the
	     * actual time value.  This is the start of the first
	     * non-NaN interval.
	     */
	    step=0;
	    while (step != steps && isnan(data[step*src->ds_cnt])) step++;
	    if (step == steps) { /* all entries were NaN */
		dst->vf.val  = DNAN;
		dst->vf.when = 0;
	    } else {
		dst->vf.val  = data[step*src->ds_cnt];
		dst->vf.when = src->start + step*src->step;
	    }
	    break;
	case VDEF_LAST:
	    /* The time value returned here is the
	     * actual time value.  This is the end of the last
	     * non-NaN interval.
	     */
	    step=steps-1;
	    while (step >= 0 && isnan(data[step*src->ds_cnt])) step--;
	    if (step < 0) { /* all entries were NaN */
		dst->vf.val  = DNAN;
		dst->vf.when = 0;
	    } else {
		dst->vf.val  = data[step*src->ds_cnt];
		dst->vf.when = src->start + (step+1)*src->step;
	    }
	    break;
    }
    return 0;
}

/* NaN < -INF < finite_values < INF */
int
vdef_percent_compar(a,b)
const void *a,*b;
{
    /* Equality is not returned; this doesn't hurt except
     * (maybe) for a little performance.
     */

    /* First catch NaN values. They are smallest */
    if (isnan( *(double *)a )) return -1;
    if (isnan( *(double *)b )) return  1;

    /* NaN doesn't reach this part so INF and -INF are extremes.
     * The sign from isinf() is compatible with the sign we return
     */
    if (isinf( *(double *)a )) return isinf( *(double *)a );
    if (isinf( *(double *)b )) return isinf( *(double *)b );

    /* If we reach this, both values must be finite */
    if ( *(double *)a < *(double *)b ) return -1; else return 1;
}
