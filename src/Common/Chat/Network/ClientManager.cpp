#include "ClientManager.hpp"

#include <stdexcept>
#include <iostream>
#include <QNetworkInterface>

using namespace std;

ClientManager::ClientManager(QString name, uint32_t port, QObject *parent) :
	QObject(parent), mLocalName(name), mPort(port)
{
	mServerSocket = new QTcpServer(this);
	connect(mServerSocket, SIGNAL(newConnection()), this, SLOT(newClientConnected()));

	mBroadcastSocket = new QUdpSocket(this);
	mBroadcastSocket->bind(mPort);
	connect(mBroadcastSocket, SIGNAL(readyRead()), this, SLOT(processDatagrams()));
}

QString ClientManager::localName()
{
	return mLocalName;
}

QList<MessageEndpoint*> ClientManager::identifiedEndpoints()
{
	return mIdentifiedEndpoints;
}
void ClientManager::addEnpointAsIdentified(MessageEndpoint* endpoint)
{
	mIdentifiedEndpoints << endpoint;
	emit endpointIdentified(endpoint);
}
void ClientManager::serverIdentifiedUs()
{
	MessageClient* client = (MessageClient*) sender();
	mClientsThatNeedToIdentify.removeOne(client);
	addEnpointAsIdentified(client);
	cout << "[ClientManager] Server " << client->remoteName().toStdString() << " identified us." << endl;

}

void ClientManager::serverFailedToIdentifyUs(ConnectionError connectionError)
{
	MessageClient* client = (MessageClient*) sender();
	emit failedToConnectToEndpoint(client, connectionError);

	mClientsThatNeedToIdentify.removeOne(client);
	client->deleteLater();
}

void ClientManager::clientIdentified()
{
	MessageServer* server = (MessageServer*) sender();
	mServersThatNeedIdentification.removeOne(server);

	cout << "[ClientManager] Client " << server->remoteName().toStdString() << " has been identified." << endl;

	addEnpointAsIdentified(server);
}

void ClientManager::clientFailedToIdentify(ConnectionError connectionError)
{
	MessageServer* server = (MessageServer*) sender();
	emit failedToConnectToEndpoint(server, connectionError);

	mServersThatNeedIdentification.removeOne(server);
	server->deleteLater();
}

void ClientManager::start()
{
	if(!mServerSocket->listen(QHostAddress::Any, mPort))
	{
		//Hack for testing.
		mLocalName = "Client";
		throw runtime_error("[ClientManager] Failed to listen to socket");
	}
}

void ClientManager::newClientConnected()
{
	QTcpSocket* newClient = mServerSocket->nextPendingConnection();
	newClient->open(QIODevice::ReadWrite);

	cout << "Got new client." << endl;

	MessageServer* server = new MessageServer(this, newClient);
	connect(server, SIGNAL(connectionFailed(ConnectionError)), this, SLOT(clientFailedToIdentify(ConnectionError)));
	connect(server, SIGNAL(identificationSuccesful()), this, SLOT(clientIdentified()));
	connect(server, SIGNAL(identificationFailed(ConnectionError)), this, SLOT(clientFailedToIdentify(ConnectionError)));
	mServersThatNeedIdentification << server;
	server->requestIdentification();
}

void ClientManager::connectToServer(QString serverHostname)
{
	QTcpSocket* remoteServer = new QTcpSocket();

	MessageClient* mc = new MessageClient(this, remoteServer);
	connect(mc, SIGNAL(connectionFailed(ConnectionError)), this, SLOT(serverFailedToIdentifyUs(ConnectionError)));
	connect(mc, SIGNAL(identificationSuccesful()), this, SLOT(serverIdentifiedUs()));
	connect(mc, SIGNAL(identificationFailed(ConnectionError)), this, SLOT(serverFailedToIdentifyUs(ConnectionError)));
	mClientsThatNeedToIdentify.append(mc);
	mc->connectToServer(serverHostname, mPort);
}

void ClientManager::setLocalName(QString name)
{
	mLocalName = name;
	emit localNameChanged();
}

void ClientManager::sendBroadcast()
{
	cout << "[ClientManager] Sending out broadcast for local clients." << endl;
	mBroadcastSocket->connectToHost(QHostAddress::Broadcast, mPort, QIODevice::WriteOnly);
	mBroadcastSocket->write("CONNECT_BACK");
}

void ClientManager::processDatagrams()
{
	while (mBroadcastSocket->hasPendingDatagrams()) {
		QByteArray datagram;
		datagram.resize(mBroadcastSocket->pendingDatagramSize());
		QHostAddress sender;

		mBroadcastSocket->readDatagram(datagram.data(), datagram.size(), &sender);
		QString broadcastMessage(datagram.data());
		if(broadcastMessage == "CONNECT_BACK")
		{
			connectToServer(sender.toString());
		}
		else
		{
			cout << "Ignoring broadcast message: " << broadcastMessage.toStdString() << " from " << sender.toString().toStdString() << endl;
		}
	}
}

