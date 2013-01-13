#ifdef __cplusplus
extern "C"{
#endif 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
};
#endif

#ifdef _WIN32
#pragma warning( disable : 4996 )
#endif


#include "alcon2011.h"

//// definitions ////

// maximum number of hands in a target image 
#define MAX_HAND_NUM 20

// size of overlayed marker 
#define marker_size 15


//// variables ////

// the number of hands //
unsigned int num_hands   = 0;
unsigned int num_hands_t = 0;




// ---------------------------------------------------------------------------------------------------
// recognition result setting //
void set_result
(
 HAND* hands,                   // pointer of hands data 
 unsigned int x,                // detected hand position-x
 unsigned int y,                // detected hand position-y
 RESULT r                       // recognition result
 )
{
  hands[num_hands].x  = x;
  hands[num_hands].y  = y;
  hands[num_hands].result = r;
 
  if(++num_hands>=MAX_HAND_NUM) fprintf(stderr,"[ Warning ] too many hands!\n");
}


// ---------------------------------------------------------------------------------------------------
// calculate score of the result //
void calc_score
(
 HAND* hands,                   // recognized hands
 HAND_TRUTH* hands_t,           // ground truth hands
 int* detect_score,             // detection score (for return)
 int* recog_score               // recognition score (for return)
 )
{
  unsigned int i = 0;
  unsigned int j = 0;

  // hand corresponding
  for(i=0;i<num_hands;++i){
    double min_length = -1.0;
    unsigned int x = hands[i].x;
    unsigned int y = hands[i].y;
    hands[i].IDc = -1;
    for(j=0;j<num_hands_t;++j){
      if(x>hands_t[j].left && x<hands_t[j].left+hands_t[j].width &&
   y>hands_t[j].top  && y<hands_t[j].top +hands_t[j].height){	
	double length = sqrt(pow((double)((int)hands_t[j].gx-(int)x),2.0)+pow((double)((int)hands_t[j].gy-(int)y),2.0));
	if(min_length<0 || length<min_length){ 
	  min_length   = length;
	  hands[i].IDc = (int)j;
	  hands[i].length = length;
	}
      } 
      
    }}  
  
  // score
  (*detect_score) = 0;
  (*recog_score)  = 0;
  for(i=0;i<num_hands_t;++i){
    int point = 1;
    RESULT result = NO_RESULT;
    double min_length = -1.0;
    for(j=0;j<num_hands;++j){
      if(hands[j].IDc == (int)i){
	// detection score
	(*detect_score) += point;
	point = -1;
	// for recognition score
	if(min_length<0 || hands[j].length<min_length){
	  min_length = hands[j].length;
	  result = hands[j].result;
	} 
      }
    }
    // recognition score
    if(result == hands_t[i].result) (*recog_score)++;
  }
    
}


// ---------------------------------------------------------------------------------------------------
// read ground truth hands //
int read_ground_truth_hands
(
 const char* filename,          // filename of ground truth hands data
 HAND_TRUTH* hands_t            // ground truth hands
 )
{
  FILE* fp = NULL;
  char res;	

  if((fp=fopen(filename, "r"))==NULL){
    fprintf(stderr, "[ error ] can not open a ground truth data : %s\n", filename);
    return -1;
  }
  
  while(fscanf(fp, "%d\t%d\t%d\t%d\t%c\n", 
	       &(hands_t[num_hands_t].left), 
	       &(hands_t[num_hands_t].top), 
	       &(hands_t[num_hands_t].width), 
	       &(hands_t[num_hands_t].height),
	       &res) !=EOF){
    switch(res){
    case 'W':
      hands_t[num_hands_t].result = RESULT_WIN;
      break;
    case 'L':
      hands_t[num_hands_t].result = RESULT_LOSE;
      break;
    case 'E':
      hands_t[num_hands_t].result = RESULT_EVEN;
	break;
    default:
      fprintf(stderr,"[ error ] given pattern does not match to janken result\n");
      return -1;
    }
    hands_t[num_hands_t].gx = hands_t[num_hands_t].left + (int)(hands_t[num_hands_t].width/2);
    hands_t[num_hands_t].gy = hands_t[num_hands_t].top  + (int)(hands_t[num_hands_t].height/2);
    
    if(++num_hands_t>=MAX_HAND_NUM) fprintf(stderr,"[ Warning ] too many hands!\n");
  }

  return 0;
}


