// need to calc the burned area

// #include <stdio.h>
// #include <fstream>
// #include <limits>
// #include <string>
// #include <opencv2/opencv.hpp>
// #include "opencv2/highgui.hpp"

#include "utils.h"
#include <qgraphicsview.h>



// FlameProcessing Constructor definition
FlameProcessing::FlameProcessing() {


}

void FlameProcessing::setIRLScale(double x, double y){
    irlScaleX = x;
    irlScaleY = y;
    //cmPerPixel =
}

void FlameProcessing::setScale() {
    cmPerPixel = irlScaleY / pixelsY;
}

void FlameProcessing::scalingMouse(int event, int x, int y, int flags) {
    if(event == 1){
        scaleClicks = scaleClicks + 1;
        currPos.x = x;
        currPos.y = y;
        std::cout<<event<<" "<<x<<" "<<y<<" clicks:"<<scaleClicks<<"\n";
    }

    else if(event == 0) {
        currPos.x = x;
        currPos.y = y;
    }
}

// class function to set the cancel button state
void FlameProcessing::setStopProcess(bool state) {
    stopProcess = state;
}

void writeToCSV(std::string filename, std::vector<std::string> cols, std::vector<std::tuple<int,int,int>> &data) {
    std::ofstream myFile(filename);

    for (int i = 0; i < cols.size(); i++) {
        myFile << cols[i];
        if(i != cols.size() - 1) {
            myFile << ",";
        }else {
            myFile << "\n";
        }
    }

    for (const auto &entry : data) {
        myFile << std::get<0>(entry) << ", " << std::get<1>(entry) << ", " << std::get<2>(entry) << "\n";
    }


    // Close the file
    myFile.close();

}

// check if the file is .mp4
bool FlameProcessing::checkMP4(std::string newFile) {
    // TODO: Move to utils.cpp
    size_t dotPosition = newFile.find_last_of('.');
    if (dotPosition == std::string::npos) {
        return false;
    }
    std::string extension = newFile.substr(dotPosition);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   ::tolower);
    return extension == ".mp4";
}

void FlameProcessing::imageROISelect(std::string videoFilePath) {
    cv::VideoCapture cap(videoFilePath);
    cv::Mat image;
    if (!cap.read(image)) {
        std::cerr << "Error: Could not read first frame!\n";
    }
    cv::resize(image, image, cv::Size(), 0.5, 0.5); // Scale down by half
    cv::Rect roi;
    roi = cv::selectROI("Selected ROI", image);
    cv::Mat imCropped;
    try {
        imCropped = image(roi);
    } catch (const cv::Exception &e) {
        return;
    }
    image = imCropped;
    maskX = roi.x;
    maskY = roi.y;
    maskH = roi.height;
    maskW = roi.width;

    cv::destroyWindow("Selected ROI");
}

void FlameProcessing::imageScaling(std::string videoFilePath, char axis) {

    cv::VideoCapture cap(videoFilePath);
    cv::Mat firstframe;
    if (!cap.read(firstframe)) {
        std::cerr << "Error: Could not read first frame!\n";
        return; // Exit if the frame cannot be read
    }

    if (firstframe.empty()) {
        std::cerr << "Error: Retrieved frame is empty!\n";
        return;
    }

    cv::resize(firstframe, firstframe, cv::Size(), 0.5, 0.5); // Scale down by half
    cv::imshow("Manual Scaling", firstframe);
    cv::setMouseCallback("Manual Scaling", FlameProcessing::mouseCallback, this);

    cv::Point startingPos(-1,-1);
    cv::Point endingPos(-1,-1);

    // Clone the image for dynamic updates
    cv::Mat tempImage;
    tempImage = firstframe.clone();

    // Wait for two points to be clicked
    while (true) {

        //tempImage = firstframe.clone();
        firstframe.copyTo(tempImage);

        if(scaleClicks == 1 && fist_point_selected == false) {
            startingPos.x = currPos.x;
            startingPos.y = currPos.y;
            fist_point_selected = true;
        }
        if (scaleClicks == 1) {
            cv::line(tempImage, startingPos, currPos, cv::Scalar(0, 255, 0), 2);
        }

        if (scaleClicks == 2) {
            endingPos.x = currPos.x;
            endingPos.y = currPos.y;

            if(axis == 'x') {
                pixelsX = abs((startingPos.x*2)-(endingPos.x*2));
            }
            else if(axis == 'y') {
                pixelsY = abs((startingPos.y*2)-(endingPos.y*2));
            }



            break;
        }


        if (cv::waitKey(1) == 27) { // Press 'ESC' to exit early
            std::cout << "Exiting before two points were selected.\n";
            break; // Exit the loop gracefully
        }



        cv::imshow("Manual Scaling", tempImage);
    }

    scaleClicks = 0;
    fist_point_selected = false;

    cv::destroyWindow("Manual Scaling"); // Destroy the window to release resources

}


