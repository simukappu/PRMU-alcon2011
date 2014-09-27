

//// structures ////

// result for each hand 
typedef enum RESULT{
  NO_RESULT,
  RESULT_WIN,
  RESULT_LOSE,
  RESULT_EVEN
} RESULT;


// data structure for each hand
typedef struct HAND{
  unsigned int x;               // hand position-x 
  unsigned int y;               // hand position-y
  RESULT result;                // hand result
  int IDc;                      // corresponding truth-hand
  double length;                // length to center of truth-hand 
}HAND;


// data structure for ground truth
typedef struct HAND_TRUTH{
  unsigned int left;            // hand rectangle-left 
  unsigned int top;             // hand rectangle-top
  unsigned int width;           // hand rectangle-width 
  unsigned int height;          // hand rectangle-height
  unsigned int gx,gy;           // gravity center of the hand rectangle
  RESULT result;                // hand result
}HAND_TRUTH;


// data structure for an image 
typedef struct IMAGE{
  unsigned int width;           // size of the image (width)
  unsigned int height;          // size of the image (height)
  unsigned char *data;          // image data 
}IMAGE;



//// functions ////

// estimation function for user development 
void user_function
(
 IMAGE image,                   // target image  
 HAND* hands                    // hand data
 );


// recognition result setting //
void set_result
(
 HAND* hands,                   // pointer of hands data 
 unsigned int x,                // detected hand position-x
 unsigned int y,                // detected hand position-y
 RESULT r                       // recognition result
 );
