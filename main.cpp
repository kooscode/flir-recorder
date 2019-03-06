#include <iostream>
#include <sstream>

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include "SpinVideo.h"

//opencv
#include "opencv2/opencv.hpp"

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace Spinnaker::Video;
using namespace std;

// Width and height
#define FLIR_WIDTH 1440
#define FLIR_HEIGHT 1080

int main(int argc, char** argv) 
{
    if (argc < 2)
    {
        std::cout << "Output video file required.\n\t example: flir-recorder foo.mpg" << std::endl; 
        return -1;
    }        
    
    std::string outfile = argv[1];
    
    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    // Finish if there are no cameras
    if ( camList.GetSize() == 0)
    {
        cout << "No cameras found.." << endl;
    }
    else
    {

        // image vector
        vector<ImagePtr> images;

        //get camera
        CameraPtr pCam = camList.GetByIndex(0);

        // Initialize camera
        pCam->Init();

        // Retrieve TL device nodemap and print device information
        INodeMap & nodeMapTLDevice = pCam->GetTLDeviceNodeMap();

        // Retrieve GenICam nodemap
        INodeMap & nodeMap = pCam->GetNodeMap();

        CIntegerPtr ptrBinH = nodeMap.GetNode("BinningHorizontal");
        ptrBinH->SetValue(2);
        CIntegerPtr ptrBinW = nodeMap.GetNode("BinningVertical");
        ptrBinW->SetValue(2);

        CIntegerPtr ptrWidth = nodeMap.GetNode("Width");
        ptrWidth->SetValue( (uint64_t) FLIR_WIDTH/2);
        CIntegerPtr ptrHeight = nodeMap.GetNode("Height");
        ptrHeight->SetValue((uint64_t) FLIR_HEIGHT/2);

        // Set acquisition mode to continuous
        CEnumerationPtr ptrAcquisitionMode = nodeMap.GetNode("AcquisitionMode");
        if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode))
        {
            cout << "Unable to set acquisition mode to continuous (node retrieval). Aborting..." << endl << endl;
            return -1;
        }

        CEnumEntryPtr ptrAcquisitionModeContinuous = ptrAcquisitionMode->GetEntryByName("Continuous");
        if (!IsAvailable(ptrAcquisitionModeContinuous) || !IsReadable(ptrAcquisitionModeContinuous))
        {
            cout << "Unable to set acquisition mode to continuous (entry 'continuous' retrieval). Aborting..." << endl << endl;
            return -1;
        }

        int64_t acquisitionModeContinuous = ptrAcquisitionModeContinuous->GetValue();

        ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

        cout << "Acquisition mode set to continuous..." << endl;

        CFloatPtr ptrAcquisitionFrameRate = nodeMap.GetNode("AcquisitionFrameRate");
        if (!IsAvailable(ptrAcquisitionFrameRate) || !IsReadable(ptrAcquisitionFrameRate))
        {
            cout << "Unable to retrieve frame rate. Aborting..." << endl << endl;
            return -1;
        }

        float fps = static_cast<float>(ptrAcquisitionFrameRate->GetValue());        
        
        //flir video recorder..
        SpinVideo video;
        const unsigned int k_videoFileSize = 2048;
        video.SetMaximumFileSize(k_videoFileSize);
        
        //video options..
        Video::MJPGOption option;
        option.frameRate = fps;
        option.quality = 99;
        video.Open(outfile.c_str(), option);

        // Begin acquiring images
        pCam->BeginAcquisition();
        
        //Create OpenCV Window
        char window_name[] = "FLIR";
        cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_FREERATIO | cv::WINDOW_AUTOSIZE);
        
        for (int i = 0; i < 10000; i++)
        {
            // Retrieve the next received image
            ImagePtr pResultImage = pCam->GetNextImage();

            if (pResultImage->IsIncomplete())
            {
                cout << "skipped frame.." << endl << flush;
            }
            else
            {
                uint32_t xpad = pResultImage->GetXPadding();
                uint32_t ypad = pResultImage->GetYPadding();
                uint32_t width = pResultImage->GetWidth();
                uint32_t height = pResultImage->GetHeight();
                
                // Deep copy image into image and append to video frame..
                ImagePtr pConvertedImage = pResultImage->Convert(PixelFormat_BGR8, HQ_LINEAR); //PixelFormat_Mono8, HQ_LINEAR));

                //image data contains padding. When allocating Mat container size, you need to account for the X,Y image data padding.
                cv::Mat cvMat = cv::Mat(height + ypad, width + xpad, CV_8UC3, pConvertedImage->GetData(), pConvertedImage->GetStride());
                
                //save video frame
                video.Append(pConvertedImage);                

                //update onscreen img.
                cv::imshow(window_name, cvMat);
                
                //wait for any Key and quit
                int x = cv::waitKey(1);
                if(x > 0 ) 
                {
                    //ESC = 27
                    break;       
                }

            }

            // Release image
            pResultImage->Release();
        }        
        
        // End acquisition
        pCam->EndAcquisition();

        // de-init camera
        pCam->DeInit();
        
        //close video file..
        video.Close();
        
    }    
    
    // Clear camera list before releasing system
    camList.Clear();

    // Release system
    system->ReleaseInstance();

    
    return 0;
}

