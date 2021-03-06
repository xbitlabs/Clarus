#include <iostream>
#include <clews/sock/SocketClient.hpp>
#include <clews/sock/NetUtil.hpp>

#include <thread>
#include <string>

#include "VideoDevice.hpp"
#include "VideoServer.hpp"
#include "RemoteVideoFeed.hpp"

#include "AudioUtil.hpp"
#include "AudioBouncer.hpp"

#include "TimeUtil.h"

using namespace std;
using namespace cv;

#define VIDEO_SERVER_PORT 9007
#define AUDIO_SERVER_PORT 9008

void vdSenderThread(VideoDevice* vd)
{
	VideoServer vdServer(*vd, VIDEO_SERVER_PORT);
	vdServer.start();
}
void vdViewerThread(std::string remoteHost)
{
	SocketClient serverConnection;
	serverConnection.open(remoteHost, VIDEO_SERVER_PORT);
	serverConnection.setMessageWrapping(true);

	RemoteVideoFeed rfv(serverConnection);

	namedWindow("Remote Feed");

	char pressedKey = 'z';
	while(pressedKey != 'a')
	{
		imshow("Remote Feed", rfv.readFrame());
		pressedKey = waitKey(30);
	}
}
void vdPainterThread(VideoDevice* vd)
{
	char pressedKey = 'z';
	while(pressedKey != 'q')
	{
		vd->paint();
		pressedKey = waitKey(30);
	}
}

void rtaListenerThread(std::string remoteHost)
{
	try
	{
		SocketClient serverConnection;
		serverConnection.open(remoteHost, AUDIO_SERVER_PORT);
		serverConnection.setMessageWrapping(true);

		AudioBouncer bouncer(&serverConnection);
		bouncer.recieve();

		while(true)
		{
		}
	}
	catch(const RtAudioError& rae)
	{
		rae.printMessage();
	}
}

void rtaSenderThread()
{
	Socket server(AUDIO_SERVER_PORT);
	server.create();
	server.listen();

	SocketConnection* client = server.accept();
	client->setMessageWrapping(true);

	AudioBouncer bouncer(client);
	bouncer.send();

	while(true)
	{
	}
}

int main()
{
	init_net();
	string remoteHost = "127.0.0.1";

	thread audioSenderThread(rtaSenderThread);
	sleepSeconds(1);
	thread audioListenerThread(rtaListenerThread, remoteHost);

	VideoDevice vd;
	thread videoSenderThread(vdSenderThread, &vd);
	sleepSeconds(1);
	thread videoViewerThread(vdViewerThread, remoteHost);
//	thread videoPainterThread(vdPainterThread, &vd);

	videoSenderThread.join();
	videoViewerThread.join();
//	videoPainterThread.join();

	audioSenderThread.join();
	audioListenerThread.join();

	cleanup_net();
	return 0;
}
