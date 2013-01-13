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

//最大手数
#define MAX_HAND  20
//最大欠損数
#define MAX_DEFECTS	30

// estimation function for user development 
void user_function
(
 IMAGE image,                   // target image  
 HAND* hands                    // hand data
 )
{  

  //--- begin user code  ---//

	int Width  = image.width;
	int Height = image.height;


	/*Parameters*/

	//最大・最小の手数（除外の処理を行うかを決定）
	const int MaxHandNum = MAX_HAND;
	const int MinHandNum = 3;

	//手の大きさの規定
	const double SmallestHandRect = Width * Height / 100.0;
	const double LargestHandRect = Width * Height * 0.3;
	const double SumLargestHandBox = Width * Height * 0.6;

	//色相の閾値
	const int HThresholdLow = 0;
	const int HThresholdHigh = 20;

	//彩度の閾値
	int SThresholdLow = 35;
	int SThresholdHigh = 255;
	const int SThresholdStep = 15;
	const int SMaxLowThreshold = 100;
	const int SMinHighThreshold = 100;
	const int SThresholdAvg = 80;

	//距離画像の閾値
	const double DistThreshold = 0.1;

	//腕の除去
	const double AspectRatioThreshold = 2.0;
	const double NewAspectRatio = 1.8;
	const int HandEndMargin = 5;

	//面積による除外
	const double AreaRatioThreshold = 6.0;

	//色の分布による除外
	const double HueWeight = 4.0;
	const double MinMahalanobisThreshold = 25.0;

	//欠損の判定
	const double AngleThreshold = CV_PI * 2.0/5.0;
	const double AngleThreshold2 = CV_PI / 2.0;
	const double DeltaAngleThreshold = CV_PI / 5.0;
	const double LengthRatioThreshold = 0.7;
	const double MinHandScaleDefectsRatio = 4.0;

	/*End of Parameters*/


	const int Rock = 0;
	const int Scissors = 1;
	const int Paper = 2;

	CvSize ImageSize = cvSize(Width, Height);
	CvPoint CenterPos = cvPoint(Width/2, Height/2);

	CvMat* OriginalImage = cvCreateMatHeader(Height, Width, CV_8UC3);
	CvMat* HSVImage = cvCreateMat(Height, Width, CV_8UC3);
	IplImage* ExtractImage = cvCreateImage(ImageSize, IPL_DEPTH_8U, 1);
	IplImage* DistImage = cvCreateImage(ImageSize, IPL_DEPTH_32F, 1);
	IplImage* DistThresholdImage = cvCreateImage(ImageSize, IPL_DEPTH_8U, 1);
	IplImage* DistTempImage = cvCreateImage(ImageSize, IPL_DEPTH_32F, 1);
	CvMemStorage* Storage = cvCreateMemStorage(0);

	CvSeq* Contour = NULL;
	CvSeq* Contours = NULL;
	CvSeq* ConvexHull = NULL;
	CvSeq* Defects = NULL;
	CvConvexityDefect* DefdectsPoint= NULL;
	CvRect Rect;
	CvPoint2D32f BoxCornerPoint[4];
	CvPoint MaxDistPoint, MiddlePoint, DepthPoint, CenterPoint;
	CvPoint LinePoint1, LinePoint2;

	CvSeq* HandContour[MAX_HAND];
	CvPoint HandPos[MAX_HAND];
	CvRect HandRect[MAX_HAND];
	CvBox2D HandBox[MAX_HAND];
	int HandRectArea[MAX_HAND];
	int HandShape[MAX_HAND];
	int ElemNum[MAX_HAND];

	int i = 0, j = 0, x = 0, y = 0;
	int Count = 0;
	int HandNum = 0;
	int HandCount = 0;
	int RockCount = 0;
	int ScissorsCount = 0;
	int PaperCount = 0;
	int DefectsDepthCount = 0;
	int DefectsDepthCount2 = 0;
	int SSum = 0;
	int RectArea = 0;
	int MinHandRectArea = 0;
	int MinRectAreaHand = 0;
	int BoxWidth = 0;
	int BoxHeight = 0;
	int HandPosMinX = Width, HandPosMaxX = 0;
	int HandPosMinY = Height, HandPosMaxY = 0;
	int HSVSum[3];
	int MaxAreaHand;

	unsigned char HLookUpTable[256];
	unsigned char SLookUpTable[256];

	double HandScale = 0.0;
	double Area = 0.0;
	double MaxArea = 0.0;
	double SumArea = 0.0;
	double AspectRatio = 0.0;
	double MaxHandArea = 0.0;
	double MaxValue = 0.0;
	double MinBoxPosX = 0.0;
	double MaxBoxPosX = 0.0;
	double MinBoxPosY = 0.0;
	double MaxBoxPosY = 0.0;
	double SAvg = 0.0;
	double DeltaAngle = 0.0;
	double LengthRatio = 0.0;
	double DefectsDepthDeltaAngle[MAX_DEFECTS];
	double DefectsDepthDeltaAngle2[MAX_DEFECTS];
	double DefectsLendth[MAX_DEFECTS];
	double DefectsLendth2[MAX_DEFECTS];
	double CalcBuf = 0.0;
	double Mahalanobis[MAX_HAND-1][MAX_HAND];
	double HSVAvg[3][MAX_HAND];
	double HSVDiv[3][MAX_HAND];
	double HSVDivSum[3];

	bool IfIncorrectHand = false;
	bool IfIncorrectHandOnce = false;
	bool IfUp = false, IfDown = false;


	//処理開始

	//元画像を変換
	cvSetData(OriginalImage, image.data, Width*3);
	cvCvtColor(OriginalImage, HSVImage, CV_RGB2HSV);

	//色相のルックアップテーブル
	for(i=0; i<256; i++)
	{
		if ((HThresholdLow <= i) && (i <= HThresholdHigh))
			HLookUpTable[i] = 255;
		else
			HLookUpTable[i] = 0;
	}

	while(1)
	{
		HandNum = 0;

		//彩度のルックアップテーブル
		for(i=0; i<256; i++)
		{
			if ((SThresholdLow <= i) && (i <= SThresholdHigh))
				SLookUpTable[i] = 255;
			else
				SLookUpTable[i] = 0;
		}

		//色の二値化による輪郭抽出
		for(y = 0; y<Height; y++)
		{
			unsigned char* ptr1 = (unsigned char*)ExtractImage->imageData + y * ExtractImage->widthStep;
			const unsigned char* ptr2 = (const unsigned char*)HSVImage->data.ptr + y * HSVImage->step;
			for(x=0; x<Width; x++)
			{
				*ptr1 = HLookUpTable[ptr2[0]] & SLookUpTable[ptr2[1]];
				ptr1++;
				ptr2 += 3;
			}
		}
		cvFindContours(ExtractImage, Storage, &Contours, sizeof(CvContour), 
			CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0, 0));

		if(Contours == NULL)
		{
			//輪郭が見つからなかった場合
			//printf("No contours\n");
			return;
		}

		for(Contour = Contours; Contour != NULL; Contour = Contour->h_next)
		{
			//矩形が規定の大きさにあれば手に登録
			Rect = cvBoundingRect(Contour);
			RectArea = Rect.width * Rect.height;
			if((double)RectArea > LargestHandRect)
			{
				IfIncorrectHand = true;
				break;
			}
			if((double)RectArea > SmallestHandRect)
			{
				if(HandNum < MaxHandNum)
				{
					HandContour[HandNum] = Contour;
					HandRect[HandNum] = Rect;
					HandRectArea[HandNum] = RectArea;
					HandNum++;
				}
				//最大の手数を超えたときは，大きいものを優先する
				else
				{
					MinHandRectArea = Width * Height;
					for(i=0; i<HandNum; i++)
					{
						if(HandRectArea[HandNum] < MinHandRectArea)
						{
							MinHandRectArea = HandRectArea[HandNum];
							MinRectAreaHand = i;
						}
					}
					if(MinHandRectArea < RectArea)
					{
						HandContour[MinRectAreaHand] = Contour;
						HandRect[MinRectAreaHand] = Rect;
						HandRectArea[MinRectAreaHand] = RectArea;
					}
				}
			}
		}

		SumArea = 0.0;
		MaxArea = 0.0;
		if(!IfIncorrectHand)
		{
			for(i=0; i<HandNum; i++)
			{
				RectArea = HandRect[i].width * HandRect[i].height;
				SumArea += RectArea;
				if(MaxArea > RectArea)
				{
					MaxArea = RectArea;
					Rect = HandRect[i];
				}
			}
		}
		if(SumArea > SumLargestHandBox)
		{
			IfIncorrectHand = true;
		}

		if(IfIncorrectHand)
		{
			//彩度の閾値の上限と下限どちらを狭めるかを色分布の平均から決定
			if(!IfIncorrectHandOnce)
			{
				Count = 0;
				SSum = 0;

				for(y = Rect.y; y<Rect.y+Rect.height; y++)
				{
					const unsigned char* ptr1 = (const unsigned char*)ExtractImage->imageData + y * ExtractImage->widthStep + Rect.x;;
					const unsigned char* ptr2 = (const unsigned char*)HSVImage->data.ptr + y * HSVImage->step + 3 * Rect.x;
					for(x=Rect.x; x<Rect.x+Rect.width; x++)
					{
						if(*ptr1)
						{
							SSum += ptr2[1];
							Count++;
						}
						ptr1++;
						ptr2 += 3;
					}
				}
				SAvg = (double)SSum / (double)Count;

				if(SAvg > SThresholdAvg) IfUp = true;
				else IfDown = true;
				IfIncorrectHandOnce = true;
			}

			//彩度の閾値を変更して再抽出
			if(IfUp) SThresholdHigh -= SThresholdStep;
			else if(IfDown) SThresholdLow += SThresholdStep;

			if(SThresholdLow > SMaxLowThreshold || SThresholdHigh < SMinHighThreshold)
			{
				//輪郭が見つからなかった場合
				//printf("No Contours at Max threshold\n");
				return;
			}
			//printf("Extract Again (Low:%3d, High:%3d)\n", SThresholdLow, SThresholdHigh);

			IfIncorrectHand = false;
			Contours = NULL;
			continue;
		}
		else break;
	}

	//printf("Extract : Threshold of Saturation (Low:%3d - High:%3d)\n", SThresholdLow, SThresholdHigh);
	//printf("Num. of Hands : %d (Extract)\n\n", HandNum);


	cvRectangle(ExtractImage, cvPoint(0, 0), cvPoint(Width, Height), cvScalarAll(0), 2);
	cvDistTransform(ExtractImage, DistImage);

	//距離画像による輪郭取得
	for(i=0; i<HandNum; i++)
	{
		cvSetImageROI(DistImage, HandRect[i]);
		cvSetImageROI(DistThresholdImage, HandRect[i]);
		cvSetImageROI(DistTempImage, HandRect[i]);

		//最大距離を持つ点の値で正規化
		cvCopy(DistImage, DistTempImage);
		cvNormalize(DistTempImage, DistTempImage, 0.0, 1.0, CV_MINMAX);

		//二値化して輪郭抽出
		cvThreshold(DistTempImage, DistThresholdImage, DistThreshold, 255, CV_THRESH_BINARY);
		cvFindContours(DistThresholdImage, Storage, &Contours, sizeof(CvContour), 
			CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0) );

		if(Contours==NULL)
		{
			HandContour[i] = NULL;
			continue;
		}

		//最大面積を持つ輪郭を手に登録する
		MaxArea = 0.0;
		for(Contour = Contours; Contour != NULL; Contour = Contour->h_next)
		{
			Area = fabs(cvContourArea(Contour, CV_WHOLE_SEQ));
			if(Area > MaxArea)
			{
				MaxArea = Area;
				HandContour[i] = Contour;
			}
		}

		//最大距離を持つ点を手の座標とする
		MaxValue = 0.0;
		for(y = 0; y<HandRect[i].height; y++)
		{
			//4 byte pointer
			const float* ptr = (const float*)DistImage->imageData + (y+HandRect[i].y) * DistImage->widthStep/4 + HandRect[i].x;
			for(x=0; x<HandRect[i].width; x++)
			{
				if(*ptr > MaxValue)
				{
					if(cvPointPolygonTest(HandContour[i], cvPoint2D32f(x, y), 0) > 0)
					{
						MaxDistPoint.x = x;
						MaxDistPoint.y = y;
						MaxValue = *ptr;
					}
				}
				ptr++;
			}
		}
		HandPos[i].x = MaxDistPoint.x + HandRect[i].x;
		HandPos[i].y = MaxDistPoint.y + HandRect[i].y;

		//最小矩形（手のスケール）
		HandBox[i] = cvMinAreaRect2(HandContour[i]);
		cvBoxPoints(HandBox[i], BoxCornerPoint);

		//腕の除去
		AspectRatio = MAX(HandBox[i].size.height, HandBox[i].size.width) / MIN(HandBox[i].size.height, HandBox[i].size.width);
		if(AspectRatio > AspectRatioThreshold)
		{
			MinBoxPosX = MIN(MIN(BoxCornerPoint[0].x, BoxCornerPoint[1].x), MIN(BoxCornerPoint[2].x, BoxCornerPoint[3].x)) + HandRect[i].x;
			MaxBoxPosX = MAX(MAX(BoxCornerPoint[0].x, BoxCornerPoint[1].x), MAX(BoxCornerPoint[2].x, BoxCornerPoint[3].x)) + HandRect[i].x;
			MinBoxPosY = MIN(MIN(BoxCornerPoint[0].y, BoxCornerPoint[1].y), MIN(BoxCornerPoint[2].y, BoxCornerPoint[3].y)) + HandRect[i].y;
			MaxBoxPosY = MAX(MAX(BoxCornerPoint[0].y, BoxCornerPoint[1].y), MAX(BoxCornerPoint[2].y, BoxCornerPoint[3].y)) + HandRect[i].y;
			if(MinBoxPosX < HandEndMargin || MaxBoxPosX > Width-HandEndMargin || MinBoxPosY < HandEndMargin || MaxBoxPosY > Height-HandEndMargin)
			{
				//printf("Remove Arm from Hand [%d]\n", i+1);
				BoxWidth = (int)HandBox[i].size.width;
				BoxHeight = (int)HandBox[i].size.height;
				cvSetImageROI(ExtractImage, HandRect[i]);

				if(HandBox[i].size.height > HandBox[i].size.width)
				{
					if((BoxCornerPoint[0].x+HandRect[i].x-CenterPos.x)*(BoxCornerPoint[0].x+HandRect[i].x-CenterPos.x)
						+ (BoxCornerPoint[0].y+HandRect[i].y-CenterPos.y)*(BoxCornerPoint[0].y+HandRect[i].y-CenterPos.y)
						> (BoxCornerPoint[1].x+HandRect[i].x-CenterPos.x)*(BoxCornerPoint[1].x+HandRect[i].x-CenterPos.x)
						+ (BoxCornerPoint[1].y+HandRect[i].y-CenterPos.y)*(BoxCornerPoint[1].y+HandRect[i].y-CenterPos.y))
					{
						LinePoint1.x = (int)(NewAspectRatio*BoxWidth*BoxCornerPoint[0].x + (BoxHeight-NewAspectRatio*BoxWidth)*BoxCornerPoint[1].x) / BoxHeight;
						LinePoint1.y = (int)(NewAspectRatio*BoxWidth*BoxCornerPoint[0].y + (BoxHeight-NewAspectRatio*BoxWidth)*BoxCornerPoint[1].y) / BoxHeight;
						LinePoint2.x = (int)(NewAspectRatio*BoxWidth*BoxCornerPoint[3].x + (BoxHeight-NewAspectRatio*BoxWidth)*BoxCornerPoint[2].x) / BoxHeight;
						LinePoint2.y = (int)(NewAspectRatio*BoxWidth*BoxCornerPoint[3].y + (BoxHeight-NewAspectRatio*BoxWidth)*BoxCornerPoint[2].y) / BoxHeight;
						cvLine(ExtractImage, LinePoint1, cvPointFrom32f(BoxCornerPoint[3]), cvScalarAll(0), 3);
						cvLine(ExtractImage, LinePoint2, cvPointFrom32f(BoxCornerPoint[0]), cvScalarAll(0), 3);
					}
					else
					{
						LinePoint1.x = (int)(NewAspectRatio*BoxWidth*BoxCornerPoint[1].x + (BoxHeight-NewAspectRatio*BoxWidth)*BoxCornerPoint[0].x) / BoxHeight;
						LinePoint1.y = (int)(NewAspectRatio*BoxWidth*BoxCornerPoint[1].y + (BoxHeight-NewAspectRatio*BoxWidth)*BoxCornerPoint[0].y) / BoxHeight;
						LinePoint2.x = (int)(NewAspectRatio*BoxWidth*BoxCornerPoint[2].x + (BoxHeight-NewAspectRatio*BoxWidth)*BoxCornerPoint[3].x) / BoxHeight;
						LinePoint2.y = (int)(NewAspectRatio*BoxWidth*BoxCornerPoint[2].y + (BoxHeight-NewAspectRatio*BoxWidth)*BoxCornerPoint[3].y) / BoxHeight;
						cvLine(ExtractImage, LinePoint1, cvPointFrom32f(BoxCornerPoint[2]), cvScalarAll(0), 3);
						cvLine(ExtractImage, LinePoint2, cvPointFrom32f(BoxCornerPoint[1]), cvScalarAll(0), 3);
					}
				}
				else
				{
					if((BoxCornerPoint[1].x+HandRect[i].x-CenterPos.x)*(BoxCornerPoint[1].x+HandRect[i].x-CenterPos.x)
						+ (BoxCornerPoint[1].y+HandRect[i].y-CenterPos.y)*(BoxCornerPoint[1].y+HandRect[i].y-CenterPos.y)
						> (BoxCornerPoint[2].x+HandRect[i].x-CenterPos.x)*(BoxCornerPoint[2].x+HandRect[i].x-CenterPos.x)
						+ (BoxCornerPoint[2].y+HandRect[i].y-CenterPos.y)*(BoxCornerPoint[2].y+HandRect[i].y-CenterPos.y))
					{
						LinePoint1.x = (int)(NewAspectRatio*BoxHeight*BoxCornerPoint[1].x + (BoxWidth-NewAspectRatio*BoxHeight)*BoxCornerPoint[2].x) / BoxWidth;
						LinePoint1.y = (int)(NewAspectRatio*BoxHeight*BoxCornerPoint[1].y + (BoxWidth-NewAspectRatio*BoxHeight)*BoxCornerPoint[2].y) / BoxWidth;
						LinePoint2.x = (int)(NewAspectRatio*BoxHeight*BoxCornerPoint[0].x + (BoxWidth-NewAspectRatio*BoxHeight)*BoxCornerPoint[3].x) / BoxWidth;
						LinePoint2.y = (int)(NewAspectRatio*BoxHeight*BoxCornerPoint[0].y + (BoxWidth-NewAspectRatio*BoxHeight)*BoxCornerPoint[3].y) / BoxWidth;
						cvLine(ExtractImage, LinePoint1, cvPointFrom32f(BoxCornerPoint[0]), cvScalarAll(0), 3);
						cvLine(ExtractImage, LinePoint2, cvPointFrom32f(BoxCornerPoint[1]), cvScalarAll(0), 3);
					}
					else
					{
						LinePoint1.x = (int)(NewAspectRatio*BoxHeight*BoxCornerPoint[2].x + (BoxWidth-NewAspectRatio*BoxHeight)*BoxCornerPoint[1].x) / BoxWidth;
						LinePoint1.y = (int)(NewAspectRatio*BoxHeight*BoxCornerPoint[2].y + (BoxWidth-NewAspectRatio*BoxHeight)*BoxCornerPoint[1].y) / BoxWidth;
						LinePoint2.x = (int)(NewAspectRatio*BoxHeight*BoxCornerPoint[3].x + (BoxWidth-NewAspectRatio*BoxHeight)*BoxCornerPoint[0].x) / BoxWidth;
						LinePoint2.y = (int)(NewAspectRatio*BoxHeight*BoxCornerPoint[3].y + (BoxWidth-NewAspectRatio*BoxHeight)*BoxCornerPoint[0].y) / BoxWidth;
						cvLine(ExtractImage, LinePoint1, cvPointFrom32f(BoxCornerPoint[3]), cvScalarAll(0), 3);
						cvLine(ExtractImage, LinePoint2, cvPointFrom32f(BoxCornerPoint[2]), cvScalarAll(0), 3);
					}
				}
				cvLine(ExtractImage, LinePoint1, LinePoint2, cvScalarAll(0), 3);

				//最大距離を持つ点の値で正規化
				cvDistTransform(ExtractImage, DistImage);
				cvCopy(DistImage, DistTempImage);
				cvNormalize(DistTempImage, DistTempImage, 0.0, 1.0, CV_MINMAX);

				//二値化して輪郭抽出
				cvThreshold(DistTempImage, DistThresholdImage, DistThreshold, 255, CV_THRESH_BINARY);
				cvFindContours(DistThresholdImage, Storage, &Contours, sizeof(CvContour), 
					CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0) );

				if(Contours==NULL)
				{
					continue;
				}

				//最大面積を持つ輪郭を手に登録する
				MaxArea = 0.0;
				for(Contour = Contours; Contour != NULL; Contour = Contour->h_next)
				{
					Area = fabs(cvContourArea(Contour, CV_WHOLE_SEQ));
					if(Area > MaxArea)
					{
						MaxArea = Area;
						HandContour[i] = Contour;
						HandBox[i] = cvMinAreaRect2(HandContour[i]);
					}
				}

				//最大距離を持つ点を手の座標とする
				MaxValue = 0.0;
				for(y = 0; y<HandRect[i].height; y++)
				{
					//4 byte pointer
					const float* ptr = (const float*)DistImage->imageData + (y+HandRect[i].y) * DistImage->widthStep / 4;
					ptr += HandRect[i].x;
					for(x=0; x<HandRect[i].width; x++)
					{
						if(*ptr > MaxValue)
						{
							if(cvPointPolygonTest(HandContour[i], cvPoint2D32f(x, y), 0) > 0)
							{
								MaxDistPoint.x = x;
								MaxDistPoint.y = y;
								MaxValue = *ptr;
							}
						}
						ptr++;
					}
				}

				HandPos[i].x = MaxDistPoint.x + HandRect[i].x;
				HandPos[i].y = MaxDistPoint.y + HandRect[i].y;

				//最小矩形（手のスケール）
				HandBox[i] = cvMinAreaRect2(HandContour[i]);
			}
		}
	}
	//printf("\n");

	HandCount = 0;
	for(i=0; i<HandNum; i++)
	{
		if(HandContour[i] != NULL)
		{
			HandContour[HandCount] = HandContour[i];
			HandRect[HandCount] = HandRect[i];
			HandBox[HandCount] = HandBox[i];
			HandPos[HandCount] = HandPos[i];
			HandCount++;
		}
	}
	HandNum = HandCount;
	//printf("Num. of Hands : %d (Extract from Distance Image)\n", HandNum);

	//抽出輪郭の大きさの割合が基準値より小さい場合は除外する
	if(HandNum > MinHandNum)
	{
		HandCount = 0;
		for(i=0; i<HandNum; i++)
		{
			if(HandBox[i].size.width * HandBox[i].size.height > MaxHandArea)
			{
				MaxHandArea = HandBox[i].size.width * HandBox[i].size.height;
				MaxAreaHand = i;
			}
		}
		for(i=0; i<HandNum; i++)
		{
			if(HandBox[i].size.width * HandBox[i].size.height * AreaRatioThreshold > MaxHandArea)
			{
				HandContour[HandCount] = HandContour[i];
				HandRect[HandCount] = HandRect[i];
				HandBox[HandCount] = HandBox[i];
				HandPos[HandCount] = HandPos[i];
				HandCount++;
			}
		}
		HandNum = HandCount;
	}
	//printf("Num. of Hands : %d (Area Exception)\n", HandNum);

	//領域間の色分布の距離が基準値より大きい場合は除外する
	//輪郭内の平均と分散を計算
	if(HandNum > MinHandNum)
	{
		for(i=0; i<HandNum; i++)
		{
			ElemNum[i] = 0;
			HSVSum[0] = 0;
			HSVSum[1] = 0;
			HSVSum[2] = 0;
			HSVDivSum[0] = 0.0;
			HSVDivSum[1] = 0.0;
			HSVDivSum[2] = 0.0;

			//画素値の平均
			for(y=HandRect[i].y; y<HandRect[i].y+HandRect[i].height; y++)
			{
				const unsigned char* ptr1 = (const unsigned char*)DistThresholdImage->imageData + y * DistThresholdImage->widthStep + HandRect[i].x;
				const unsigned char* ptr2 = (const unsigned char*)HSVImage->data.ptr + y * HSVImage->step + 3 * HandRect[i].x;
				for(x=HandRect[i].x; x<HandRect[i].x+HandRect[i].width; x++)
				{
					if(*ptr1)
					{
						HSVSum[0] += ptr2[0] % 180;
						HSVSum[1] += ptr2[1];
						HSVSum[2] += ptr2[2];
						ElemNum[i]++;
					}
					ptr1++;
					ptr2 += 3;
				}
			}
			HSVAvg[0][i] = (double)HSVSum[0] / (double)ElemNum[i];
			HSVAvg[1][i] = (double)HSVSum[1] / (double)ElemNum[i];
			HSVAvg[2][i] = (double)HSVSum[2] / (double)ElemNum[i];

			//画素値の分散
			for(y = HandRect[i].y; y<HandRect[i].y+HandRect[i].height; y++)
			{
				const unsigned char* ptr1 = (const unsigned char*)DistThresholdImage->imageData + y * DistThresholdImage->widthStep + HandRect[i].x;
				const unsigned char* ptr2 = (const unsigned char*)HSVImage->data.ptr + y * HSVImage->step + 3 * HandRect[i].x;
				for(x=HandRect[i].x; x<HandRect[i].x+HandRect[i].width; x++)
				{
					if(*ptr1)
					{
						CalcBuf = fabs((double)ptr2[0] - HSVAvg[0][i]);
						CalcBuf = MIN(CalcBuf, 180-CalcBuf);
						HSVDivSum[0] += CalcBuf * CalcBuf;
						CalcBuf = fabs((double)ptr2[1] - HSVAvg[1][i]);
						CalcBuf = MIN(CalcBuf, 255-CalcBuf);
						HSVDivSum[1] += CalcBuf * CalcBuf;
						CalcBuf = fabs((double)ptr2[2] - HSVAvg[2][i]);
						CalcBuf = MIN(CalcBuf, 255-CalcBuf);
						HSVDivSum[2] += CalcBuf * CalcBuf;
					}
					ptr1++;
					ptr2 += 3;
				}
			}
			HSVDiv[0][i] = HSVDivSum[0] / (double)ElemNum[i];
			HSVDiv[1][i] = HSVDivSum[1] / (double)ElemNum[i];
			HSVDiv[2][i] = HSVDivSum[2] / (double)ElemNum[i];
		}

		//領域間の色分布のマハラノビス距離
		for(i=0; i<HandNum-1; i++)
		{
			for(j=i+1; j<HandNum; j++)
			{
				Mahalanobis[i][j] = 0.0;
				Mahalanobis[i][j] += HueWeight * (HSVAvg[0][i] - HSVAvg[0][j]) * (HSVAvg[0][i] - HSVAvg[0][j]) * (ElemNum[i] + ElemNum[j]) / (ElemNum[i] * HSVDiv[0][i] + ElemNum[j] * HSVDiv[0][j]);
				Mahalanobis[i][j] += (HSVAvg[1][i] - HSVAvg[1][j]) * (HSVAvg[1][i] - HSVAvg[1][j]) * (ElemNum[i] + ElemNum[j]) / (ElemNum[i] * HSVDiv[1][i] + ElemNum[j] * HSVDiv[1][j]);
				Mahalanobis[i][j] += (HSVAvg[2][i] - HSVAvg[2][j]) * (HSVAvg[2][i] - HSVAvg[2][j]) * (ElemNum[i] + ElemNum[j]) / (ElemNum[i] * HSVDiv[2][i] + ElemNum[j] * HSVDiv[2][j]);
			}
		}

		//マハラノビス距離を用いた閾値処理による除外
		HandCount = 0;
		for(i=0; i<HandNum; i++)
		{
			if(i < MaxAreaHand)
			{
				if(Mahalanobis[i][MaxAreaHand] < MinMahalanobisThreshold)
				{
					HandContour[HandCount] = HandContour[i];
					HandRect[HandCount] = HandRect[i];
					HandBox[HandCount] = HandBox[i];
					HandPos[HandCount] = HandPos[i];
					HandCount++;
				}
			}
			else if(MaxAreaHand < i)
			{
				if(Mahalanobis[MaxAreaHand][i] < MinMahalanobisThreshold)
				{
					HandContour[HandCount] = HandContour[i];
					HandRect[HandCount] = HandRect[i];
					HandBox[HandCount] = HandBox[i];
					HandPos[HandCount] = HandPos[i];
					HandCount++;
				}
			}
			else
			{
				HandContour[HandCount] = HandContour[i];
				HandRect[HandCount] = HandRect[i];
				HandBox[HandCount] = HandBox[i];
				HandPos[HandCount] = HandPos[i];
				HandCount++;
			}
		}
		HandNum = HandCount;
	}
	//printf("Num. of Hands : %d (Color Exception)\n\n", HandNum);

	//じゃんけん中心の推定
	if(HandNum == 2)
	{
		if(abs(HandPos[0].x - HandPos[1].x) < Width / 8)
		{
			CenterPoint.x = Width / 2;
			CenterPoint.y = (HandPos[0].y + HandPos[1].y / 2);
		}
		else if(abs(HandPos[0].y - HandPos[1].y) < Width / 8)
		{
			CenterPoint.x = (HandPos[0].x + HandPos[1].x) / 2;
			CenterPoint.y = Height / 2;
		}
		else
		{
			CenterPoint.x =CenterPos.x;
			CenterPoint.y = CenterPos.y;
		}
	}
	else
	{
		for(i=0; i<HandNum; i++)
		{
			if(HandPos[i].x < HandPosMinX) HandPosMinX = HandPos[i].x;
			if(HandPos[i].x > HandPosMaxX) HandPosMaxX = HandPos[i].x;
			if(HandPos[i].y < HandPosMinY) HandPosMinY = HandPos[i].y;
			if(HandPos[i].y > HandPosMaxY) HandPosMaxY = HandPos[i].y;
		}
		CenterPoint.x = (HandPosMinX + HandPosMaxX) / 2;
		CenterPoint.y = (HandPosMinY + HandPosMaxY) / 2;
	}

	//手の形状の判定処理
	for(i=0; i<HandNum; i++)
	{
		//凸包と欠損を取得
		ConvexHull = cvConvexHull2(HandContour[i], Storage);
		Defects = cvConvexityDefects(HandContour[i], ConvexHull, Storage);

		//スケールに対して一定以上の大きさを持つ欠損
		DefectsDepthCount = 0;
		DefectsDepthCount2 = 0;
		HandScale = MIN(HandBox[i].size.height, HandBox[i].size.width) / MinHandScaleDefectsRatio;
		for(j=0; j < Defects->total; j++)
		{
			DefdectsPoint = CV_GET_SEQ_ELEM(CvConvexityDefect, Defects, j);

			if(DefdectsPoint->depth > HandScale)
			{
				MiddlePoint.x = (DefdectsPoint->start->x + DefdectsPoint->end->x) / 2 + HandRect[i].x;
				MiddlePoint.y = (DefdectsPoint->start->y + DefdectsPoint->end->y) / 2 + HandRect[i].y;
				DepthPoint.x = DefdectsPoint->depth_point->x + HandRect[i].x;
				DepthPoint.y = DefdectsPoint->depth_point->y + HandRect[i].y;

				double Dist1 = (CenterPoint.x - DepthPoint.x) * (CenterPoint.x - DepthPoint.x)
					+ (CenterPoint.y - DepthPoint.y) * (CenterPoint.y - DepthPoint.y);
				double Dist2 = (MiddlePoint.x - DepthPoint.x) * (MiddlePoint.x - DepthPoint.x)
					+ (MiddlePoint.y - DepthPoint.y) * (MiddlePoint.y - DepthPoint.y);
				double Dist3 = (CenterPoint.x - MiddlePoint.x) * (CenterPoint.x - MiddlePoint.x)
					+ (CenterPoint.y - MiddlePoint.y) * (CenterPoint.y - MiddlePoint.y);
				double Dist4 = (CenterPoint.x - HandPos[i].x) * (CenterPoint.x - HandPos[i].x)
					+ (CenterPoint.y - HandPos[i].y) * (CenterPoint.y - HandPos[i].y);
				double Angle = acos((Dist1 + Dist2 - Dist3) / (2 * sqrt(Dist1) * sqrt(Dist2)));

				//手の重心よりもじゃんけん中心に近い欠損
				if(Dist3 < Dist4)
				{
					//欠損とじゃんけん中心への方向がなす角度を計算
					if(Angle < AngleThreshold)
					{
						DefectsDepthDeltaAngle[DefectsDepthCount] = atan2((double)(MiddlePoint.y-DepthPoint.y), (double)(MiddlePoint.x-DepthPoint.x));
						DefectsLendth[DefectsDepthCount] = sqrt(Dist2);
						DefectsDepthCount++;
					}
					else if(Angle < AngleThreshold2)
					{
						DefectsDepthDeltaAngle2[DefectsDepthCount2] = atan2((double)(MiddlePoint.y-DepthPoint.y), (double)(MiddlePoint.x-DepthPoint.x));
						DefectsLendth2[DefectsDepthCount2] = sqrt(Dist2);
						DefectsDepthCount2++;
					}
				}
			}
		}

		//数えた欠損の数と幾何学的関係により手の形状を判定
		if(DefectsDepthCount > 2)
		{
			//printf("Hand [%d] : Paper\n", i+1);
			HandShape[i] = Paper;
			PaperCount++;
		}
		else if(DefectsDepthCount == 2)
		{
			double DeltaAngle = fabs(DefectsDepthDeltaAngle[0] - DefectsDepthDeltaAngle[1]);
			double LengthRatio = MIN(DefectsLendth[0], DefectsLendth[1]) / MAX(DefectsLendth[0], DefectsLendth[1]);
			DeltaAngle = MIN(DeltaAngle, 2*CV_PI-DeltaAngle);
			if(DefectsDepthCount2 > 0 || (DeltaAngle < DeltaAngleThreshold && LengthRatio > LengthRatioThreshold))
			{
				//printf("Hand [%d] : Paper\n", i+1);
				HandShape[i] = Paper;
				PaperCount++;
			}
			else
			{
				//printf("Hand [%d] : Scissors\n", i+1);
				HandShape[i] = Scissors;
				ScissorsCount++;
			}
		}
		else if(DefectsDepthCount == 1)
		{
			if(DefectsDepthCount2 > 1)
			{
				//printf("Hand [%d] : Paper\n", i+1);
				HandShape[i] = Paper;
				PaperCount++;
			}
			else
			{
				if(DefectsDepthCount2 > 0)
				{
					DeltaAngle = fabs(DefectsDepthDeltaAngle[0] - DefectsDepthDeltaAngle2[0]);
					LengthRatio = MIN(DefectsLendth[0], DefectsLendth2[0]) / MAX(DefectsLendth[0], DefectsLendth2[0]);
					DeltaAngle = MIN(DeltaAngle, 2*CV_PI-DeltaAngle);
					if(DeltaAngle < DeltaAngleThreshold && LengthRatio > LengthRatioThreshold)
					{
						//printf("Hand [%d] : Paper\n", i+1);
						HandShape[i] = Paper;
						PaperCount++;
					}
					else
					{
						//printf("Hand [%d] : Scissors\n", i+1);
						HandShape[i] = Scissors;
						ScissorsCount++;
					}
				}
				else
				{
					HandShape[i] = Scissors;
					//printf("Hand [%d] : Scissors\n", i+1);
					ScissorsCount++;
				}
			}
		}
		else
		{
			//printf("Hand [%d] : Rock\n", i+1);
			HandShape[i] = Rock;
			RockCount++;
		}
	}
	//printf("\n");

	//勝敗判定
	if(HandNum == 1)
	{
		//Win (Paper > Rock)
		set_result(hands, HandPos[0].x, HandPos[0].y, RESULT_WIN);
	}
	else if(HandNum>1)
	{
		//No Rock
		if(!RockCount)
		{
			if(!ScissorsCount)
			{
				//All Paper
				//Even
				for(i=0; i<HandNum; i++)
				{
					set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_EVEN);
					//printf("Hand [%d] : Even\n", i+1);
				}
			}
			else if(!PaperCount)
			{
				//All Scissors
				//Even
				for(i=0; i<HandNum; i++)
				{
					set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_EVEN);
					//printf("Hand [%d] : Even\n", i+1);
				}
			}
			else
			{
				//Scissors and Paper
				for(i=0; i<HandNum; i++)
				{
					//Scissors Win
					//Paper Lose
					if(HandShape[i] == Scissors)
					{
						set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_WIN);
						//printf("Hand [%d] : Win\n", i+1);
					}
					else
					{
						set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_LOSE);
						//printf("Hand [%d] : Lose\n", i+1);
					}
				}
			}
		}

		//No Scissors
		else if(!ScissorsCount)
		{
			if(!PaperCount)
			{
				//All Rock
				//Even
				for(i=0; i<HandNum; i++)
				{
					set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_EVEN);
					//printf("Hand [%d] : Even\n", i+1);
				}
			}
			else
			{
				//Rock and Paper
				for(i=0; i<HandNum; i++)
				{
					//Paper Win
					//Rock Lose
					if(HandShape[i] == Paper)
					{
						set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_WIN);
						//printf("Hand [%d] : Win\n", i+1);
					}
					else
					{
						set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_LOSE);
						//printf("Hand [%d] : Lose\n", i+1);
					}
				}
			}
		}

		//No Paper
		else if(!PaperCount)
		{
			//Rock and Scissors
			for(i=0; i<HandNum; i++)
			{
				//Rock Win
				//Scissors Lose
				if(HandShape[i] == Rock)
				{
					set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_WIN);
					//printf("Hand [%d] : Win\n", i+1);
				}
				else
				{
					set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_LOSE);
					//printf("Hand [%d] : Lose\n", i+1);
				}
			}
		}

		//Rock, Scissors and Paper
		else
		{
			//Even
			for(i=0; i<HandNum; i++)
			{
				set_result(hands, HandPos[i].x, HandPos[i].y, RESULT_EVEN);
				//printf("Hand [%d] : Even\n", i+1);
			}
		}
	}
	//printf("\n");

	cvReleaseMat(&OriginalImage);
	cvReleaseMat(&HSVImage);
	cvReleaseImage(&DistImage);
	cvReleaseImage(&DistThresholdImage);
	cvReleaseImage(&DistTempImage);
	cvClearMemStorage(Storage);


  //--- end user code  ---//

  return;
}
