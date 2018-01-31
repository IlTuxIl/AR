//
// Created by julien on 10/01/18.
//

#include <window.h>
#include "CamCalibration.h"

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif

using namespace cv;
using namespace std;

class Settings
{
public:
    Settings() : goodInput(false) {}
    enum Pattern { NOT_EXISTING, CHESSBOARD, CIRCLES_GRID, ASYMMETRIC_CIRCLES_GRID };
    enum InputType {INVALID, CAMERA, VIDEO_FILE, IMAGE_LIST};

    void write(FileStorage& fs) const                        //Write serialization for this class
    {
        fs << "{" << "BoardSize_Width"  << boardSize.width
           << "BoardSize_Height" << boardSize.height
           << "Square_Size"         << squareSize
           << "Calibrate_Pattern" << patternToUse
           << "Calibrate_NrOfFrameToUse" << nrFrames
           << "Calibrate_FixAspectRatio" << aspectRatio
           << "Calibrate_AssumeZeroTangentialDistortion" << calibZeroTangentDist
           << "Calibrate_FixPrincipalPointAtTheCenter" << calibFixPrincipalPoint

           << "Write_DetectedFeaturePoints" << bwritePoints
           << "Write_extrinsicParameters"   << bwriteExtrinsics
           << "Write_outputFileName"  << outputFileName

           << "Show_UndistortedImage" << showUndistorsed

           << "Input_FlipAroundHorizontalAxis" << flipVertical
           << "Input_Delay" << delay
           << "Input" << input
           << "}";
    }
    void read(const FileNode& node)                          //Read serialization for this class
    {
        node["BoardSize_Width" ] >> boardSize.width;
        node["BoardSize_Height"] >> boardSize.height;
        node["Calibrate_Pattern"] >> patternToUse;
        node["Square_Size"]  >> squareSize;
        node["Calibrate_NrOfFrameToUse"] >> nrFrames;
        node["Calibrate_FixAspectRatio"] >> aspectRatio;
        node["Write_DetectedFeaturePoints"] >> bwritePoints;
        node["Write_extrinsicParameters"] >> bwriteExtrinsics;
        node["Write_outputFileName"] >> outputFileName;
        node["Calibrate_AssumeZeroTangentialDistortion"] >> calibZeroTangentDist;
        node["Calibrate_FixPrincipalPointAtTheCenter"] >> calibFixPrincipalPoint;
        node["Input_FlipAroundHorizontalAxis"] >> flipVertical;
        node["Show_UndistortedImage"] >> showUndistorsed;
        node["Input"] >> input;
        node["Input_Delay"] >> delay;
        interprate();
    }
    void interprate()
    {
        goodInput = true;
        if (boardSize.width <= 0 || boardSize.height <= 0)
        {
            cerr << "Invalid Board size: " << boardSize.width << " " << boardSize.height << endl;
            goodInput = false;
        }
        if (squareSize <= 10e-6)
        {
            cerr << "Invalid square size " << squareSize << endl;
        }
        if (nrFrames <= 0)
        {
            cerr << "Invalid number of frames " << nrFrames << endl;
            goodInput = false;
        }

        if (input.empty())      // Check for valid input
            inputType = INVALID;
        else
        {
            if (input[0] >= '0' && input[0] <= '9')
            {
                stringstream ss(input);
                ss >> cameraID;
                inputType = CAMERA;
            }
            else
            {
                if (isListOfImages(input) && readStringList(input, imageList))
                {
                    inputType = IMAGE_LIST;
                    nrFrames = (nrFrames < (int)imageList.size()) ? nrFrames : (int)imageList.size();
                }
                else
                    inputType = VIDEO_FILE;
            }
            if (inputType == CAMERA)
                inputCapture.open(cameraID);
            if (inputType == VIDEO_FILE)
                inputCapture.open(input);
            if (inputType != IMAGE_LIST && !inputCapture.isOpened())
                inputType = INVALID;
        }
        if (inputType == INVALID)
        {
            cerr << " Inexistent input: " << input;
            goodInput = false;
        }

        flag = 0;
        if(calibFixPrincipalPoint) flag |= CV_CALIB_FIX_PRINCIPAL_POINT;
        if(calibZeroTangentDist)   flag |= CV_CALIB_ZERO_TANGENT_DIST;
        if(aspectRatio)            flag |= CV_CALIB_FIX_ASPECT_RATIO;


        calibrationPattern = NOT_EXISTING;
        if (!patternToUse.compare("CHESSBOARD")) calibrationPattern = CHESSBOARD;
        if (!patternToUse.compare("CIRCLES_GRID")) calibrationPattern = CIRCLES_GRID;
        if (!patternToUse.compare("ASYMMETRIC_CIRCLES_GRID")) calibrationPattern = ASYMMETRIC_CIRCLES_GRID;
        if (calibrationPattern == NOT_EXISTING)
        {
            cerr << " Inexistent camera calibration mode: " << patternToUse << endl;
            goodInput = false;
        }
        atImageList = 0;

    }
    Mat nextImage()
    {
        Mat result;
        if( inputCapture.isOpened() )
        {
            Mat view0;
            inputCapture >> view0;
            view0.copyTo(result);
        }
        else if( atImageList < (int)imageList.size() )
            result = imread(imageList[atImageList++], CV_LOAD_IMAGE_COLOR);

        return result;
    }