void FlameProcessing::parseVideo(std::string videoFilePath, QGraphicsView *view) {


    auto const MASK_WINDOW = "Mask Settings";
    cv::namedWindow(MASK_WINDOW, cv::WINDOW_AUTOSIZE);

    cv::VideoCapture cap(videoFilePath);

    // HSV range to detect blue color
    int minHue = 0, maxHue = 50;
    int minSat = 50, maxSat = 255;
    int minVal =  150, maxVal = 255;

    int minThresh = 200, maxThresh = 255;
    int blurAmount = 2;
    int minBBArea = 40;

    std::vector<std::tuple<int, int, int>> fireVerticalPositions;
    std::vector<std::pair<int, int>> fireLastPositions;

    // Check if the video file was opened successfully
    if (!cap.isOpened()) {
        std::cerr << "Error: FAILED OPENING FILE" << std::endl;

    } else {
        // Get video frame rate
        double fps = cap.get(cv::CAP_PROP_FPS);
        double timePerFrame = 1.0/fps; // in seconds
        double accumulatedTime = 0.0;
        double sampleInterval = 1.0;
        int seconds = 0;

        std::cout << "Frames per second: " << fps << std::endl;






        int currY = -1;
        int lastY = -1;

        imageScaling(videoFilePath, 'x');
        imageROISelect(videoFilePath);
        int frameCount = 0;


        while (true) {

            if(stopProcess) {
                break;
            }

            accumulatedTime = accumulatedTime + timePerFrame;
            // Capture each frame
            cap >> frame;
            // If the frame is empty, break the loop (end of video)
            if (frame.empty()) {
                break;
            }
            cv::resize(frame, frame, cv::Size(), 0.5, 0.5); // Scale down by half
            // Create a black mask
            cv::Mat newFrame = cv::Mat::zeros(frame.size(), frame.type());
            // Define the rectangle for the mask
            cv::Rect box(maskX, maskY, maskW, maskH);
            // Copy the contents of the rectangle from the frame to the mask
            frame(box).copyTo(newFrame(box));
            // apply mask
            bg_sub->apply(newFrame, mask);

            // reinit image processing masks
            foreground.setTo(cv::Scalar(0, 0, 0));
            HSVFrame.setTo(cv::Scalar(0, 0, 0));
            dilateErodeMask.setTo(cv::Scalar(0,0,0));
            newFrame.copyTo(foreground, mask);

            // convert foreground to HSV
            cv::cvtColor(foreground, HSVFrame, cv::COLOR_BGR2HSV);

            // Create an HSV mask for flame colors
            cv::inRange(HSVFrame, cv::Scalar(minHue, minSat, minVal), cv::Scalar(maxHue, maxSat, maxVal), hsvMask);

            // dilate the flame
            cv::dilate(hsvMask, dMask, kernel, cv::Point(-1, -1), 2);

            std::vector<std::vector<cv::Point>> contours;
            std::vector<std::vector<cv::Point>> filteredContours;
            cv::findContours(dMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

            for (const auto& contour : contours) {

                int area = cv::contourArea(contour);

                if (area > minBBArea) {
                    filteredContours.push_back(contour);
                }

            }

            //Replace the original contours with the filtered ones
            contours = filteredContours;

            int highestY = std::numeric_limits<int>::max();

            for (const auto& contour : contours) {
                // Get the bounding box for the current contour
                cv::Rect boundingBox = cv::boundingRect(contour);
                int bottom_y = boundingBox.y + boundingBox.height;
                int top_y = boundingBox.y;

                //std::cout << bottom_y << " ";
                if (bottom_y > currY && frameCount > 10) {
                    currY = bottom_y;
                }

                if (top_y < highestY) {
                    highestY = top_y;
                    lastY = top_y;
                }
            }

            //std::cout << "NEXT" << "\n";

            if(accumulatedTime > sampleInterval) {
                std::cout << frameCount << "\n";

                fireVerticalPositions.emplace_back(seconds, currY, lastY);
                //fireLastPositions.emplace_back(seconds, lastY);

                //std::cout << " " << fireVerticalPositions() "\n";


                accumulatedTime = accumulatedTime - sampleInterval;
                seconds = seconds +1;
            }



            // Define the point you want to draw
            cv::Point tl(80, currY-5);  // Coordinates of the point
            cv::Point br(800, currY+5);  // Coordinates of the point

            cv::Point tl2(80, highestY-5);  // Coordinates of the point
            cv::Point br2(800, highestY+5);  // Coordinates of the point


            // Draw the point on the image
            cv::rectangle(newFrame, tl,br,cv::Scalar(0,255,0),cv::FILLED);
            cv::rectangle(newFrame, tl2,br2,cv::Scalar(0,200,0),cv::FILLED);

            cv::drawContours(newFrame, contours, -1, cv::Scalar(255, 0, 0), cv::FILLED);

            if (frameCount % 5 == 0) { // Show every 5th frame
                // //cv::imshow("cleaned mask", frame);
                // view->scene()->clear();
                // QImage qimg = matToQImage(newFrame);

                // // Create a QPixmap from the QImage and add it to the scene

                // scene->addPixmap(QPixmap::fromImage(qimg));
                //view->setSceneRect(0, 0, qimg.width(), qimg.height());

                // Optionally set the view size to match the image size
                //view->setSceneRect(0, 0, qimg.width(), qimg.height());
                //graphicsViewHelper(view, newFrame, scene);

            }

            // Wait for 1 ms between frames (to simulate real-time playback)
            if (cv::waitKey(1) == 27) { // Exit if 'Esc' is pressed
                break;
            }

            frameCount++;
        }

        setStopProcess(false);

        // Release video capture object and close display window
        cap.release();
        cv::destroyAllWindows();

        std::vector<std::string> cols = {"Time", "Flame Front", "Flame Back"};

        writeToCSV("FirePositions.csv", cols, fireVerticalPositions);
    }

}

// Mouse callback function
void FlameProcessing::mouseCallback(int event, int x, int y, int flags, void* userdata) {
    FlameProcessing* self = static_cast<FlameProcessing*>(userdata);
    self->scalingMouse(event, x, y, flags);
}





void FlameProcessing::setROIBox(int x, int y, int h, int w) {
    maskX = x;
    maskY = y;
    maskH = h;
    maskW = w;
}
cv::Mat FlameProcessing::findContourImage(cv::Mat original_frame) {
    int minThresh = 200, maxThresh = 255;
    int blurAmount = 2;
    int minBBArea = 60;
    // HSV range to detect blue color
    int minHue = 0, maxHue = 50;
    int minSat = 50, maxSat = 255;
    int minVal =  150, maxVal = 255;
    // Create a black mask
    cv::Mat newFrame = cv::Mat::zeros(original_frame.size(), original_frame.type());
    // Define the rectangle for the mask
    cv::Rect box(maskX, maskY, maskW, maskH);
    // Copy the contents of the rectangle from the frame to the mask
    original_frame(box).copyTo(newFrame(box));
    // apply mask
    bg_sub->apply(newFrame, mask);
    // reinit image processing masks
    foreground.setTo(cv::Scalar(0, 0, 0));
    HSVFrame.setTo(cv::Scalar(0, 0, 0));
    dilateErodeMask.setTo(cv::Scalar(0,0,0));
    newFrame.copyTo(foreground, mask);
    // convert foreground to HSV
    cv::cvtColor(foreground, HSVFrame, cv::COLOR_BGR2HSV);
    // Create an HSV mask for flame colors
    cv::inRange(HSVFrame, cv::Scalar(minHue, minSat, minVal), cv::Scalar(maxHue, maxSat, maxVal), hsvMask);
    // dilate the flame
    cv::dilate(hsvMask, dMask, kernel, cv::Point(-1, -1), 2);
    std::vector<std::vector<cv::Point>> contours;

    cv::findContours(dMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    for (const auto& contour : contours) {
        int area = cv::contourArea(contour);
        if (area > minBBArea) {
            filteredContours.push_back(contour);
            std::cout<<"removing contour\n";
        }
    }
    //Replace the original contours with the filtered ones
    contours = filteredContours;
    cv::drawContours(newFrame, filteredContours, -1, cv::Scalar(255, 0, 0), cv::FILLED);
    return newFrame;
}

// loop through lines, find lowest and closest point to each line between threshold
std::vector<double> FlameProcessing::recordData(std::vector<double> segments){
    int threshold = 5;
    std::vector<double> posData; // array of each line
    //loop through all segments
    for(int i = 0; i < segments.size(); i++) {
        double currentX = segments[i];
        int leadingY = -1;

        // loop through all contours
        for (const auto& contour : filteredContours) {

            //loop through each contour pixel
            for (const auto& point : contour) {
                if(point.x > (currentX-threshold) && point.x < (currentX+threshold) && point.y > leadingY) {
                    leadingY = point.y;
                }
            }
        }
        posData.push_back(leadingY);


    }

    filteredContours.clear();
    return posData;
}

std::vector<std::vector<double>> FlameProcessing::cleanData(std::vector<std::vector<double>> positions) {
    int numOfLines = positions[0].size();
    std::vector<std::vector<double>> allLines;
    std::cout<<"CM PER PIXEL:" << cmPerPixel << std::endl;
    for (int line = 0; line < numOfLines; line++) {
        std::vector<double> cleanedPositions;
        bool firstDataEncountered = false;
        double last_known_position = 0.0; // Initialize position at 0 cm
        double initialPx = 0;

        //loop through all frames
        for (int frame = 0; frame < positions.size(); frame++) {
            int curr = positions[frame][line];
            if (curr == -1) {
                // If no new data, keep the last position
                cleanedPositions.push_back(last_known_position);
            } else if (curr != -1 && !firstDataEncountered) {
                initialPx = curr;
                cleanedPositions.push_back(last_known_position); // keep as 0 for the first one
                firstDataEncountered = true;
            } else if (curr != -1 && firstDataEncountered) {
                last_known_position = (curr - initialPx) * cmPerPixel;
                cleanedPositions.push_back(last_known_position);
            }

            std::cout<<"Line[" << line << "]:" << cleanedPositions[frame]<< ", with px:" << positions[frame][line]<<std::endl;
        }
        std::cout << "NEXT DATA SEGMENT" << std::endl;
        allLines.emplace_back(cleanedPositions);
    }
    return allLines;
}