// ---------------------------------------------------------------------------------------------------
// output resutls to the target IO 
int output_result
(
 FILE* io,                      // target IO 
 HAND* hands,                   // recognized hands
 HAND_TRUTH* hands_t            // ground truth hands
 )
{
  unsigned int i = 0;

  if(io==NULL){
    fprintf(stderr,"[ error ] null IO is given\n");
    return -1;
  }

  if(num_hands==0){
    fprintf(stderr,"[ error ] hand data are empty\n");
    return -1;
  }

  fprintf(io, "x\ty\trecog.\n");
    
  for(i=0;i<num_hands;++i){
    if(hands[i].result != NO_RESULT){
      fprintf(io, "%d\t%d", hands[i].x, hands[i].y);
      switch(hands[i].result){
      case RESULT_WIN:
	fprintf(io, "\tWIN");
	break;
      case RESULT_LOSE:
	fprintf(io, "\tLOSE");
	break;
      case RESULT_EVEN:
	fprintf(io, "\tEVEN");
	break;
      default:
	break;
      }
      fprintf(io,"\n");
    }
  }
   
  if(num_hands_t>0){
    int detect_score = 0;
    int recog_score = 0;
    calc_score(hands, hands_t, &detect_score, &recog_score);
    fprintf(io, "\ndetection score   : %d\n", detect_score);
    fprintf(io, "recognition score : %d\n", recog_score);
    fprintf(io, "------------------------\n");
    fprintf(io, "total score       : %d\n\n", detect_score+recog_score);      
  }
    
        
  return 0;
}


// ---------------------------------------------------------------------------------------------------
// output results to the target file
int output_result_to_file
(
 const char* filename_out,      // name of the target file 
 HAND* hands,                   // recognized hands
 HAND_TRUTH* hands_t            // ground truth hands
 )
{
  FILE* fp = NULL;
  if((fp=fopen(filename_out,"w"))==NULL){
    fprintf(stderr, "[ error ] can not open a target file : %s\n", filename_out);
    return -1;
  }

  return output_result(fp, hands, hands_t);
}


// ---------------------------------------------------------------------------------------------------
// color from result //
void color_from_result
(
 RESULT r,                      // hand result
 unsigned char col[]            // corresponding color
 )
{
  col[0] = 0;
  col[1] = 0;
  col[2] = 0;

  switch(r){
  case RESULT_WIN:
    col[0] = 0;
    col[1] = 0;
    col[2] = 255;
    break;
  case RESULT_LOSE:
    col[0] = 255;
    col[1] = 0;
    col[2] = 0;
    break;
  case RESULT_EVEN:
    col[0] = 0;
    col[1] = 255;
    col[2] = 0;
    break;
  default:
    break;
  }   
}


// ---------------------------------------------------------------------------------------------------
// draw point //
void draw_point
(
 IMAGE image,                   // image data
 int x,                         // position-x
 int y,                         // position-y
 unsigned char col[]            // target color
 )
{
  if(x<0 || x>(int)image.width-1)  return;
  if(y<0 || y>(int)image.height-1) return;
	
  {	
	int ref = (y*image.width+x)*3;
	int k	= 0;
	for(k=0;k<3;++k) image.data[ref+k] = col[k];
  }	
}