    static bool readStringList( const string& filename, vector<string>& l )
    {
        l.clear();
        FileStorage fs(filename, FileStorage::READ);
        if( !fs.isOpened() )
            return false;
        FileNode n = fs.getFirstTopLevelNode();
        if( n.type() != FileNode::SEQ )
            return false;
        FileNodeIterator it = n.begin(), it_end = n.end();
        for( ; it != it_end; ++it )
            l.push_back((string)*it);
        return true;
    }

    static bool isListOfImages( const string& filename)
    {
        string s(filename);
        // Look for file extension
        if( s.find(".xml") == string::npos && s.find(".yaml") == string::npos && s.find(".yml") == string::npos )
            return false;
        else
            return true;
    }
public:
    Size boardSize;            // The size of the board -> Number of items by width and height
    Pattern calibrationPattern;// One of the Chessboard, circles, or asymmetric circle pattern
    float squareSize;          // The size of a square in your defined unit (point, millimeter,etc).
    int nrFrames;              // The number of frames to use from the input for calibration
    float aspectRatio;         // The aspect ratio
    int delay;                 // In case of a video input
    bool bwritePoints;         //  Write detected feature points
    bool bwriteExtrinsics;     // Write extrinsic parameters
    bool calibZeroTangentDist; // Assume zero tangential distortion
    bool calibFixPrincipalPoint;// Fix the principal point at the center
    bool flipVertical;          // Flip the captured images around the horizontal axis
    string outputFileName;      // The name of the file where to write
    bool showUndistorsed;       // Show undistorted images after calibration
    string input;               // The input ->



    int cameraID;
    vector<string> imageList;
    int atImageList;
    VideoCapture inputCapture;
    InputType inputType;
    bool goodInput;
    int flag;

private:
    string patternToUse;


};

static void read(const FileNode& node, Settings& x, const Settings& default_value = Settings())
{
    if(node.empty())
        x = default_value;
    else
        x.read(node);
}

enum { DETECTION = 0, CAPTURING = 1, CALIBRATED = 2 };

bool runCalibrationAndSave(Settings& s, Size imageSize, Mat&  cameraMatrix, Mat& distCoeffs, vector<Mat> rvecs, vector<Mat> tvecs,
                           vector<vector<Point2f> > imagePoints );


