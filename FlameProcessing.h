#ifndef FLAMEPROCESSING_H
#define FLAMEPROCESSING_H

#include <qgraphicsscene.h>
#endif // FLAMEPROCESSING_H

#include <stdio.h>
#include <fstream>
#include <limits>
#include <string>

#include <opencv2/opencv.hpp>
#include "opencv2/highgui.hpp"


class FlameProcessing {
public:
    FlameProcessing();

    void parseVideo(std::string videoFilePath, QGraphicsView *scene);

    bool checkMP4(std::string newFile);

    void setStopProcess(bool state);

private:
    bool stopProcess = false;
    QGraphicsScene *scene = new QGraphicsScene();
    //QGraphicsScen *scene = new QGraphicsScene();

};


