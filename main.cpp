#include <time.h>
#include <opencv2/opencv.hpp>
#include <math.h>
#include <iostream>


using namespace std;
using namespace cv;


int main()
{
    clock_t start,finish;
    double totaltime, heights[16];
    int hi = 0;
    VideoCapture capture("/home/einstein/桌面/save_10.avi");
    //VideoCapture capture(1);
    Mat frame, binary;
    RotatedRect RA[16], R[16];
    int stateNum = 4;
    int measureNum = 2;
    KalmanFilter KF(stateNum, measureNum, 0);
    //Mat processNoise(stateNum, 1, CV_32F);
    Mat measurement = Mat::zeros(measureNum, 1, CV_32F);
    KF.transitionMatrix = (Mat_<float>(stateNum, stateNum) << 1, 0, 1, 0,//A 状态转移矩阵
        0, 1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1);
    //这里没有设置控制矩阵B，默认为零
    setIdentity(KF.measurementMatrix);//H=[1,0,0,0;0,1,0,0] 测量矩阵
    setIdentity(KF.processNoiseCov, Scalar::all(1e-5));//Q高斯白噪声，单位阵
    setIdentity(KF.measurementNoiseCov, Scalar::all(1e-1));//R高斯白噪声，单位阵
    setIdentity(KF.errorCovPost, Scalar::all(1));//P后验误差估计协方差矩阵，初始化为单位阵
    randn(KF.statePost, Scalar::all(0), Scalar::all(0.1));//初始化状态为随机值


    for(;;)
    {
        start = clock();
        capture>> frame;
        frame.copyTo(binary);

        cvtColor(frame,frame,COLOR_BGR2GRAY);

        threshold(frame, frame, 160, 255,cv::THRESH_BINARY);//调阈值就差不多了
        // Find all the contours in the thresholded image
        vector<vector<Point>> contours;
        imshow("sdfa",frame);
        findContours(frame, contours, RETR_LIST, CHAIN_APPROX_NONE);


        for (size_t i = 0; i < contours.size(); i++){

            vector<Point> points;
            double area = contourArea(contours[i]);
            if (area < 20 || 1e3 < area) continue;
            drawContours(frame, contours, static_cast<int>(i), Scalar(0), 2);

            double high;
            points = contours[i];

            RotatedRect rrect = fitEllipse(points);
            cv::Point2f* vertices = new cv::Point2f[4];
                rrect.points(vertices);


                for (int j = 0; j < 4; j++)
                {
                    cv::line(binary, vertices[j], vertices[(j + 1) % 4], cv::Scalar(0, 255, 0),4);
                }

             //ellipse(binary,rrect,Scalar(0));
             high = rrect.size.height;


             for(size_t j = 1;j < contours.size();j++){

                 vector<Point> pointsA;
                 double area = contourArea(contours[j]);
                 if (area < 20 || 1e3 < area) continue;


                 double highA, distance;
                 double slop ;
                 pointsA = contours[j];

                 RotatedRect rrectA = fitEllipse(pointsA);

                 slop = abs(rrect.angle - rrectA.angle);
                 highA = rrectA.size.height;
                 distance  =sqrt((rrect.center.x-rrectA.center.x)*(rrect.center.x-rrectA.center.x)+
                                 (rrect.center.y-rrectA.center.y)*(rrect.center.y-rrectA.center.y));


                 double max_height, min_height;
                 if(rrect.size.height > rrectA.size.height){
                     max_height = rrect.size.height;
                     min_height = rrectA.size.height;
                 }
                 else{
                     max_height = rrectA.size.height;
                     min_height = rrect.size.height;
                 }

                 double line_x = abs(rrect.center.x-rrectA.center.x);
                 double difference = max_height - min_height;
                 double aim =   distance/((highA+high)/2);
                 double difference3 = abs(rrect.size.width -rrectA.size.width);
                 double height = (rrect.size.height+rrectA.size.height)/200;
                 double slop_low = abs(rrect.angle + rrectA.angle)/2;


                 if((aim < 3.0 - height && aim > 2.0 - height     //小装甲板
                       && slop <= 5 && difference <=8 && difference3 <= 5
                      &&(slop_low <= 30 || slop_low >=150) && line_x >0.6*distance)
                      || (aim < 5.0-height && aim > 3.2 - height  //大装甲板
                          && slop <= 7 && difference <=15 && difference3 <= 8
                         &&(slop_low <= 30 || slop_low >=150) && line_x >0.7*distance)){

                     heights[hi] = (rrect.size.height+rrectA.size.height)/2;
                     R[hi] = rrect;
                     RA[hi] = rrectA;
                     hi++;
                 }
             }
        }


        double max = 0;
        int mark;
        for(int i = 0;i < hi;i++){     //多个目标存在，打更近装甲板
            if(heights[i]  >= max){
                max = heights[i];
                mark = i;
            }
        }
        if(hi != 0){
            cv::circle(binary,Point((R[mark].center.x+RA[mark].center.x)/2,
                       (R[mark].center.y+RA[mark].center.y)/2),
                       15,cv::Scalar(0,0,255),4);


            double center_x = (R[mark].center.x+RA[mark].center.x)/2;
            double center_y = (R[mark].center.y+RA[mark].center.y)/2;

            Mat prediction = KF.predict();
            Point predict_pt = Point((int)prediction.at<float>(0), (int)prediction.at<float>(1));

            measurement.at<float>(0) = (float)center_x;
            measurement.at<float>(1) = (float)center_y;
            KF.correct(measurement);

            circle(binary, predict_pt, 3, Scalar(34, 255, 255), -1);

            center_x = (int)prediction.at<float>(0);
            center_y = (int)prediction.at<float>(1);


           double lessx  =  320 - center_x;   //坐标差
           double lessy =   240 - center_y;
           //cout <<  lessx << "  " << lessy << " " << down <<endl;

        }

        imshow("okey",binary);
        waitKey(2);

        finish = clock();
        totaltime=(double)(finish-start)/CLOCKS_PER_SEC;
        cout<<"Time whole"<<totaltime<<"秒！"<<endl;
        hi = 0;
    }

}