void CamCalibration::calibrate() {

    Settings s;
    const string inputSettingsFile = "conf/in_VID5.xml";
    FileStorage fs(inputSettingsFile, FileStorage::READ); // Read the settings
    if (!fs.isOpened())
    {
        cout << "Could not open the configuration file: \"" << inputSettingsFile << "\"" << endl;
    }
    fs["Settings"] >> s;
    fs.release();                                         // close Settings file

    if (!s.goodInput)
    {
        cout << "Invalid input detected. Application stopping. " << endl;
    }

    vector<vector<Point2f> > imagePoints;

    Size imageSize;
    clock_t prevTimestamp = 0;
    const Scalar RED(0,0,255), GREEN(0,255,0);
    const char ESC_KEY = 27;

    std::vector<cv::Mat> rvecs;
    std::vector<cv::Mat> tvecs;

    while(true)
    {
        Mat view;
        bool blinkOutput = false;
        view = s.nextImage();

        //-----  If got enough image, then stop calibration-------------
        if(imagePoints.size() >= (unsigned)s.nrFrames) {
            runCalibrationAndSave(s, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs, imagePoints);
            break;
        }

        imageSize = view.size();  // Format input image.
        if( s.flipVertical )    flip( view, view, 0 );

        vector<Point2f> pointBuf;
        bool found = findChessboardCorners( view, s.boardSize, pointBuf, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FAST_CHECK | CV_CALIB_CB_NORMALIZE_IMAGE);

        if (found)
        {
            // improve the found corners' coordinate accuracy for chessboard
            Mat viewGray;
            cvtColor(view, viewGray, COLOR_BGR2GRAY);
            cornerSubPix(viewGray, pointBuf, Size(11,11), Size(-1,-1), TermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 30, 0.1 ));

            if(!s.inputCapture.isOpened() || clock() - prevTimestamp > s.delay*1e-3*CLOCKS_PER_SEC)
            {
                imagePoints.push_back(pointBuf);
                prevTimestamp = clock();
                blinkOutput = s.inputCapture.isOpened();
            }

            // Draw the corners.
            drawChessboardCorners( view, s.boardSize, Mat(pointBuf), found );
        }

        if( blinkOutput )
            bitwise_not(view, view);

        //------------------------------ Show image and check for input commands -------------------
        imshow("Image View", view);
        char key = (char)waitKey(s.inputCapture.isOpened() ? 50 : s.delay);

        if( key  == ESC_KEY )
            break;

        if( s.inputCapture.isOpened() && key == 'g' )
            imagePoints.clear();
    }
}

void CamCalibration::load(std::string filePath) {
    FileStorage fs(filePath, FileStorage::READ); // Read the settings
    fs["Distortion_Coefficients"] >> distCoeffs;
    fs["Camera_Matrix"]  >> cameraMatrix;
}


void CamCalibration::computeFrustum() {
    invCameraMatrix = Mat::ones(3, 3, CV_64F);
    invert(cameraMatrix, invCameraMatrix);
    Mat xmin, xmax, ymin, ymax;
    Mat a, b, c, d;

    xmin = Mat::ones(3, 1, CV_64F);
    xmax = Mat::ones(3, 1, CV_64F);
    ymin = Mat::ones(3, 1, CV_64F);
    ymax = Mat::ones(3, 1, CV_64F);

    a = Mat::ones(3, 1, CV_64F);
    b = Mat::ones(3, 1, CV_64F);
    c = Mat::ones(3, 1, CV_64F);
    d = Mat::ones(3, 1, CV_64F);

    xmin.at<double>(0) = 0;
    xmin.at<double>(1) = -(window_height() / 2);

    xmax.at<double>(0) = window_width();
    xmax.at<double>(1) = (window_height() / 2);

    ymin.at<double>(0) = window_width() / 2;
    ymin.at<double>(1) = 0;

    ymax.at<double>(0) = window_width() / 2;
    ymax.at<double>(1) = window_height();

    a = invCameraMatrix * xmin;
    b = invCameraMatrix * ymax;
    c = invCameraMatrix * xmax;
    d = invCameraMatrix * ymin;

    double near = 0.1;
    double far = 1000.0;

    a = a * near; //left
    b = b * near; //top
    c = c * near; //right
    d = d * near; //bot

    double distX = norm(c - a);
    double distY = norm(b - d);

    double left = -distX/2;
    double right = distX/2;
    double top = distY/2;
    double bot = -distX/2;

    frustum.m[0][0] = (2*near)/ distX;
    frustum.m[0][2] = (right+left)/(distX);

    frustum.m[1][1] = (2*near) / distY;
    frustum.m[1][2] = (top+bot)/(distY);

    frustum.m[2][2] = -((far+near)/(far-near));
    frustum.m[2][3] = -((2*(far*near))/(far-near));

    frustum.m[3][2] = -1;

}