// ---------------------------------------------------------------------------------------------------
// output results as an overlayed image //
int output_result_image
(
 const char* filename_out,      // name of the target file, must be '~.ppm' 
 IMAGE image,                   // target image  
 HAND* hands,                   // recognized hands
 HAND_TRUTH* hands_t            // ground truth hands
 )
{
  FILE* fp = NULL;
  unsigned int i = 0;
  int x = 0;
  int y = 0;

  // copy image
  IMAGE dimage;
  dimage.width  = image.width;
  dimage.height = image.height;
  dimage.data = (unsigned char*)malloc(dimage.width*dimage.height*3);
  memcpy(dimage.data, image.data, dimage.width*dimage.height*3);

  // draw marker (recognized hands)
  for(i=0;i<num_hands;++i){
    unsigned char col[3];
    color_from_result(hands[i].result, col);
    for(y=-marker_size+(int)hands[i].y; y<=marker_size+(int)hands[i].y;++y){
      for(x=-marker_size+(int)hands[i].x; x<=marker_size+(int)hands[i].x;++x){
	draw_point(dimage, x, y, col);
      }}
  }

  // draw rectangle (ground truth hands)
  for(i=0;i<num_hands_t;++i){
    unsigned char col[3];
    color_from_result(hands_t[i].result, col);
    for(x=0; x<=(int)hands_t[i].width; ++x){
      draw_point(dimage, hands_t[i].left+x, hands_t[i].top, col);
      draw_point(dimage, hands_t[i].left+x, hands_t[i].top+hands_t[i].height, col);
    }
    for(y=0; y<=(int)hands_t[i].height; ++y){
      draw_point(dimage, hands_t[i].left,                  hands_t[i].top+y, col);
      draw_point(dimage, hands_t[i].left+hands_t[i].width, hands_t[i].top+y, col);
    }
  }

  // write to file

  if((fp=fopen(filename_out,"wb"))==NULL){
    fprintf(stderr,"[ error ] can not open the target file : %s\n", filename_out);
  }else{
    fprintf(fp, "P6\n%d %d\n255\n", dimage.width, dimage.height);
    fwrite(dimage.data, 1, dimage.width*dimage.height*3, fp);
    fclose(fp);
  }
  free(dimage.data);
  
  return 0;
}




// ---------------------------------------------------------------------------------------------------
// disp help //
void disp_help(void)
{
  printf("Usage : alcon2011 [options]\n");
  printf("  -s:filename \t indicates an input image file as 'filename'\n");
  printf("  -d:filename \t recognized results are written into a 'filename' file instead of the terminal \n");
  printf("  -i:filename \t recognized results are saved as a 'filename' image\n");
  printf("  -g:filename \t calculates and displays recognition scores compared with ground truth data in 'filaname'\n");
}


// ---------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{

  int i = 0;

  // variables 
  char* filename_img_in  = NULL;
  char* filename_img_out = NULL;
  char* filename_out     = NULL;
  char* filename_g       = NULL;
  HAND hands[MAX_HAND_NUM];
  HAND_TRUTH hands_t[MAX_HAND_NUM];
  IMAGE image;

  // help 
  if(argc<2){
    disp_help();
    return 0;
  }

  // option analysis
  for(i=1;i<argc;++i){
    if(strlen(argv[i])>3 && argv[i][0]=='-' && argv[i][2]==':'){
      
      switch(argv[i][1]){
      case 's' :
	if(!filename_img_in)  filename_img_in  = strdup(&argv[i][3]);
	break;
      case 'd' :
	if(!filename_out)     filename_out     = strdup(&argv[i][3]);
	break;
      case 'i' :
	if(!filename_img_out) filename_img_out = strdup(&argv[i][3]);
	break;
      case 'g' :
	if(!filename_g)       filename_g       = strdup(&argv[i][3]);
	break;	
      default:
	break;
      }
    }}
  
  // input image 
  if(filename_img_in){
    printf("[ report ] Image is read from '%s'.\n", filename_img_in);
  }else{
    fprintf(stderr,"[ error ] input image is not given.\n");
    return -1;
  }

  // read image file 
  {
  FILE* fp = NULL;
  unsigned int size = 0;
  if((fp=fopen(filename_img_in, "rb"))==NULL){
    fprintf(stderr,"[ error ] can not open an image file : %s.\n", filename_img_in);
    return -1;
  }
  if(fscanf(fp, "P6\n%d %d\n255\n", &image.width, &image.height)!=2){
    fprintf(stderr,"[ error ] image file format error\n");
    return -1;
  }
  size = image.width*image.height*3; 
  image.data = (unsigned char*)malloc(size);
  if(fread(image.data, 1, size, fp) != size){
    fprintf(stderr,"[ error ] image file size is not correct\n");
    free(image.data);
    return -1;
  }
  fclose(fp);
  }

  // read ground truth
  if(filename_g){
    if(read_ground_truth_hands(filename_g,hands_t)<0){
      free(image.data);
      return -1;
    }
  }
      
    
  // estimate win / lose / even for each hand
  user_function(image,hands);

  
  // output results 
  if(filename_out){
    output_result_to_file(filename_out, hands, hands_t);
  }else{
    output_result(stdout, hands, hands_t);
  }
  if(filename_img_out) output_result_image(filename_img_out, image, hands, hands_t);


  // clean
  free(image.data);

  return 0;
}

