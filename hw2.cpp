#include <algorithm>
#include <iostream>
#include <vector>
#include "Timer.h"
#include "UdpSocket.h"

using namespace std;

#define PORT 23460  // my UDP port
#define MAX 20000   // times of message transfer
#define MAXWIN 30   // the maximum window size
#define LOOP 10     // loop in test 4 and 5

// client packet sending functions
void clientUnreliable(UdpSocket &sock, const int max, int message[]);
int clientStopWait(UdpSocket &sock, const int max, int message[]);
int clientSlidingWindow(UdpSocket &sock, const int max, int message[],
                        int windowSize);
// int clientSlowAIMD( UdpSocket &sock, const int max, int message[],
//		     int windowSize, bool rttOn );

// server packet receiving fucntions
void serverUnreliable(UdpSocket &sock, const int max, int message[]);
void serverReliable(UdpSocket &sock, const int max, int message[]);
void serverEarlyRetrans(UdpSocket &sock, const int max, int message[],
                        int windowSize);
// void serverEarlyRetrans( UdpSocket &sock, const int max, int message[],
//			 int windowSize, bool congestion );

enum myPartType { CLIENT, SERVER, ERROR } myPart;

int main(int argc, char *argv[]) {
  int message[MSGSIZE / 4];  // prepare a 1460-byte message: 1460/4 = 365 ints;
  UdpSocket sock(PORT);      // define a UDP socket

  myPart = (argc == 1) ? SERVER : CLIENT;

  if (argc != 1 && argc != 2) {
    cerr << "usage: " << argv[0] << " [serverIpName]" << endl;
    return -1;
  }

  if (myPart == CLIENT)  // I am a client and thus set my server address
    if (sock.setDestAddress(argv[1]) == false) {
      cerr << "cannot find the destination IP name: " << argv[1] << endl;
      return -1;
    }

  int testNumber;
  cerr << "Choose a testcase" << endl;
  cerr << "   1: unreliable test" << endl;
  cerr << "   2: stop-and-wait test" << endl;
  cerr << "   3: sliding windows" << endl;
  cerr << "--> ";
  cin >> testNumber;

  if (myPart == CLIENT) {
    Timer timer;          // define a timer
    int retransmits = 0;  // # retransmissions

    switch (testNumber) {
      case 1:
        timer.start();                         // start timer
        clientUnreliable(sock, MAX, message);  // actual test
        cerr << "Elasped time = ";             // lap timer
        cerr << timer.lap() << endl;
        break;
      case 2:
        timer.start();                                     // start timer
        retransmits = clientStopWait(sock, MAX, message);  // actual test
        cerr << "Elasped time = ";                         // lap timer
        cerr << timer.lap() << endl;
        cerr << "retransmits = " << retransmits << endl;
        break;
      case 3:
        for (int windowSize = 1; windowSize <= MAXWIN; windowSize++) {
          timer.start();  // start timer
          retransmits = clientSlidingWindow(sock, MAX, message,
                                            windowSize);  // actual test
          cerr << "Window size = ";                       // lap timer
          cerr << windowSize << " " << endl;
          cerr << "Elasped time = ";
          cerr << timer.lap() << endl;
          cerr << "retransmits = " << retransmits << endl;
        }
        break;
      default:
        cerr << "no such test case" << endl;
        break;
    }
  }
  if (myPart == SERVER) {
    switch (testNumber) {
      case 1:
        serverUnreliable(sock, MAX, message);
        break;
      case 2:
        serverReliable(sock, MAX, message);
        break;
      case 3:
        for (int windowSize = 1; windowSize <= MAXWIN; windowSize++) {
          serverEarlyRetrans(sock, MAX, message, windowSize);
        }
        break;
      default:
        cerr << "no such test case" << endl;
        break;
    }

    // The server should make sure that the last ack has been delivered to
    // the client. Send it three time in three seconds
    cerr << "server ending..." << endl;
    for (int i = 0; i < 10; i++) {
      sleep(1);
      int ack = MAX - 1;
      sock.ackTo((char *)&ack, sizeof(ack));
    }
  }

  cerr << "finished" << endl;

  return 0;
}

// Test 1: client unreliable message send -------------------------------------
void clientUnreliable(UdpSocket &sock, const int max, int message[]) {
  cerr << "client: unreliable test:" << endl;

  // transfer message[] max times
  for (int i = 0; i < max; i++) {
    message[0] = i;                         // message[0] has a sequence #
    sock.sendTo((char *)message, MSGSIZE);  // udp message send
    cerr << "message = " << message[0] << endl;
  }
}

