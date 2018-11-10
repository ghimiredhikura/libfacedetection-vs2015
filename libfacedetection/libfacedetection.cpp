// libfacedetection.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "opencv2/opencv.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "facedetectcnn-dll.h"
#include <vector>

#include <Windows.h>
#include <stdint.h> // portable: uint64_t   MSVC: __int64 

//define the buffer size. Do not change the size!
#define DETECT_BUFFER_SIZE 0x20000

using namespace std;
using namespace cv;

enum DATASET { AFW, PASCAL_FACE, FDDB, WIDER_TEST, WIDER_VAL, UFDD };
enum MODE { WEBCAM, BENCHMARK_EVALUATION, IMAGE, IMAGE_LIST };

typedef struct _tagResult {
	cv::Rect r;
	float score;
}Result;


int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
	// Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
	// This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
	// until 00:00:00 January 1, 1970 
	static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

	SYSTEMTIME  system_time;
	FILETIME    file_time;
	uint64_t    time;

	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	time = ((uint64_t)file_time.dwLowDateTime);
	time += ((uint64_t)file_time.dwHighDateTime) << 32;

	tp->tv_sec = (long)((time - EPOCH) / 10000000L);
	tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
	return 0;
}

double what_time_is_it_now()
{
	struct timeval time;
	if (gettimeofday(&time, NULL)) {
		return 0;
	}
	return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

cv::RNG rng(0xFFFFFFFF);

std::vector<Result> faceDetection(cv::Mat image, int min_img_size = 10)
{
	std::vector<Result> m_results;
	if (image.empty())
	{
		return m_results;
	}

	int * pResults = NULL;
	//pBuffer is used in the detection functions.
	//If you call functions in multiple threads, please create one buffer for each thread!
	unsigned char * pBuffer = (unsigned char *)malloc(DETECT_BUFFER_SIZE);
	if (!pBuffer)
	{
		fprintf(stderr, "Can not alloc buffer.\n");
		return m_results;
	}

	///////////////////////////////////////////
	// CNN face detection 
	// Best detection rate
	//////////////////////////////////////////
	//!!! The input image must be a RGB one (three-channel)
	//!!! DO NOT RELEASE pResults !!!
	pResults = facedetect_cnn(pBuffer, (unsigned char*)(image.ptr(0)), image.cols, image.rows, (int)image.step);

	printf("%d faces detected.\n", (pResults ? *pResults : 0));
	//Mat result_cnn = image.clone();;
	//print the detection results
	for (int i = 0; i < (pResults ? *pResults : 0); i++)
	{
		short * p = ((short*)(pResults + 1)) + 142 * i;
		int x = p[0];
		int y = p[1];
		int w = p[2];
		int h = p[3];
		int neighbors = p[4];
		int angle = p[5];

		cv::Rect r1 = Rect(x, y, w, h);
		//float score = rng.uniform(90, 100);
		//score = score / 100.0;
		float score = float(neighbors) / 100.0;
		if (score > 1.0) score = 1.0;

		printf("face_rect=[%d, %d, %d, %d], neighbors=%d, angle=%d\n", x, y, w, h, neighbors, angle);
		rectangle(image, r1, Scalar(0, 255, 0), 2);

		Result result;
		result.r = r1;
		result.score = score;

		m_results.push_back(result);
	}

	free(pBuffer);

	return m_results;
}

void run_afw_pascal_fddb(DATASET m_data, std::string dataset_path)
{
	cout << m_data << endl;
	cout << dataset_path << endl;

	std::string str_img_file;
	if(m_data == AFW) str_img_file = "detections/AFW/afw_img_list.txt";
	else if (m_data == PASCAL_FACE) str_img_file = "detections/PASCAL/pascal_img_list.txt";
	else if (m_data == FDDB) str_img_file = "detections/FDDB/fddb_img_list.txt";	
	else return ;

	cout << str_img_file << endl;

	std::ifstream inFile(str_img_file.c_str(), std::ifstream::in);

	std::vector<string> image_list;
	std::string imname;
	while(std::getline(inFile, imname)) 
	{
		image_list.push_back(imname);
	}

	std::string str_out_file;
	if(m_data == AFW) str_out_file = "detections/AFW/afw_dets.txt";
	else if (m_data == PASCAL_FACE) str_out_file = "detections/PASCAL/pascal_dets.txt";
	else if (m_data == FDDB) str_out_file = "detections/FDDB/fddb_dets.txt";	

	std::ofstream outFile(str_out_file.c_str());
	// process each image one by one
	for(int i = 0; i < image_list.size(); i++)
	{
		std::string imname = image_list[i];
		std::string tempname = imname;

		if(m_data==AFW) imname = dataset_path+imname+".jpg";
		else if(m_data==PASCAL_FACE) imname = dataset_path+imname;
		else if(m_data==FDDB) imname = dataset_path+imname+".jpg";

		cout << "processing image " << i+1 << "/" << image_list.size() << " [" << imname.c_str() << "]" << endl;

		cv::Mat image = cv::imread(imname);

		std::vector<Result> detection_results = faceDetection(image);

		if(m_data!=FDDB) 
		{			
			for(int j=0;j<detection_results.size();j++) {
				outFile << tempname << " " << detection_results[j].score << " " << detection_results[j].r.x << " "
					<< detection_results[j].r.y << " " << detection_results[j].r.x+detection_results[j].r.width << " " 
					<< detection_results[j].r.y+detection_results[j].r.height << endl;		
			}			
		} else {
			
			outFile << tempname << "\n";
			outFile << detection_results.size() << "\n";
			for(int j =0; j < detection_results.size(); j++) {
				outFile << detection_results[j].r.x << " " << detection_results[j].r.y << " " 
				<< detection_results[j].r.width << " " << detection_results[j].r.height << " " << detection_results[j].score << "\n";
			}
		}

		imshow("test", image);

		waitKey(1);
	}
	outFile.close();
}

void run_wider(DATASET m_data, std::string dataset_path)
{
	std::string result_dir;
	if(m_data == WIDER_TEST) result_dir = "detections/WIDER/test/";
	else result_dir = "detections/WIDER/val/";

	std::string str_img_file;
	if(m_data == WIDER_TEST) str_img_file = "detections/WIDER/wider_test_list.txt";
	else if (m_data == WIDER_VAL) str_img_file = "detections/WIDER/wider_val_list.txt";
	else return;

	std::ifstream inFile(str_img_file.c_str(), std::ifstream::in);

	std::vector<string> image_list;
	while(!inFile.eof()) 
	{
		std::string str;
		inFile >> str;
		if(str=="") continue;
		image_list.push_back(str);
	}

	std::string wider_path = dataset_path;

	for(int i = 0; i < image_list.size(); i++)
	{
		std::string imname = wider_path+image_list[i];
		cout << imname << endl;
		cout << "processing image " << i+1 << "/" << image_list.size() << " [" << image_list[i].c_str() << "]" << endl;

		cv::Mat image = cv::imread(imname);

		//initial models without image's width or height
		std::vector<Result> detection_results = faceDetection(image);

		std::string txt_name = image_list[i];
		txt_name.replace(txt_name.end()-4, txt_name.end(), ".txt");
		txt_name = result_dir+txt_name;
	
		std::ofstream out_txt(txt_name.c_str());
		out_txt << image_list[i] << "\n";
		out_txt << detection_results.size() << "\n";
		for(int j =0; j < detection_results.size(); j++) {
			out_txt << detection_results[j].r.x << " " << detection_results[j].r.y << " " << detection_results[j].r.width << " " 
			<< detection_results[j].r.height << " " << detection_results[j].score << "\n";
		}
		out_txt.close();

		imshow("image", image);

		waitKey(30);
	}	
}

void run_ufdd(std::string dataset_path)
{
	std::string result_dir;
	result_dir = "detections/UFDD/";

	std::string str_img_file;
	str_img_file = "detections/UFDD/ufdd_img_list.txt";

	std::ifstream inFile(str_img_file.c_str(), std::ifstream::in);

	std::vector<string> image_list;
	while(!inFile.eof()) 
	{
		std::string str;
		inFile >> str;
		if(str=="") continue;
		image_list.push_back(str);
	}

	std::string ufdd_path = dataset_path;

	for(int i = 0; i < image_list.size(); i++)
	{
		std::string imname = ufdd_path+image_list[i]+".jpg";
		cout << imname << endl;
		cout << "processing image " << i+1 << "/" << image_list.size() << " [" << image_list[i].c_str() << "]" << endl;

		cv::Mat image = cv::imread(imname);

		std::vector<Result> detection_results = faceDetection(image);

		std::string txt_name = image_list[i]+".txt";
		txt_name = result_dir+txt_name;
	
		std::ofstream out_txt(txt_name.c_str());
		out_txt << image_list[i] << "\n";
		out_txt << detection_results.size() << "\n";
		for(int j =0; j < detection_results.size(); j++) {
			out_txt << detection_results[j].r.x << " " << detection_results[j].r.y << " " << detection_results[j].r.width << " " 
			<< detection_results[j].r.height << " " << detection_results[j].score << "\n";
		}
		out_txt.close();

		imshow("image", image);

		waitKey(1);
	}	
}

void run_webcam(int webcam_id)
{
	VideoCapture cap(webcam_id);
	if(!cap.isOpened()) {
		cout << "fail to open webcam!" << endl;
		return;
	}

	cv::Mat image;
	while(true) {
	
		cap >> image;

		if(image.empty()) break;
		
		double time_begin = what_time_is_it_now();
		//detect face by min_size(60)
		std::vector<Result> detection_results = faceDetection(image);		
		double time_now = what_time_is_it_now();
		double time_diff = time_now-time_begin;
		
		cout << "FPS: " << 1/time_diff << endl;

		imshow("result", image);
		if (waitKey(1) >= 0) break;		
	}
}

void run_image(std::string image_path)
{
	cv::Mat image = cv::imread(image_path.c_str());
	if(image.empty()) {
		cout << "Image not exist in the specified dir!";
		return;	
	} 

	std::vector<Result> results = faceDetection(image);		

	imshow("result", image);	
	waitKey(1);
	getchar();
}

void run_images(std::string image_path)
{
	std::vector<cv::String> img_list;
	glob(image_path+"*.jpg", img_list, false);

	size_t count = img_list.size();
	for(size_t i=0;i<count;i++) 
	{
		cv::Mat image = cv::imread(img_list[i].c_str());
		if(image.empty()) {
			cout << "Image not exist in the specified dir!";
			return;	
		} 
	
		std::vector<Result> detection_results = faceDetection(image);		
	
		imshow("result", image);	
		waitKey(1);
	}
} 

cv::String keys =
	"{ help h 		| | Print help message. }"
	"{ mode m 		| 0 | Select running mode: "
						"0: WEBCAM - get image streams from webcam, "
						"1: IMAGE - detect faces in single image, "
						"2: IMAGE_LIST - detect faces in set of images, "
						"3: BENCHMARK_EVALUATION - benchmark evaluation, results will be stored in 'detections' }"
	"{ webcam i 	| 0 | webcam id, if mode is 0 }"
	"{ dataset d 	| AFW | select dataset, if mode is 3:"
						   "AFW: afw dataset, "
						   "PASCAL_FACE: pascal dataset, "
						   "FDDB: fddb dataset, "
						   "WIDER_VAL: wider validation set, "
						   "WIDER_TEST: wider test set, "
						   "UFDD: ufdd valudation set }"
	"{ path p 		| | Path to image file or image list dir or benchmark dataset }";

int main(int argc, char ** argv)
{	
	cv::CommandLineParser parser(argc, argv, keys);

	if (argc == 1 || parser.has("help"))
	{
		parser.printMessage();
		return 0;
	}

	MODE m_mode;
	int mode = parser.get<int>("mode");

	if (mode == 1) m_mode = IMAGE;
	else if (mode == 2) m_mode = IMAGE_LIST;
	else if (mode == 3) m_mode = BENCHMARK_EVALUATION;
	else m_mode = WEBCAM;

	if (m_mode == WEBCAM)
	{
		int webcam_id = parser.get<int>("webcam");
		run_webcam(webcam_id);
	}
	else if (m_mode == IMAGE)
	{
		std::string image_path = parser.get<String>("path");
		run_image(image_path);
	}
	else if (m_mode == IMAGE_LIST)
	{
		std::string image_path = parser.get<String>("path");
		run_images(image_path);
	}
	else if (m_mode == BENCHMARK_EVALUATION)
	{
		DATASET m_data;
		std::string dataset_name = parser.get<String>("dataset");
		cout << dataset_name << endl;
		if (dataset_name == "PASCAL") m_data = PASCAL_FACE;
		else if (dataset_name == "FDDB") m_data = FDDB;
		else if (dataset_name == "WIDER_VAL") m_data = WIDER_VAL;
		else if (dataset_name == "WIDER_TEST") m_data = WIDER_TEST;
		else if (dataset_name == "UFDD") m_data = UFDD;
		else if (dataset_name == "AFW") m_data = AFW;

		std::string dataset_path = parser.get<String>("path");

		if (m_data == AFW || m_data == PASCAL_FACE || m_data == FDDB)
			run_afw_pascal_fddb(m_data, dataset_path);
		else if (m_data == WIDER_VAL || m_data == WIDER_TEST)
			run_wider(m_data, dataset_path);
		else if (m_data == UFDD)
			run_ufdd(dataset_path);
	}

	return 1;

	/*Mat image = imread("libfacedetection-master/libfacedetection-master/images/keliamoniz1.jpg");
	std::vector<Result> detection_results = faceDetection(image);
	imshow("result_cnn", image);
	waitKey();
	*/
	return 0;
}