void CamCalibration::start(std::string filePath, bool needCalibration) {

    if(!needCalibration)
        load();
    else
        calibrate();

    computeFrustum();

    VideoCapture cam(0);
    Mat view;
    Size2i s = {7,4};
    std::vector<Point2f> pointImage;
    std::vector<Point3f> pointMire = initPoint3D(7, 4, 3.5);

    Mat tmp;
    bool flag;
    bool first = false;
    for(;;) {
        cam >> view;
        // chessboard
        flag = findChessboardCorners(view, s, pointImage, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FAST_CHECK);
        if (flag) {
            drawChessboardCorners(view, s, Mat(pointImage), flag);

            solvePnP(pointMire, pointImage, cameraMatrix, distCoeffs, rvec, tvec, first, CV_EPNP);
            Rodrigues(rvec, tmp);
            computeTransform(tmp, tvec);

            getEulerAngle(tmp, rot);
            transform = tvec;
        }

        // magic wand detection
        findMagicWand(view);

        char key = (char)waitKey(50);

        if( key  == 27 )
            break;
        imshow(" ", view);
    }


    cam.release();
}

void CamCalibration::getEulerAngle(Mat &rotCamerMatrix,Vec3d &eulerAngles){

    Mat cameraMatrix,rotMatrix,transVect,rotMatrixX,rotMatrixY,rotMatrixZ;
    double* _r = rotCamerMatrix.ptr<double>();
    double projMatrix[12] = {_r[0],_r[1],_r[2],0,
                             _r[3],_r[4],_r[5],0,
                             _r[6],_r[7],_r[8],0};

    decomposeProjectionMatrix( Mat(3,4,CV_64FC1,projMatrix),
                               cameraMatrix,
                               rotMatrix,
                               transVect,
                               rotMatrixX,
                               rotMatrixY,
                               rotMatrixZ,
                               eulerAngles);
}

static double computeReprojectionErrors( const vector<vector<Point3f> >& objectPoints,
                                         const vector<vector<Point2f> >& imagePoints,
                                         const vector<Mat>& rvecs, const vector<Mat>& tvecs,
                                         const Mat& cameraMatrix , const Mat& distCoeffs,
                                         vector<float>& perViewErrors)
{
    vector<Point2f> imagePoints2;
    int i, totalPoints = 0;
    double totalErr = 0, err;
    perViewErrors.resize(objectPoints.size());

    for( i = 0; i < (int)objectPoints.size(); ++i )
    {
        projectPoints( Mat(objectPoints[i]), rvecs[i], tvecs[i], cameraMatrix,
                       distCoeffs, imagePoints2);
        err = norm(Mat(imagePoints[i]), Mat(imagePoints2), CV_L2);

        int n = (int)objectPoints[i].size();
        perViewErrors[i] = (float) std::sqrt(err*err/n);
        totalErr        += err*err;
        totalPoints     += n;
    }

    return std::sqrt(totalErr/totalPoints);
}

static void calcBoardCornerPositions(Size boardSize, float squareSize, vector<Point3f>& corners)
{
    corners.clear();

    for( int i = 0; i < boardSize.height; ++i )
        for( int j = 0; j < boardSize.width; ++j )
            corners.push_back(Point3f(float( j*squareSize ), float( i*squareSize ), 0));
}

static bool runCalibration( Settings& s, Size& imageSize, Mat& cameraMatrix, Mat& distCoeffs,
                            vector<vector<Point2f> > imagePoints, vector<Mat>& rvecs, vector<Mat>& tvecs,
                            vector<float>& reprojErrs,  double& totalAvgErr)
{

    cameraMatrix = Mat::eye(3, 3, CV_64F);
    if( s.flag & CV_CALIB_FIX_ASPECT_RATIO )
        cameraMatrix.at<double>(0,0) = 1.0;

    distCoeffs = Mat::zeros(8, 1, CV_64F);

    vector<vector<Point3f> > objectPoints(1);
    calcBoardCornerPositions(s.boardSize, s.squareSize, objectPoints[0]);

    objectPoints.resize(imagePoints.size(),objectPoints[0]);

    //Find intrinsic and extrinsic camera parameters
    double rms = calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix,
                                 distCoeffs, rvecs, tvecs, s.flag|CV_CALIB_FIX_K4|CV_CALIB_FIX_K5);

    cout << "Re-projection error reported by calibrateCamera: "<< rms << endl;

    bool ok = checkRange(cameraMatrix) && checkRange(distCoeffs);

    totalAvgErr = computeReprojectionErrors(objectPoints, imagePoints,
                                            rvecs, tvecs, cameraMatrix, distCoeffs, reprojErrs);

    return ok;
}

