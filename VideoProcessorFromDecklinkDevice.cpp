#include "stdafx.h"
#include "VideoProcessorFromDecklinkDevice.h"
#include "DeckLinkDevice.h"
#include <iostream>

#include "opencv2\core\core.hpp"
#include "opencv2\imgproc\imgproc.hpp"


using namespace cvf::decklink;
using namespace std;

namespace cvf {

VideoProcessorFromDecklinkDevice::VideoProcessorFromDecklinkDevice(double frameRate)
	:mFrameRate(frameRate)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// de momento, a saco
	mHeight = 1080;
	mWidth = 1920;
	

	cv::Mat mat = cv::Mat(mHeight, mWidth, CV_8UC3 );
       


}


VideoProcessorFromDecklinkDevice::~VideoProcessorFromDecklinkDevice(void)
{
	CoUninitialize();
}

double VideoProcessorFromDecklinkDevice::getFrameRate() {
	return mFrameRate;
}



bool VideoProcessorFromDecklinkDevice::setInput()
{
	m_deckLinkDiscovery = new DeckLinkDeviceDiscovery( shared_from_this() );
		
	if (! m_deckLinkDiscovery->Enable()) {
		cout << "Please install the Blackmagic Desktop Video drivers to use the features of this application." << "This application requires the Desktop Video drivers installed.";
		return false;
	}

	
	int64 ticks = ::GetTickCount64();
	while(1)
	{
		lock_guard<mutex> g(mtxDevices);
		if (!mLstDevices.empty())
		{
			auto device = mLstDevices[0];
			device->StartCapture(0, this , true );
			return true;
		}

		if (::GetTickCount64() - ticks > 2000)  //espera dos segundos
			break;
	}

	return false;
}


void VideoProcessorFromDecklinkDevice::addDevice( std::shared_ptr<decklink::DeckLinkDevice> newDevice ) 
{

	// Initialise new DeckLinkDevice object
	if (!newDevice->Init())
	{
		newDevice->Release();
		return;
	}

	mLstDevices.push_back( newDevice );

	std::string name = newDevice->GetDeviceName();


}


HRESULT		VideoProcessorFromDecklinkDevice::DrawFrame(IDeckLinkVideoFrame* videoFrame)
{

    // Handle Video Frame
    if(videoFrame)// && !stopped)
    {
        if (videoFrame->GetFlags() & bmdFrameHasNoInputSource)
        {
			fprintf(stderr, "Frame received - No input signal detected\n");
        }
        else
        {
            convertFrameToOpenCV( videoFrame );
        }

    }

	return S_OK;
}


bool  VideoProcessorFromDecklinkDevice::readNextFrame( cv::Mat &frame )
{
	if (!mFrame.empty()) {
		frame = mFrame;
		return true;
	}
    return false;
}

bool VideoProcessorFromDecklinkDevice::convertFrameToOpenCV(IDeckLinkVideoFrame* in)
{
	switch (in->GetPixelFormat()) {
    case bmdFormat8BitYUV:
    {
        void* data;
        if (FAILED(in->GetBytes(&data)))
            return false;
        cv::Mat mat = cv::Mat(in->GetHeight(), in->GetWidth(), CV_8UC2, data,
            in->GetRowBytes());

		lock_guard<mutex> g( mtxFrameBytes);
        cv::cvtColor(mat, mFrame, CV_YUV2BGR_UYVY);
        return true;
    }
    case bmdFormat8BitBGRA:
    {
        void* data;
        if (FAILED(in->GetBytes(&data)))
            return false;
        cv::Mat mat = cv::Mat(in->GetHeight(), in->GetWidth(), CV_8UC4, data);
        
		lock_guard<mutex> g( mtxFrameBytes);
		cv::cvtColor(mat, mFrame, CV_BGRA2BGR);
        return true;
    }
    default:
    {
		/*
        ComPtr<IDeckLinkVideoConversion> deckLinkVideoConversion =
            CreateVideoConversionInstance();
        if (! deckLinkVideoConversion)
            return false;

        CvMatDeckLinkVideoFrame cvMatWrapper(in->GetHeight(), in->GetWidth());
        if (FAILED(deckLinkVideoConversion->ConvertFrame(in.get(), &cvMatWrapper)))
            return false;
        
		lock_guard<mutex> g( mtxFrameBytes);
		cv::cvtColor(cvMatWrapper.mat, out, CV_BGRA2BGR);
        return true;*/
		return false;
    }}
}


}