// Test1: server unreliable message receive -----------------------------------
void serverUnreliable(UdpSocket &sock, const int max, int message[]) {
  cerr << "server: unreliable test:" << endl;

  // receive message[] max times
  for (int i = 0; i < max; i++) {
    sock.recvFrom((char *)message, MSGSIZE);  // udp message receive
    cerr << message[0] << endl;               // print out message
  }
}

int clientStopWait(UdpSocket &sock, const int max, int message[]) {
  cerr << "client: stop_and_wait test:" << endl;

  int resendCount = 0;
  Timer timer;
  for (int i = 0; i < max; i++) {
    message[0] = i;

    cerr << "message = " << message[0] << endl;

    sock.sendTo((char *)message, MSGSIZE / 4);

    // Variable to say if we got a response
    bool received = false;

    // Start timer
    timer.start();

    // While no response
    while (!received) {
      // If we have a response
      if (sock.pollRecvFrom() > 0) {
        sock.recvFrom((char *)&message[0], sizeof(int));
        received = true;
      }
      // Else no response yet
      else {
        // Check if we have a timeout
        if (timer.lap() > 1500) {
          // Resend the message
          sock.sendTo((char *)message, MSGSIZE / 4);

          cerr << "Retransmit:\t" << message[0] << endl;

          // Increment the number of retransmits
          resendCount++;

          // Restart the timer
          timer.start();
        }
      }
    }
  }
  return resendCount;
}

void serverReliable(UdpSocket &sock, const int max, int message[]) {
  cerr << "server: reliable test:" << endl;

  // receive message[] max times
  for (int i = 0; i < max; i++) {
    // While nothing received
    do {
      sock.recvFrom((char *)message, MSGSIZE / 4);  // udp message receive

    } while (message[0] != i);

    sock.ackTo((char *)&i, sizeof(int));  // Send ack

    cerr << "Message:\t" << message[0] << endl;
  }
}

int clientSlidingWindow(UdpSocket &sock, const int max, int message[],
                        int windowSize) {
  cerr << "client: sliding window test:" << endl;

  vector<int> sentId;
  int resendCount = 0;
  Timer timer;
  int segment = 0;

  do {
    while (sentId.size() < windowSize) {
      message[0] = segment;
      sentId.push_back(message[0]);
      segment++;
      cerr << "Message:\t" << message[0] << endl;

      sock.sendTo((char *)message, MSGSIZE / 4);

      if (sock.pollRecvFrom() > 0) {
        sock.recvFrom((char *)&message[0], sizeof(int));
        if (message[0] >= sentId[0]) {
          auto segmentAck = find(sentId.begin(), sentId.end(), message[0]);
          if (segmentAck == sentId.begin()) {
            sentId.erase(segmentAck);
          } else {
            sentId.erase(sentId.begin(), segmentAck);
          }
        }
      }
    }

    timer.start();
    while (sock.pollRecvFrom() < 1) {
      if (timer.lap() >= 1500) {
        message[0] = sentId[0];
        sock.sendTo((char *)message, MSGSIZE / 4);
        cerr << "Retransmit:\t" << message[0] << endl;
        resendCount++;
        timer.start();
      }
    }
    sock.recvFrom((char *)&message[0], sizeof(int));
    if (message[0] >= sentId[0]) {
      auto segmentAck = find(sentId.begin(), sentId.end(), message[0]);
      if (segmentAck == sentId.begin()) {
        sentId.erase(segmentAck);
      } else {
        sentId.erase(sentId.begin(), segmentAck);
      }
    }

  } while (segment < max);

  return resendCount;
}

void serverEarlyRetrans(UdpSocket &sock, const int max, int message[],
                        int windowSize) {
  cerr << "server: early retransmit test:" << endl;

  bool received[MAX] = {false};
  int lastReceived = -1, lastAck = 0;

  do {
    if (sock.pollRecvFrom() > 0) {
      sock.recvFrom((char *)message, MSGSIZE / 4);  // udp message receive
      lastReceived = message[0];

      if ((lastReceived - lastAck) < windowSize) {
        if (!received[lastReceived]) {
          received[lastReceived] = true;
        }
        while (received[lastAck]) {
          lastAck++;
        }
        lastAck = (lastAck < lastReceived) ? lastAck : lastReceived;

        sock.ackTo((char *)&lastAck, sizeof(lastAck));
        cerr << "Ack sent:\t" << lastAck << endl;
      }

      cerr << "Message:\t" << message[0] << endl;  // print out message
    }

  } while (lastAck < max);
}