// Print camera parameters to the output file
static void saveCameraParams( Settings& s, Size& imageSize, Mat& cameraMatrix, Mat& distCoeffs,
                              const vector<Mat>& rvecs, const vector<Mat>& tvecs,
                              const vector<float>& reprojErrs, const vector<vector<Point2f> >& imagePoints,
                              double totalAvgErr )
{
    FileStorage fs( s.outputFileName, FileStorage::WRITE );

    time_t tm;
    time( &tm );
    struct tm *t2 = localtime( &tm );
    char buf[1024];
    strftime( buf, sizeof(buf)-1, "%c", t2 );

    fs << "calibration_Time" << buf;

    if( !rvecs.empty() || !reprojErrs.empty() )
        fs << "nrOfFrames" << (int)std::max(rvecs.size(), reprojErrs.size());
    fs << "image_Width" << imageSize.width;
    fs << "image_Height" << imageSize.height;
    fs << "board_Width" << s.boardSize.width;
    fs << "board_Height" << s.boardSize.height;
    fs << "square_Size" << s.squareSize;

    if( s.flag & CV_CALIB_FIX_ASPECT_RATIO )
        fs << "FixAspectRatio" << s.aspectRatio;

    if( s.flag )
    {
        sprintf( buf, "flags: %s%s%s%s",
                 s.flag & CV_CALIB_USE_INTRINSIC_GUESS ? " +use_intrinsic_guess" : "",
                 s.flag & CV_CALIB_FIX_ASPECT_RATIO ? " +fix_aspectRatio" : "",
                 s.flag & CV_CALIB_FIX_PRINCIPAL_POINT ? " +fix_principal_point" : "",
                 s.flag & CV_CALIB_ZERO_TANGENT_DIST ? " +zero_tangent_dist" : "" );
        cvWriteComment( *fs, buf, 0 );

    }

    fs << "flagValue" << s.flag;

    fs << "Camera_Matrix" << cameraMatrix;
    fs << "Distortion_Coefficients" << distCoeffs;

    fs << "Avg_Reprojection_Error" << totalAvgErr;
    if( !reprojErrs.empty() )
        fs << "Per_View_Reprojection_Errors" << Mat(reprojErrs);

    if( !rvecs.empty() && !tvecs.empty() )
    {
        CV_Assert(rvecs[0].type() == tvecs[0].type());
        Mat bigmat((int)rvecs.size(), 6, rvecs[0].type());
        for( int i = 0; i < (int)rvecs.size(); i++ )
        {
            Mat r = bigmat(Range(i, i+1), Range(0,3));
            Mat t = bigmat(Range(i, i+1), Range(3,6));

            CV_Assert(rvecs[i].rows == 3 && rvecs[i].cols == 1);
            CV_Assert(tvecs[i].rows == 3 && tvecs[i].cols == 1);
            //*.t() is MatExpr (not Mat) so we can use assignment operator
            r = rvecs[i].t();
            t = tvecs[i].t();
        }
        cvWriteComment( *fs, "a set of 6-tuples (rotation vector + translation vector) for each view", 0 );
        fs << "Extrinsic_Parameters" << bigmat;
    }

    if( !imagePoints.empty() )
    {
        Mat imagePtMat((int)imagePoints.size(), (int)imagePoints[0].size(), CV_32FC2);
        for( int i = 0; i < (int)imagePoints.size(); i++ )
        {
            Mat r = imagePtMat.row(i).reshape(2, imagePtMat.cols);
            Mat imgpti(imagePoints[i]);
            imgpti.copyTo(r);
        }
        fs << "Image_points" << imagePtMat;
    }
}

