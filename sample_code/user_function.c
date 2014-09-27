#ifdef __cplusplus
extern "C"{
#endif 

#include <math.h>

#ifdef __cplusplus
};
#endif 

#include "cv.h"
#include "highgui.h"
#include "alcon2011.h"

// estimation function for user development 
void user_function
(
 IMAGE image,                   // target image  
 HAND* hands                    // hand data
 )
{  

  //--- begin user code  ---//

  int i = 0;
  unsigned int width  = image.width;
  unsigned int height = image.height;
  CvMat* gray_img = cvCreateMat(height, width, CV_8UC1);
  CvMat* skin_img = cvCreateMat(height, width, CV_8UC1);
  CvSeq* contours = NULL;
  CvMemStorage* storage = cvCreateMemStorage(0);
  CvSeq*    hand_contour[2]; 
  CvPoint   hand_pos[2];
  CvMoments hand_moments[2];
  int hand_shape[2];

  // 0. ppm -> cvMat //
  CvMat* src = cvCreateMatHeader(height, width, CV_8UC3);
  cvSetData(src, image.data, width*3);  

  // 1. detect hands //
  
  // 1-1. detect skin-color region //
  cvCvtColor(src, gray_img, CV_RGB2GRAY);
  cvThreshold(gray_img, skin_img, 250.0, 255.0, CV_THRESH_BINARY_INV); 
  cvReleaseMat(&gray_img);

  // 1-2. detect contours of skin-color region //
  cvFindContours(skin_img, storage, &contours, sizeof(CvContour), 
		 CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0) );
  if(contours==NULL || contours->h_next==NULL){
    cvClearMemStorage( storage );
    return;
  }

  // 1-3. pick up 2 largest regions -> hands //
  {
  double area_max = 0;
  CvSeq* c = NULL;
  for(c = contours; c != NULL; c = c->h_next){
    CvMoments m;
    cvMoments( c, &m, 0 );
    if(m.m00>area_max){
      area_max = m.m00;
      hand_moments[0] = m;
      hand_contour[0] = c;
    }
  }
  area_max = 0;
  for(c = contours; c != NULL; c = c->h_next){
    CvMoments m;
    cvMoments( c, &m, 0 );
    if(m.m00>area_max && c != hand_contour[0]){
    area_max = m.m00;
      hand_moments[1] = m;
      hand_contour[1] = c;
    }
  }
  for(i=0 ; i<2 ; i++) hand_pos[i] = cvPoint( (int)(hand_moments[i].m10/hand_moments[i].m00),
					      (int)(hand_moments[i].m01/hand_moments[i].m00) );
  }


  // 2. recognize shape of the hands //
  // 0 : stone / 1 : scissors / 2 : paper 
  for(i=0; i<2; i++){
    double hand_contour_length = fabs(cvArcLength(hand_contour[i], CV_WHOLE_SEQ, -1));
    double comp = hand_contour_length/hand_moments[i].m00;
    hand_shape[i] = 1;
    if( comp < 0.060 ) hand_shape[i] = 0; 
    if( comp > 0.080 ) hand_shape[i] = 2;
  }
  cvClearMemStorage( storage );

  // 3. judge //
  if( hand_shape[0] == hand_shape[1] ){ 
    for(i=0;i<2;i++) set_result(hands, hand_pos[i].x, hand_pos[i].y, RESULT_EVEN);
  }else{
    if( abs( hand_shape[0] - hand_shape[1] ) == 1 ){
      if( hand_shape[0] > hand_shape[1] ){
	set_result(hands, hand_pos[0].x, hand_pos[0].y, RESULT_LOSE);
	set_result(hands, hand_pos[1].x, hand_pos[1].y, RESULT_WIN);
    }
      else{
	set_result(hands, hand_pos[0].x, hand_pos[0].y, RESULT_WIN);
	set_result(hands, hand_pos[1].x, hand_pos[1].y, RESULT_LOSE);
      }
      
    }else{
      if( hand_shape[0] > hand_shape[1] ){ 
	set_result(hands, hand_pos[0].x, hand_pos[0].y, RESULT_WIN);
	set_result(hands, hand_pos[1].x, hand_pos[1].y, RESULT_LOSE);
      }
      else{
	set_result(hands, hand_pos[0].x, hand_pos[0].y, RESULT_LOSE);
	set_result(hands, hand_pos[1].x, hand_pos[1].y, RESULT_WIN);
      }
    }
  }
  
  //--- end user code  ---//

  return;
}
