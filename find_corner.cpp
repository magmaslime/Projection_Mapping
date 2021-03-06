#include"headers.h"

STimerList st;

using namespace std;
using namespace cv;

#define SRC_IMAGE "C:/Users/koyajima/Pictures/Projection_Mapping/id0102.png"

///////////////////   fullscreen.cpp   /////////////////////

typedef struct {
    int dispno;
    int arraysz;
    MONITORINFO *minfo;
} MonList;

bool CALLBACK
monCallback(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM dwData);

MonList *
createMonList(HDC hdc, LPCRECT lprcClip);

int
releaseMonList(MonList **ml);

typedef struct {
    int width;
    int height;
    int x0; // x position of top-left corner
    int y0; // y position of top-left corner
    int monitor_id;
} ScreenInfo;


int
getScreenInfo(int monitor_id, ScreenInfo *si);

int
undecorateWindow(const char *win);

int
setWindowFullscreen(const char *win, ScreenInfo *si);

int
setWindowTranslucent(const char *win, unsigned char alpha);

int
setWindowChromaKeyed(const char *win, CvScalar color);


////////////  main  /////////////

int main(int argc, char *argv[]){

//      Fullscreenの初期設定
	cv::VideoCapture cap(1);
	if(!cap.isOpened())
	{
		cout << "error:camera not found" << endl;
		return -1;
	}
	cap.set(CV_CAP_PROP_FRAME_WIDTH, 640);
	cap.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
	//cv::namedWindow("Capture", CV_WINDOW_AUTOSIZE|CV_WINDOW_FREERATIO);
	cv::Mat prjcted = cv::imread(SRC_IMAGE);
	if(prjcted.empty())
	{
		cout << "error:image not found" << endl;
		return -1;
	}
	cv::namedWindow("projection");
	undecorateWindow("projection"); 
	ScreenInfo si;
    getScreenInfo(1, &si);
	setWindowFullscreen("projection", &si);
    cv::resize(prjcted, prjcted, cv::Size(si.width, si.height),0, 0, cv::INTER_CUBIC);

//       輪郭検出の初期設定
	
	Mat src, gray, bin;
	vector< vector<cv::Point> > contours;
	cv::vector<cv::vector<cv::Point>> squares;
	cv::vector<cv::vector<cv::Point> > poly;
	const int idx=-1;
	const int thick=2;
	std::vector<cv::Vec4i> hierarchy;
	std::vector<cv::Point> approx;

//        ARtoolkitの初期設定

	// ARToolKitPlus側
	ARToolKitPlus::TrackerMultiMarker *tracker;

	// tracker を作る
	tracker = new ARToolKitPlus::TrackerMultiMarkerImpl<6,6,12,32,64>(WIDTH, HEIGHT);
			// <marker_width, marker_height, sampling step, max num in cfg, max num in scene>
	tracker->init("calibration_data.cal", "marker_dummy.cfg", NEAR_LEN, FAR_LEN);

	// tracker オブジェクトの各種設定（通常このままでよい）
	tracker->setPixelFormat(ARToolKitPlus::PIXEL_FORMAT_BGR);
	tracker->setBorderWidth(0.125f); // thin
	tracker->activateAutoThreshold(true);
	tracker->setUndistortionMode(ARToolKitPlus::UNDIST_NONE);
	tracker->setPoseEstimator(ARToolKitPlus::POSE_ESTIMATOR_RPP);
	tracker->setMarkerMode(ARToolKitPlus::MARKER_ID_BCH);

	// マーカ座標系からカメラ座標系への変換行列を格納するための配列
	//float m34[ 3 ][ 4 ];
	// マーカ中心のオフセット（通常このままでよい）
	float c[ 2 ] = { 0.0f, 0.0f };
	// マーカの一辺のサイズ [mm] （実測値）
	float w = 59.0f;
	cv::Point2f corner[4];
	int radius = 5; // キーポイントの半径

	cv::Point2f scree[4];

	while(1) {
		st.laptime(0);

		cap >> src;  // キャプチャ
		//グレースケール変換
		cv::cvtColor(src, gray, CV_BGR2GRAY); 

////////////////////////////////////   輪郭検出   //////////////////////////////// 

		//2値化
		cv::threshold(gray, bin, 100, 255, cv::THRESH_BINARY|cv::THRESH_OTSU); 

		//収縮・膨張
		cv::erode(bin, bin, cv::Mat(), cv::Point(-1,-1), 3); 
		cv::erode(bin, bin, cv::Mat(), cv::Point(-1,-1), 3);
		cv::dilate(bin, bin, cv::Mat(), cv::Point(-1,-1), 1);
		cv::dilate(bin, bin, cv::Mat(), cv::Point(-1,-1), 1);

		//輪郭検出
		cv::findContours (bin, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);
		//cv::drawContours (src, contours, -1, cv::Scalar(100), 2, 8);
		cout << "loop" << endl;
		//検出した輪郭ごとに見て回る
		for (unsigned int j = 0; j < contours.size(); j++){
		approx = contours[j];
		cout << "aprrox" << endl;
		if(approx.size()==4){
			cout << "size4" << endl;
		}
		if(hierarchy[j][2] != -1){
			cout << "hie" << endl;
		}
			//輪郭を近似する
			cv::approxPolyDP(contours[j], approx, cv::arcLength(contours[j], true)*0.02, true);
			//頂点が4つの場合
			if (approx.size() == 4 && hierarchy[j][2] != -1)
			{
				//4つの頂点を描く
				for (unsigned int k = 0; k < approx.size(); k++){
					cv::circle(src, approx[k], 5,  CV_RGB(255,0,0), 2, 8, 0);
				}
			}
		}
		
/////////////////////////   ARtoolkit     ///////////////////////////

		// 画像中からマーカを検出
		tracker->calc((unsigned char *)(src.data));
		
		// 検出されたマーカの数
		unsigned int num = tracker->getNumDetectedMarkers();

		// 検出された各マーカ情報を見ていく
		for(unsigned int m = 0; m < num; m++){

			// i 番目のマーカ情報を取得（id 降順に並べられている様子）
			ARToolKitPlus::ARMarkerInfo a = tracker->getDetectedMarker(m);

			//取得した座標
			if(a.id==1)
			{
				corner[0].x = a.vertex[0][0];//右下
				corner[0].y = a.vertex[0][1];
				corner[1].x = a.vertex[1][0];
				corner[1].y = a.vertex[1][1];
				corner[2].x = a.vertex[2][0];
				corner[2].y = a.vertex[2][1];
				corner[3].x = a.vertex[3][0];
				corner[3].y = a.vertex[3][1];
			}
			if(a.id==2){
				scree[0].x = a.vertex[0][0];
				scree[0].y = a.vertex[0][1];
			}
			if(a.id==3){
				scree[1].x = a.vertex[0][0];
				scree[11].y = a.vertex[0][1];
			}
			if(a.id==4){
				scree[2].x = a.vertex[0][0];
				scree[2].y = a.vertex[0][1];
			}
			if(a.id==5){
				scree[3].x = a.vertex[0][0];
				scree[3].y = a.vertex[0][1];
			}
		}
		///////4点を描く
		if(corner[0].x != 0){
			for(int l = 0; l < 4; l++){
				cv::circle(src, corner[l], radius,  CV_RGB(0,0,255), 5, 8, 0);
			}
		}
		if(scree[0].x != 0){
			for(int l = 0; l < 4; l++){
				cv::circle(src, scree[l], radius,  CV_RGB(0,0,255), 5, 8, 0);
			}
		}
		/*表示*/
		cv::imshow("projection", prjcted);
		cv::imshow("Capture", src);
		//cv::imshow("bin", bin);

		if (waitKey(2) > 0) 
		{
            break;
        }

		}//ループ終わり

		return 0;
}