bool runCalibrationAndSave(Settings& s, Size imageSize, Mat& cameraMatrix, Mat& distCoeffs, vector<Mat> rvecs, vector<Mat> tvecs, vector<vector<Point2f> > imagePoints )
{
    vector<float> reprojErrs;
    double totalAvgErr = 0;

    bool ok = runCalibration(s,imageSize, cameraMatrix, distCoeffs, imagePoints, rvecs, tvecs,
                             reprojErrs, totalAvgErr);

    cout << (ok ? "Calibration succeeded" : "Calibration failed")
         << ". avg re projection error = "  << totalAvgErr ;

    if( ok )
        saveCameraParams( s, imageSize, cameraMatrix, distCoeffs, rvecs ,tvecs, reprojErrs,
                          imagePoints, totalAvgErr);
    return ok;
}

std::vector<Point3f> CamCalibration::initPoint3D(int x, int y, float squareSize) {

    std::vector<cv::Point3f> ret;

    for(int i = 0; i < y; ++i) {
        for (int j = 0; j < x; ++j) {
            Point3f tmp;
            tmp.x = j*squareSize;
            tmp.y = i*squareSize;
            tmp.z = 0;

            ret.push_back(tmp);
        }
    }

    return ret;
}

void CamCalibration::computeTransform(cv::Mat rodri, cv::Mat translation) {

    for(int i = 0; i < 3; ++i)
        for(int j = 0; j < 3; ++j)
            transformation.m[i][j] = rodri.at<double>(j,i);

    for(int i = 0; i < 2; ++i)
        transformation.m[i][3] = translation.at<double>(i);

    transformation.m[2][3] = -translation.at<double>(2);

}

bool CamCalibration::findMagicWand(Mat& view) {
    std::vector< std::vector< cv::Point > > contours;
    std::vector<cv::Vec4i> hierarchy;

    // create a mask to filter the red color
    cv::Mat hsv_foreground, mask_color, low_mask_color, high_mask_color;
    cv::cvtColor(view, hsv_foreground, cv::COLOR_BGR2HSV);

    /*
    // red color mask in HSV
    // first mask : low red in hsv
    cv::Scalar low_color_hsv = cv::Scalar(160,200,70,0);
    cv::Scalar middle_low_color_hsv = cv::Scalar(180,255,255,0);
    // second mask : upper red in hsv
    cv::Scalar middle_high_color_hsv = cv::Scalar(0,200,70,0);
    cv::Scalar high_color_hsv = cv::Scalar(20,255,255,0);
    */

    // yellow color
    cv::Scalar low_color_hsv = cv::Scalar(20, 100, 100);
    cv::Scalar high_color_hsv = cv::Scalar(30, 255, 255);

    inRange(hsv_foreground, low_color_hsv, high_color_hsv, mask_color);

    /*
    // keep (white) the pixels which are between the 2 hsv values
    inRange(hsv_foreground, low_color_hsv, middle_low_color_hsv, low_mask_color); // first mask : lower
    inRange(hsv_foreground, middle_high_color_hsv, high_color_hsv, high_mask_color); // second mask : upper
    bitwise_or(low_mask_color, high_mask_color, mask_color); // merge of the two masks
    */

    imshow("mask", mask_color);

    // increase the quality with erode (decrease the noise) and dilate (fill the holes)
    erode(mask_color, mask_color, cv::Mat::ones(3, 3,CV_32F), cv::Point(-1,-1), 1, 1, 1);
    dilate(mask_color, mask_color, cv::Mat::ones(9,9,CV_32F), cv::Point(-1,-1), 2, 1, 1);

    // contours detection
    cv::findContours(mask_color, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0));

    // search the center of the red torso
    if(contours.size() > 0) {
        cv::Moments mu = moments(contours[0]);
        cv::Point center(mu.m10/mu.m00 , mu.m01/mu.m00);
        cv::rectangle(view, cv::Point(center.x-5, center.y-5), cv::Point(center.x+5, center.y+5), cv::Scalar(0,255,0), 1, 8, 0);
    }
}

