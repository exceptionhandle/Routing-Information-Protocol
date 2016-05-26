/**
 * @ubitname_assignment1
 * @author  Fullname <ubitname@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../include/packUnpack.h"
#include <vector>
#include <map>
#include <string>
#include <queue>
#include <fstream>
#include <sstream>
#define STDIN 0
#define MAXDATASIZE 1024
#define INF ((1<<16)-1)
#define resetTime(x) x->Time = x->period//time(NULL);
#define remainTime(x) x->Time//((x->Time) + (x->period)) - time(NULL)

using namespace std;

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */


 #define sendPacket(newpack, newfd) \
 { \
     int size = 0, len = sizeof(newpack), sz = 0; \
     while((sz = send(newfd, newpack + size, len, 0)) > 0) \
     { \
        size += sz; \
        len -= sz; \
        if(len <= 0) \
        { \
           break; \
        } \
     } \
 }



class HOST{
protected:
    char IP[256];
    int newfd;
    int routrListener;
    int dataListener;

public:
    char *getHost(char *IP){
        struct hostent *he;
        struct in_addr stIP;
        if(!inet_aton(IP,&stIP)){
            return NULL;
        }

        if ((he = gethostbyaddr((const void *)&stIP,strlen(IP),AF_INET) )== NULL) {
            herror("gethostbyname");
        }
        return he->h_name;
    }


    // get sockaddr, IPv4 or IPv6:
    void *get_in_addr(struct sockaddr *sa)
    {
        if (sa->sa_family == AF_INET) {
            return &(((struct sockaddr_in*)sa)->sin_addr);
        }
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }

  inline  void getadd(){

        struct hostent *he;
        struct in_addr **addr_list;
        char hostname[100];
        gethostname(hostname,1024); ///fills ip add

        if ((he = gethostbyname(hostname) )== NULL) {  // get the host info
            herror("gethostbyname");
        }

        // save IP of this host:
        addr_list = (struct in_addr **)he->h_addr_list;
        for(int i = 0; addr_list[i] != NULL; i++) {
            strcpy(IP,inet_ntoa(*addr_list[i]));
        }
    }

  inline  int createListenPort(char *ListenPORT, int connectType){
//      cout<<"Creating Socket Listener Now with Port ::"<<ListenPORT<<endl;
      int listener;
      int yes = 1;        // for setsockopt() SO_REUSEADDR, below
      int rv;
      struct addrinfo hints, *ai, *p;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = connectType;
        hints.ai_flags = AI_PASSIVE;

        if ((rv = getaddrinfo(NULL, ListenPORT, &hints, &ai)) != 0) {
  //          cout<<"FAILED :: Cannot get address info for New Listener"<<endl;
            exit(1);
        }
        for(p = ai; p != NULL; p = p->ai_next) {
            listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (listener < 0) {
                continue;
            }
            // lose the pesky "address already in use" error message
            setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
            if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
  //            cout<<"FAILED :: Cannot Bind New Listener"<<endl;
                close(listener);
                continue;
            }
            break;
        }

        if (p == NULL) {
            exit(2);
        }
        freeaddrinfo(ai); // all done with this
        if (connectType != SOCK_DGRAM && listen(listener, 10) == -1) {
  //          cout<<"FAILED :: Cannot listen with New Listener"<<endl;
            perror("listen");
            exit(3);
        }
//        cout<<"SUCCESS :: Creating Socket Listener Now"<<endl;
        return listener;
    }

};


struct IDInfo
{
  unsigned int nextHopID, DestID, DestRouterPort, DestDataPort, cost;
  unsigned long DestIP;
  long int Time, period;
  int sockfd, missedCnt;
  char DestIPstr[40];
};

  struct min_comparator {
    bool operator()(IDInfo* i, IDInfo* j) {
      return (*i).Time > (*j).Time;
    }
  };

  struct max_comparator {
    bool operator()(IDInfo* i, IDInfo* j) {
      return (*i).Time < (*j).Time;
    }
  };
/************* Store neighbours and the cost ****************/
class neighbours {
  public :
   unsigned int numOfNeigh, UpdateInterval;
   struct IDInfo* OwnIDinfo;
   map<long, struct IDInfo*> DestbyIDInfo;
   map<long, struct IDInfo*> OriginalDestbyIDInfo;
   map<long, struct IDInfo*> DestbyIPInfo;
   unsigned char lastSentPack[1036];
   unsigned char secLastSentPack[1036];
   unsigned long remainTimer;

  neighbours()
  {
     remainTimer = (1<<30);
     memset(lastSentPack, 0, 1036);
     memset(secLastSentPack, 0, 1036);
     OwnIDinfo = new (struct IDInfo)();
//     cout<<" Allocated space :: OwnIDInfo **"<<endl;
  }
//  priority_queue<IDInfo*, vector<IDInfo*>, min_comparator> MinHeap_MinTimeFor_Timer_Restrt;
//  priority_queue<IDInfo*, vector<IDInfo*>, max_comparator> MaxHeap_To_set_Router_INF;

inline   int connectUDP(char *IP, int port)
  {
//    cout<<" Connecting UDP connection for IP ::"<<IP<<" ***"<<endl;
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_DGRAM;

    char prt[100];
    sprintf(prt, "%d", port);
//    cout<<"With IP Port Number ::"<<prt<<endl;
    if ((getaddrinfo((const char*)IP, prt, &hints, &res)) != 0) {
        return -1;
    }
    if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
    {
       return -1;
    }
    connect(sockfd, res->ai_addr, res->ai_addrlen);
    return sockfd;
  }

//*************** NOOB way to RESORT ***************//
/*  void refreshHeap()
  {
    struct IDInfo* x;
    cout<<"RESET_HEAP ::Going to check top of the Heap"<<endl;
    if(!MaxHeap_To_set_Router_INF.empty())
    {
      x = MaxHeap_To_set_Router_INF.top();
      cout<<"RESET_HEAP ::Extracted Top value"<<endl;
      MaxHeap_To_set_Router_INF.pop();
      MaxHeap_To_set_Router_INF.push(x);
    }
    if(!MinHeap_MinTimeFor_Timer_Restrt.empty())
    {
      x = MinHeap_MinTimeFor_Timer_Restrt.top();
      MinHeap_MinTimeFor_Timer_Restrt.pop();
      MinHeap_MinTimeFor_Timer_Restrt.push(x);
    }
     cout<<"RESET_HEAP ::Elements in HEAP refreshed"<<endl;
  }
*/
// *************************  Work Done by Reset Timer *************************
/* 1. If not a neighbour Don't Check
   2. Renew Time remaining for each Node
   3. Send Routing Table If Self TimeOut
   4. Check for Neighbour Router Not sending regular update
   5. Restart Timer with minimum timer
*/
inline  void resetTimers(int TIME_INTERRUPT)
  {
    // ****************** TODO :: GET SURE MAX HEAP IS SORTED **********//
//    cout<<"RESET_TIMER ::Going to Reset Heap"<<endl;
//    refreshHeap();
//    cout<<"RESET_TIMER ::HEAP HAS BEEN RESET"<<endl;

    for(map<long, struct IDInfo*>::iterator it = DestbyIDInfo.begin(); it != DestbyIDInfo.end(); ++it)
    {
        IDInfo* temp = ((*it).second);
        if(temp->period >= INF || temp->Time >= INF) // Router Not my neighbour Dont Reset its router
        {
           continue;
        }

        temp->Time -= TIME_INTERRUPT;
//        cout<<" TEMP TIME ::"<<(temp->Time)<<" "<<TIME_INTERRUPT<<endl;
        if(temp->cost <= 0)
        {
            ifTimeToSendRouting(temp);
        }
        else
        {
            checkTimerMissed(temp);
        }

        if(temp->Time <= 0)
        {
  //          cout<<"RESET_TIMER ::Reset Time of the Router ::"<<(*temp).DestID<<" to Time ::"<<(temp->period)<<endl;
            temp->Time = temp->period;
            ((*it).second) = temp;
        }
        if(remainTimer > temp->Time)
        {
            remainTimer = temp->Time;
        }
    }
  }

  //*************** Timer Interrupt 1. If need to send routing info 2. If any neighbours timer expire ******************//
  inline int TimerInterrupt(int TIME_INTERRUPT)
  {
//     cout<<"TIMER_INTERRUPT :: RESET ALL TIMERS"<<endl;
     resetTimers(TIME_INTERRUPT);
//     cout<<"TIMER_INTERRUPT :: ALL TIMERS HAVE BEEN RESET"<<endl;
     if(OwnIDinfo->period == 0)
     {
    //    cout<<"TIMER_INTERRUPT :: INIT NOT YET RECEIVED"<<endl;
        return (1<<30);
     }
     return getMinTimer();
  }

inline  void ifTimeToSendRouting(IDInfo* node)
  {
//      cout<<endl<<"START ::IF_TIME_TO_SEND_TABLE :: CHECK TIME TO SEND ROUTING ::"<<(node->Time)<<endl;
     if( node->Time <= 0 )
     {
        sendRoutingTableToNeigh();
        node->Time = node->period;
  //      cout<<"IF_TIME_TO_SEND_TABLE :: YES TABLE SENT ::"<<endl;
     }
  //   cout<<"END ::IF_TIME_TO_SEND_TABLE :: TIME TO SEND ROUTING TABLE"<<endl<<endl;
  }

  //******************* If time exceeded by 3 times update cost to infinity ********************//
  // TODO :: initialize missedCnt to 0 if received an input
inline  void checkTimerMissed(struct IDInfo* ID)
  {
//     cout<<"CHECK_NEIGH_TIMER :: CHECK Router ID ::"<<ID->DestID<<" MINIMUM TIMER "<<endl;
     if( remainTime(ID) <= 0)
     {
        if( (*ID).missedCnt == 3)
        {
          OriginalDestbyIDInfo[ID->DestID]->cost = INF;
          OriginalDestbyIDInfo[ID->DestID]->nextHopID = INF;
          if((*ID).nextHopID == (*ID).DestID)
          {
              (*ID).cost = INF;
              (*ID).nextHopID = INF;
          }
    //       cout<<"CHECK_NEIGH_TIMER :: YES Timer Missed 3 Times :: Set Cost to INF for ID ::"<<(*ID).DestID<<endl;
        }
        else
        {
            (*ID).missedCnt++;
  //          cout<<"CHECK_NEIGH_TIMER :: YES Timer Missed for ID ::"<<(*ID).DestID<<" Current MISS Count ::"<<(*ID).missedCnt<<endl;
        }
     }
  }

  //****************** Get the timer of IDInfo object with minimum time remaining to set select Timer *********************//
  inline long int getMinTimer()
  {
/*    refreshHeap();
     cout<<"START :: GET MIN TIME"<<endl;
     if(MinHeap_MinTimeFor_Timer_Restrt.empty())
     {
        cout<<"GET_MIN_HEAP :: MIN HEAP EMPTY"<<endl;
        return 0;
     }
     IDInfo* ID = MinHeap_MinTimeFor_Timer_Restrt.top();
     unsigned int remain = remainTime(ID);
     if( remain >= INF )
     {
        cout<<"GET_MIN_HEAP :: FOUND INFINITY for ID ::"<<(*ID).DestID<<" TIME :: "<<(*ID).Time<<endl;
        return 0;
     }
     cout<<"END :: GET_MIN_HEAP ::"<<remain<<endl;
*//*     if( remain <= 0)
     {
        resetTime(ID);
        remain = remainTime(ID);
        cout<<" Remain Time after update ::"<<remain<<endl;
     }
*/     return remainTimer;
  }

  //************* Get Next hop ID to reach Destination IP **************//
  inline unsigned long nextHopID(unsigned long ID)
  {
     return DestbyIDInfo[ID]->nextHopID;
  }

  // ************ Create Object for each ID and map them all **** Done only once initially ******** //
  // 1. Create Object of Each Node in network
  // 2. Start Timer for self
  // 3. If Cost is INF set nextHop to INF else to self
  // 4. Make UDP connection for each neighbour and store sockfd in object, Done Once
  // 5. Add in minHeap to find minimum time Object to help start Timer accordingly
  // 6. Initially timer will be set to the time period of Self Timer.
  // 7. Initially all other Objects will have INF Time
  // 8. Initialize missedCnt to 0 for all neighbours
  inline void CreateNMapID(char *payload, unsigned int UpdateInterval)
  {
     unsigned long ID = 0, RPort = 0, DPort = 0, Cost = 0, IP = 0;
     unpack((unsigned char*)payload, (char*)"HHHHL", &ID, &RPort, &DPort, &Cost, &IP);

//     cout<<endl<<"HASHING :: ROUTING INFORMATION *"<<endl;
     struct IDInfo* IInfo = (struct IDInfo*)malloc(sizeof(struct IDInfo));
     IInfo->DestID = ID;
     sprintf(IInfo->DestIPstr,"%d.%d.%d.%d", ((IP>>24)&((1<<8)-1)), ((IP>>16)&((1<<8)-1)), ((IP>>8)&((1<<8)-1)), (IP&((1<<8)-1)));
//     cout<<"HASHING :: IP address ::"<<(IInfo->DestIPstr)<<endl;
     IInfo->DestRouterPort = RPort;
     IInfo->DestDataPort = DPort;
     IInfo->cost = Cost;
     IInfo->DestIP = IP;
//     cout<<"HASHING :: IP address ::"<<(IInfo->DestIP)<<endl;
//     cout<<"HASHING :: ID ::"<<(IInfo->DestID)<<endl;
//     cout<<"HASHING :: Cost :"<<Cost<<endl;
     IInfo->Time = INF;
     IInfo->period = INF;
     IInfo->nextHopID = ID;
     if(Cost == INF)
     {
        IInfo->nextHopID = INF;
     }
     else if(Cost == 0) // TODO :: WHEN NEW ROUTING INFORMATION FROM THE NEIGHBOUR IS RECEIVED THEN UPDATE THE PERIOD FOR EACH
     {
        IInfo->period = UpdateInterval;
        OwnIDinfo = IInfo;
        resetTime(OwnIDinfo);
//        cout<<"HASHING :: UPDATE TIME ::"<<(IInfo->period)<<endl;
//        addInHeap(IInfo);
     }
     else
     {
        IInfo->sockfd = connectUDP(IInfo->DestIPstr, RPort);
     }
     DestbyIDInfo[ID] = IInfo;
     DestbyIPInfo[IP] = IInfo;
     copyToMyNet(IInfo);
//     cout<<"END :: HASHING :: ROUTING INFORMATION *"<<endl<<endl;
  }

  void copyToMyNet(struct IDInfo* routerInfo)
  {
     struct IDInfo* Router = (struct IDInfo*)malloc(sizeof(struct IDInfo));
     Router->nextHopID = routerInfo->nextHopID;
     Router->DestID = routerInfo->DestID;
     Router->DestRouterPort = routerInfo->DestRouterPort;
     Router->DestDataPort = routerInfo->DestDataPort;
     Router->cost = routerInfo->cost;
     Router->DestIP = routerInfo->DestIP;
     Router->Time = routerInfo->Time;
     Router->period = routerInfo->period;
     Router->sockfd = routerInfo->sockfd;
     Router->missedCnt = routerInfo->missedCnt;
     memcpy(Router->DestIPstr,routerInfo,40);
     OriginalDestbyIDInfo[routerInfo->DestID] = Router;

  }
/*
  void addInHeap(struct IDInfo* IInfo)
  {
      MinHeap_MinTimeFor_Timer_Restrt.push(IInfo);
      MaxHeap_To_set_Router_INF.push(IInfo);
  }
*/
  void sendRoutingTable(unsigned long controlIP, unsigned int controlCode, int newfd)
  {
  //   cout<<"REPLY CONTROLLER :: ROUTING TABLE *"<<endl;
     unsigned char newpack[1000];
     const unsigned long zeropad = 0;
     //****************** number of 2 byte strings *******************//
     unsigned long lenOfPayLoad = DestbyIDInfo.size() * 4 * 2;
  //   cout<<"REPLY CONTROLLER :: size of Payload :"<<lenOfPayLoad<<endl;
     int size = pack(newpack, (char*)"LCCH", controlIP, controlCode, zeropad, lenOfPayLoad);
    //  int size = pack(newpack, (char*)"LHH", controlIP, zeropad, lenOfPayLoad);
     //sendPacket(newpack, newfd);
     int sz = 0;
     for(map<long, struct IDInfo*>::iterator it = DestbyIDInfo.begin(); it != DestbyIDInfo.end(); ++it)
     {
       size += sz;
       unsigned char *packet = newpack + size;
  //     cout<<"REPLY CONTROLLER ::Sending content ::"<<(((*it).second)->DestID)<< " "<<zeropad<< " "<<(((*it).second)->nextHopID)<<" "<< (((*it).second)->cost)<<endl<<endl;
       sz = pack(packet, (char*)"HHHH", ((*it).second)->DestID, zeropad, ((*it).second)->nextHopID, ((*it).second)->cost);
//       sendPacket(packet, newfd);
//       cout<<"REPLY CONTROLLER :: Content of Packet containing Forwarding Row :"<<endl;
       printf("%x %x %x %x\n", *packet, *(packet + 1), *(packet + 2), *(packet + 3));
       printf("%x %x %x %x\n", *(packet + 4), *(packet + 5), *(packet + 6), *(packet + 7));
     }
     if(!DestbyIDInfo.empty())
        sendPacket(newpack, newfd);
//     cout<<"END :: REPLY CONTROLLER :: ROUTING TABLE ***"<<endl<<endl;
  }

  void sendRoutingTableToNeigh()
  {
//    cout<<"BROADCAST :: ROUTING TABLE TO NEIGHBOURS *"<<endl;
    unsigned char newpack[10000];
//    cout<<"BROADCAST :: Packing Routing Table and Sending to all Neighbours *"<<endl;

    unsigned long lenOfPayLoad = DestbyIDInfo.size() * 4 * 2;

//    cout<<"BROADCAST ::UDP :: size of Payload :"<<lenOfPayLoad<<endl<<endl;
    int size = pack(newpack, (char*)"LHH", (*OwnIDinfo).DestID, UpdateInterval, lenOfPayLoad);
    //sendPacket(newpack, newfd);
    int sz = 0;
    for(map<long, struct IDInfo*>::iterator it = DestbyIDInfo.begin(); it != DestbyIDInfo.end(); ++it)
    {
      size += sz;
      unsigned char *packet = newpack + size;

      sz = pack(packet, (char*)"HH", ((*it).second)->DestID, ((*it).second)->cost);

    }
    if(!DestbyIDInfo.empty())
    {
//      for(map<long, struct IDInfo*>::iterator it = DestbyIDInfo.begin(); it != DestbyIDInfo.end(); ++it)
      for(map<long, struct IDInfo*>::iterator it = OriginalDestbyIDInfo.begin(); it != OriginalDestbyIDInfo.end(); ++it)
      {
         IDInfo *temp = ((*it).second);
         if(temp->cost != 0 && temp->cost != INF)
         {
    //        cout<<"Sending Routing Table to IP ::"<<temp->DestIPstr<<" COST ::"<<temp->cost<<endl;
        //    UDPsend( temp->DestIP,  temp->DestRouterPort,  (char*)newpack,  temp->sockfd);
            sendPacket(newpack, temp->sockfd);
         }
      }
    }

//    cout<<"END ::BROADCAST :: ROUTING TABLE TO NEIGHBOURS ***"<<endl<<endl;

  }

inline  void UDPsend(int IP, int port, char *pack, int fd)
  {
    struct hostent *hp;     /* host information */
    struct sockaddr_in servaddr;    /* server address */

    /* fill in the server's address and data */
    memset((char*)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = port;


    /* put the host's address into the server address structure */
    memcpy((void *)&servaddr.sin_addr, (const void *)&IP, 32);
//    cout<<"ADDRESS PACKET SENT TO ::"<<(char*)(servaddr.sin_addr)<<endl;
    /* send a message to the server */
    if (sendto(fd, pack, strlen(pack), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
      perror("UDP_SEND ::sendto failed");
      return ;
    }
  }

inline  void save_The_Packet(unsigned char *newpack)
  {

     memcpy(secLastSentPack, lastSentPack, 1036);
     memcpy(lastSentPack, newpack, 1036);
  }

inline  unsigned char* getLastSent()
  {
     return lastSentPack;
  }

inline  unsigned char* getSecLastSent()
  {
     return secLastSentPack;
  }

// ************* Set timer for the Routing table update for the Router ***//
  void setTimer(long senderID, unsigned long timer)
  {
  //   cout<<"SET_TIMER ::Set timer for the Routing table update for the Router "<<endl<<endl;
     if(DestbyIDInfo.find(senderID) != DestbyIDInfo.end())
     {
       struct IDInfo* NodeInfo = DestbyIDInfo[senderID];
       NodeInfo->period = timer;
       NodeInfo->Time = timer;
//       MaxHeap_To_set_Router_INF.push(NodeInfo);
//       MinHeap_MinTimeFor_Timer_Restrt.push(NodeInfo);
  //     cout<<"SET_TIMER :: TIMER SET FOR ID ::"<<(NodeInfo->period)<<endl<<endl;
     }
  }

// **************** Check if any node is reachable with lower cost through this sender ******//
  void findShortest(unsigned char *payload, long senderID)
  {
  //  cout<<"FIND_SHORTEST :: UPDATE COST"<<endl;
    int costToSender;
    if(DestbyIDInfo.find(senderID) != DestbyIDInfo.end())
    {
  //     cout<<"FIND_SHORTEST ::UDP :: Sender ID ::"<<senderID<<endl;
       struct IDInfo* newNextHopInfo = DestbyIDInfo[senderID];
       costToSender = (newNextHopInfo->cost);
    }
    else
    {
  //     cout<<"FIND_SHORTEST ::NOT FOUND senderID ::"<<senderID<<endl;
       return;
    }

    for(int i = 0; i < numOfNeigh; i++)
    {
        unsigned int DestID, destCost;
  //      cout<<"FIND_SHORTEST ::UDP :: UNPACK :: PAYLOAD "<<endl;
        unpack(payload, (char*)"HH", &DestID, &destCost);
        payload += 4;
        int oldCost;
        struct IDInfo* NodeInfo;

        if(DestbyIDInfo.find(DestID) != DestbyIDInfo.end())
        {
  //        cout<<"FIND_SHORTEST ::UDP :: FOUND :: Destination ID ::"<<DestID<<endl;
          NodeInfo = DestbyIDInfo[DestID];
          oldCost = NodeInfo->cost;
        }
        else
        {
  //        cout<<"FIND_SHORTEST ::UDP :: NOT_FOUND :: Destination ID ::"<<DestID<<endl;
           continue;
        }
        int newCost = destCost + costToSender;
  //      cout<<"FIND_SHORTEST ::UDP :: DESTID ::"<<DestID<<" MY COST ::"<<oldCost<<" COST VIA ROUTER ::"<<newCost<<endl;

        // UPDATE THE COST IF THE OLD NEXTHOP ID IS THIS SENDER
        if(newCost < oldCost || (NodeInfo->nextHopID == senderID) && newCost > oldCost)
        {
    //        cout<<"FIND_SHORTEST ::UDP ::Cost Updated for ID ::"<<DestID<<" Previous Cost ::"<<oldCost<<" New Cost ::"<<newCost<<endl<<endl;
    //        cout<<"FIND_SHORTEST ::UDP ::IF THE COST RISEN THEN DON'T PANIC. COST MUST HAVE BEEN UPDATED"<<endl;
            NodeInfo->nextHopID = senderID;
            NodeInfo->cost = newCost;
            DestbyIPInfo[NodeInfo->DestIP] = NodeInfo;
            DestbyIDInfo[NodeInfo->DestIP] = NodeInfo;

        }
        if(OriginalDestbyIDInfo[DestID]->cost < NodeInfo->cost)
        {
    //        cout<<"FIND_SHORTEST ::UPDATE COST BACK TO ORIGINAL ID ::"<<DestID<<" Previous Cost ::"<<oldCost<<" New Cost ::"<<newCost<<endl<<endl;
            NodeInfo->cost = OriginalDestbyIDInfo[DestID]->cost;
        }
    }

  }

  //************* Initialize for first time **************//
  void initTable(char *payload)
  {
    unpack((unsigned char*)payload, (char*)"HH",  &numOfNeigh, &UpdateInterval);
    payload += 4;
//    cout<<endl<<"INIT_TABLE ::Start :: Routing Table Contents"<<endl;
  //  cout<<endl<<"INIT_TABLE ::Number of Neighbours :"<<numOfNeigh<<endl;
  //  cout<<"INIT_TABLE ::Update Interval :"<<UpdateInterval<<endl<<endl;

    for(int i = 0; i < numOfNeigh; i++)
    {
       CreateNMapID(payload, UpdateInterval);
       payload += 12;
    }
//    printNeigh();
  }

  void updatePath(char *payload)
  {
  //  cout<<endl<<"UPDATE_PATH ::Start :: Updating Path *"<<endl;
    unsigned int ID = 0, pathCost = 0;
    unpack((unsigned char*)payload, (char*)"HH",  &ID, &pathCost);
    if(DestbyIDInfo.find(ID)==DestbyIDInfo.end())
    {
//       cout<<"UPDATE_PATH ::Exception :: Cannot Find the Destination ID Information !!!!!!!!!!!"<<endl;
//       cout<<"UPDATE_PATH ::UNSUCCESS :: Update"<<endl;
       return;
    }
    DestbyIDInfo[ID]->cost = pathCost;
//    cout<<endl<<"UPDATE_PATH ::ENDd :: Update *"<<endl;
  }




  void printNeigh( )
  {
    for(map<long, struct IDInfo*>::iterator it = DestbyIDInfo.begin(); it != DestbyIDInfo.end(); ++it)
    {
      cout << "PRINT_NEIGH ::ID :" <<((*it).second)-> DestID<< endl;
      cout << "PRINT_NEIGH ::Router Port :" << ((*it).second)->DestRouterPort << endl;
      cout << "PRINT_NEIGH ::Data Port :" << ((*it).second)->DestDataPort << endl;
      cout << "PRINT_NEIGH ::Path Cost :"  << ((*it).second)->cost << endl;
      cout << "PRINT_NEIGH ::IP Address :"<<((*it).second)->DestIPstr<<endl<<endl;
    }
    cout<<endl<<"END :: PRINT_NEIGH ::Routing Table Contents"<<endl;
  }
};

struct transferInfo {
   unsigned int TransferID, TTL;
   char FileName[1000];
   long destRouterIP;
   vector<unsigned long> SeqNum;
};

class controlHeader {
public :
   long controlIPAdd, payLength;
   unsigned int controlCode, responseCode;
   char pload[12000];

   controlHeader()
   {
     controlIPAdd = 0;
     controlCode = 0;
     responseCode = 0;
   }

   void print()
   {
      cout << endl << "CONTROL HEADER ::PRINT ::Control Header Information "<<endl;
      cout <<"CONTROL HEADER ::PRINT ::Controller IP Address :"<<controlIPAdd<<endl;
      cout <<"CONTROL HEADER ::PRINT ::Control Code :"<<controlCode<<endl;
      cout <<"CONTROL HEADER ::PRINT ::Response Code :"<<responseCode<<endl;
      cout <<"CONTROL HEADER ::PRINT ::Payload Packet packed Content :"<<pload<<endl<<endl;
   }
};
unsigned remotefd = -1;
unsigned TCPdatafd = -1;
int TIME_INTERRUPT = 1;

class RegisServer : public HOST{
  public :
    controlHeader CHeader;
    neighbours n;
    int newfd;
    int controListener;
    char remoteIP[INET6_ADDRSTRLEN];
     int OwnID, OwnRoutrPrt, OwnDataPrt, OwnRouterFd, OwnDataFd;
    fd_set master;
    struct timeval timeout;
    int fdmax;
    long OwnIPAdd;
    int countPack;

    vector<struct transferInfo *> StoredTransInfo;
    RegisServer(char *port){
        //FILLS SYSTEMS IP ADDRESS
        OwnRouterFd = -1;
        OwnDataFd = -1;
        getadd();
        controListener = createListenPort(port, SOCK_STREAM);
        connectServer();
        timeout.tv_sec = 1;
        countPack = 0;
    }
//**************** Added new port for Router or Data connection to Router ******************//
    int addSocketPort(int port, int connectionType)
    {
  //      cout<<endl<<"ADD_SOCKET_PORT ::Trying to create socket Listener of Type :"<<connectionType<<endl;
        char newPrt[40];
        sprintf(newPrt,"%d",port);
        int listener = createListenPort(newPrt, connectionType);
  //      cout<<"ADD_SOCKET_PORT ::Addying the new socket in the fdset *"<<endl;
        FD_SET(listener, &master);
        if (listener > fdmax)
        {
            fdmax = listener;
        }
  //      cout<<"ADD_SOCKET_PORT ::Connection END *"<<endl;
        return listener;
    }

    unsigned char *connectNRecv(int listener)
    {
  //    cout<<"CONNECT_RECEIVE ::LISTEN :: Connect and Receive the Packet "<<endl;
      unsigned char *chunk = (unsigned char *)malloc(sizeof(unsigned char)*1050);
      struct sockaddr_storage remoteaddr;
      socklen_t addrlen;
      addrlen = sizeof remoteaddr;

      newfd = accept(listener,(struct sockaddr *)&remoteaddr,&addrlen);
      if (newfd == -1)
      {
          perror("CONNECT_RECEIVE ::accept");
      }
      else
      {
          FD_SET(newfd, &master);
          if (newfd > fdmax)
          {
              fdmax = newfd;
          }
          /************** Get socket information of the connection **************/
          sockaddr_in *clientadd = (sockaddr_in *)get_in_addr((struct sockaddr*)&remoteaddr);
          /************* Convert IP address to Human Readable Form **************/
          inet_ntop(remoteaddr.ss_family, clientadd, remoteIP, INET6_ADDRSTRLEN);
          /***************** Work on the packet from Controller *****************/
    //      cout<<"CONNECT_RECEIVE ::START :: Receiving the packet"<<endl<<endl;
          int size = 0, sz = 0;
          int len = 1000;
          while((sz = recv(newfd + size , chunk , len , 0))  > 0)
          {
              size += sz;
              len -= sz;
    //          cout<<"CONNECT_RECEIVE ::SUCCESS :: Packet Received"<<endl<<endl;
          }
          unsigned int incominPrt = (*clientadd).sin_port;
  //        cout<<"CONNECT_RECEIVE ::SENDER :: IP Address ::"<<remoteIP<<endl<<endl;
  //        cout<<endl<<"SENDER :: Port Address ::"<<incominPrt<<endl;
          return chunk;
        }
  //      cout<<"END ::CONNECT_RECEIVE :: Connect and Receive the Packet "<<endl;
    }
    int TCPDataServer(int listener)
    {
//      cout<<"CONNECT_RECEIVE ::LISTEN :: Connect and Receive the Packet "<<endl;
      unsigned char *chunk = (unsigned char *)malloc(sizeof(unsigned char)*1050);
      struct sockaddr_storage remoteaddr;
      socklen_t addrlen;
      addrlen = sizeof remoteaddr;

      newfd = accept(listener,(struct sockaddr *)&remoteaddr,&addrlen);
      if (newfd == -1)
      {
          perror("CONNECT_RECEIVE ::accept");
      }
      else
      {
          FD_SET(newfd, &master);
          if (newfd > fdmax)
          {
              fdmax = newfd;
          }
          /************** Get socket information of the connection **************/
          sockaddr_in *clientadd = (sockaddr_in *)get_in_addr((struct sockaddr*)&remoteaddr);
          /************* Convert IP address to Human Readable Form **************/
          inet_ntop(remoteaddr.ss_family, clientadd, remoteIP, INET6_ADDRSTRLEN);
          unsigned int incominPrt = (*clientadd).sin_port;
  //        cout<<"CONNECT_RECEIVE ::SENDER :: IP Address ::"<<remoteIP<<endl<<endl;
  //        cout<<endl<<"SENDER :: Port Address ::"<<incominPrt<<endl;
          return newfd;
        }
//        cout<<"END ::CONNECT_RECEIVE :: Connect and Receive the Packet "<<endl;
    }


    unsigned char *receive(int fd)
    {
      unsigned char *chunk = (unsigned char *)malloc(sizeof(unsigned char)*10000000);
  //    cout<<"CONNECT_RECEIVE ::START :: Receiving the packet"<<endl<<endl;
      int size = 0, sz = 0;
      int len = 10000000;
      while((sz = recv(fd , chunk + size , len , 0))  > 0)
      {
          size += sz;
          len -= sz;
  //        cout<<"CONNECT_RECEIVE ::SUCCESS :: Packet Received"<<endl<<endl;
      }
      return chunk;

    }

    void UnPackDoBellmanFord()
    {
       // ******* start timer for the neighbour to receive timely Routing Update *//
       unsigned long timer = (CHeader.controlCode << 16) | ((CHeader.responseCode)&((1<<16)-1));
       n.setTimer(CHeader.controlIPAdd, timer);
       // ************** Here controlIPAdd = SenderID *********//
       n.findShortest((unsigned char*)CHeader.pload, CHeader.controlIPAdd);
    }


    void connectServer(){
        /********** Declarations for adding and keeping TCP connections ***********/
        int nextHopID;
        int nextHopDprt;
        char* nextHopIP;

        fd_set read_fds;
        fdmax = controListener;
        unsigned char buf[256];
        int nbytes, i, j;
        FD_ZERO(&master);    // clear the master and temp sets
        FD_ZERO(&read_fds);
        FD_SET(controListener, &master);
        char fileContent[100000000];
        for(;;) {
            read_fds = master;
            int ret;
            if ((ret = select(fdmax+1, &read_fds, NULL, NULL, &timeout)) < 0) {
                perror("Select");
                exit(4);
            }
/*            if(ret == 0)
            {
                cout<<endl<<"SERVER ::Socket Closed"<<endl;
                close(newfd);
            }
*/
//              cout<<endl<<"SERVER :: TIME OUT :: 1 sec COMPLETE"<< ret<<endl<<endl;
              n.TimerInterrupt(1);
//              cout<<"SERVER :: TIMER ::"<< timeout.tv_sec<<endl<<endl;

//            memset( (void *)&buf, '\0', sizeof(buf));
            // ************** Check if Any timer Exceeds ********//

            // *********** Check if Any Socket FD read is set ***//
//            if (FD_ISSET(OwnRouterFd, &read_fds)) {
            unsigned char chunk[1036];
            struct sockaddr_storage their_addr;
            socklen_t addr_len = sizeof their_addr;
            cout << "SERVER ::UDP Router Port :: Check If Packet Incoming" << endl<<endl;
            if (OwnRouterFd != -1 && FD_ISSET(OwnRouterFd, &read_fds) && recvfrom(OwnRouterFd, chunk, 1036, 0, (struct sockaddr *)&their_addr, &addr_len) >= 0)
            // if (recv(OwnRouterFd, chunk, 1000, 0)>=0)//, (struct sockaddr *)&their_addr, &addr_len) >= 0)
            {
                // TODO: verify the packet is an acknowledgement
                // of the packet sent above and not something else...
//                cout << "UDP :: Packet received" << endl;
//                cout<<"UDP :: Connect & Receive Packet"<<endl;
            //  unsigned int *chunk = connectNRecv();
//                cout<<"UDP :: UNPACK :: Header"<<endl;
                unpackHeader(chunk);
//                cout<<"UDP :: UNPACK :: Payload Apply BellmenFord"<<endl;
                UnPackDoBellmanFord();
                FD_CLR(OwnRouterFd, &master);
            }
            // TODO :: CHECK FIN IS 1
  //          cout << "SERVER ::TCP DATA Port :: Check If Packet Incoming MY IP ::" <<OwnIPAdd<< endl<<endl;
            if ((OwnDataFd != -1 && FD_ISSET(OwnDataFd, &read_fds))||(remotefd != -1 && FD_ISSET(remotefd, &read_fds))) {
                 cout <<endl<< "START ::TCP_DATA :: FD IS SET" << endl;
                 unsigned char *chunk;
                 if(remotefd == -1)
                 {
    //               cout<<" I am here REMOTE FD ::"<< remotefd<<endl;
                   remotefd = TCPDataServer(OwnDataFd);

                 }

    //             cout<<"REMOTE FD ::"<< remotefd<<endl;
                 chunk = receive(remotefd);
    //             cout<<"PACKET RECEIVED ::"<<chunk<<endl;
                 //close(newfd);
                 //FD_CLR(newfd,&master);
            //     cout << "TCP_DATA :: Packet received /--/" << endl;
                int i = 0;
                while(1){
                 unsigned long finpad;
                 unsigned long TransferID = 0, TTL = 0, SeqNum = 0;
                 char pload[1036];
                 unpack(chunk+1, (char*)"LCCHL", &(CHeader.controlIPAdd), &TransferID, &TTL, &SeqNum, &finpad);//, pload);
                 i += 1036;
              //   memcpy(pload,chunk+12,1024);
              //   cout<<"TCP_DATA :: SIZE OF CHUNK RECEIVED ::"<<strlen((char*)chunk)<<endl;
//                 cout<<"TCP_DATA :: IP ADDRESS OF THE DESTINATION IN THE PACKET ::"<<CHeader.controlIPAdd<<" SELF IP ADDRESS ::"<<OwnIPAdd<<" Finpad ::"<<finpad<<" TTL ::"<<TTL<<" TransferID ::"<<TransferID<<" Seq Num"<<SeqNum<<endl;
                 if(CHeader.controlIPAdd == OwnIPAdd)
                 {
                    TTL--;
                    n.save_The_Packet(chunk);
    //                cout<<"File :: Destined to ME"<<endl<<endl;
                    //pload[strlen(pload)] = '\0';
                    strcat(fileContent, (char*)chunk+12);//pload);
                    if(finpad != 0)
                    {
                      FD_CLR(remotefd, &master);
                      close(remotefd);
                      remotefd = -1;
                          cout<<"FIN FOUND SET"<<endl;

                      recvFile(fileContent, TransferID);
                      //recvFile((char*)fileContent.c_str(), TransferID);
//                      close(remotefd);
//                      FD_CLR(remotefd);
  //                    FD_CLR(remotefd, &master);
//                      continue;
                      break;
                    }
                 }
                 else
                 {
        //            cout<<"TCP_DATA :: File :: Not Destined to ME so Forward"<<endl<<endl;
                    if(TCPdatafd == -1)
                    {
                      nextHopID = (n.DestbyIPInfo[CHeader.controlIPAdd])->nextHopID;
                      nextHopDprt = (n.DestbyIDInfo[nextHopID])->DestDataPort;
                      nextHopIP = (n.DestbyIDInfo[nextHopID])->DestIPstr;

                    }
      //              cout<<"nextHopIP ::"<<nextHopIP<<" nextHopDprt ::"<<nextHopDprt<<endl;
                    // *************** Decrease TTL and Forward *****//
                    TTL -= 1;
                    // ************* If TTL become Zero Drop it ****//
                    if(TTL == 0)
                    {
    //                   cout<<"TCP_DATA :: TTL IS ZERO, DROP IT !!"<<endl;
                       continue;
                    }
                    n.save_The_Packet(chunk);
                    pack(chunk+5, (char*)"C", TTL);

                    //pack((unsigned char *)chunk, (char*)"LCCHLs", (CHeader.controlIPAdd), TransferID, TTL, SeqNum, finpad, pload);
                    if(TCPdatafd == -1)
                    {
                       TCPdatafd = TCPSendPack( nextHopDprt, nextHopIP);
                    }
                    sendTCP(TCPdatafd, chunk);
                    if(finpad != 0)
                    {
//                      cout<<"FIN FOUND SET"<<endl;
//                      FD_CLR(TCPdatafd, &master);
                      //close(OwnDataFd);
                      FD_CLR(remotefd, &master);
                      close(remotefd);
                      remotefd = -1;
                      close(TCPdatafd);
                      TCPdatafd = -1;
                      remotefd = -1;
//                      cout<<"FIN FOUND SET 2"<<endl;
                       break;
                    }

                   }
                 }
//                  TCPSendPack((unsigned char*)chunk, nextHopDprt, nextHopIP);

//               exit(0);
  //             cout<<"END :: TCP_DATA"<<endl<<endl;
//               close(newfd);
            }
            if (FD_ISSET(controListener, &read_fds)) {
                /********* Listen on the Controller Port **********/
                    unsigned char *chunk = connectNRecv(controListener);
    //                cout<<"CONTROLLER :: START :: Controller Packet Check *//"<<endl<<endl;
                    unpackHeader((unsigned char *)chunk);

    //                cout<<endl<<"UNPACK :: CONTROLLER :: Check Control Code, Unpack Payload and Respond"<<endl;
                    unpackPayload();
    //                cout<<endl<<"SUCCESS :: CONTROLLER :: Payload Checked"<<endl;

  //                  cout<<endl<<"// ******* END :: CONTROLLER :: Packet Check ********* //"<<endl<<endl;
                    FD_CLR(newfd, &master);
            }

        } // END handle data from client

    } // END for(;;)--and you thought it would never end!

    void recvFile(char *payload, int transID)
    {
      string fileName = "file-";
      int i = 0;
      char content[12000];
//      BreakInTwo(payload, fileName, content);
/*      for( i = 0;i<sizeof(payload);i++)
      {
         if(payload[i]== '#' && payload[i+1] == '#')
         {
            break;
         }
         fileName += payload[i];
      }
*/
      char trans[10000];
      sprintf(trans,"%d",transID);
      fileName += trans;
  //    cout<<" ---- File Found with Content length :: "<<strlen(payload)<<" ---- "<<endl<<endl;
      ofstream myfile;
      myfile.open((char*)fileName.c_str(), ofstream::out | ofstream::app);
      myfile<<(payload);//content);
      myfile.close();
    }


    void unpackHeader(unsigned char *chunk)
    {
        /********** Unpack the packet form the controller ***********/
//        cout<<endl<<"UNPACK :: Controller Header"<<endl;
/*        if(strlen((char*)chunk) < 6)
        {
          cout<<"FAILED :: SMALL PACK SIZE OF THE CONTROLLER HEADER"<<endl;
          return;
        }
*/        unpack((unsigned char *)chunk, (char*)"LCCs", &(CHeader.controlIPAdd), &(CHeader.controlCode), &(CHeader.responseCode), CHeader.pload);
//        cout<<"SUCCESS :: Unpacked the Controller Header"<<endl;
        CHeader.print();
    }
/*
    void sendFile(char *file, int Dport, char* IP, struct transferInfo* transIP)
    {

        unsigned long zeroPad = 0;
        cout<<"SEND_FILE ::PACKING FILE FOR SENDFILE "<<endl;
        unsigned char newpack[1050];
        unsigned short finAdded = 0;
        cout<<"SEND_FILE ::PACK :: HEADER FOR SENDFILE ::"<<file<<" DESTINED TO ::"<<CHeader.controlIPAdd<<" NEXT HOP IP ::"<<IP<<endl;

        string line;
        ifstream myfile(file);
        cout<<"SEND_FILE ::OPEN :: Opening File ::"<<file<<endl;
        string s = "";
        if (myfile.is_open())
        {
           cout<<"SEND_FILE ::OPENED :: Opened File ::"<<file<<endl;
           int len = 0;
           string line2;
           while ( 1 )
           {

             getline (myfile,line);
             if(!getline (myfile,line2))
             {
                finAdded = ((1<<7));
             }
              len += line.length();
              s += line;
              cout<<"SEND_FILE ::GETLINE :: Get Line from File ::"<<s<<" Length of Line ::"<<s.length()<<endl;
              if(len >= 1022)
              {
                // ************ subset 1st index is inclusive 2nd index is exclusive ************ //
                string subset = s.substr(0,1024);
                cout<<"PACKED :: SEND PACKET NOW"<<(char*)s.c_str()<<" length ::"<<strlen((char*)s.c_str())<<" SEQUENCE NUMBER ::"<<(transIP->SeqNum).back()<<" finAdded ::"<<finAdded<<endl;
                int size = pack(newpack, (char*)"LCCHCs", transIP->destRouterIP, transIP->TransferID, transIP->TTL, (transIP->SeqNum).back(), finAdded, (char*)s.c_str());
                cout<<"PACK :: FILE SIZE ::"<<size<<endl;
                TCPSendPack(newpack, Dport, IP);
                // ***************** 1 argument subset is the first index to the end of the string ******************* //
                s = s.substr(1025);
              }
              line = line2;
              if(finAdded)
              {
                 break;
              }
           }
           if(s.length() > 0)
           {
             cout<<"PACKED :: SEND PACKET NOW"<<(char*)s.c_str()<<" length ::"<<strlen((char*)s.c_str())<<" SEQUENCE NUMBER ::"<<(transIP->SeqNum).back()<<" finAdded ::"<<finAdded<<endl;
             int size = pack(newpack, (char*)"LCCHCs", transIP->destRouterIP, transIP->TransferID, transIP->TTL, (transIP->SeqNum).back(), finAdded, (char*)s.c_str());
             cout<<"PACK :: FILE SIZE ::"<<size<<endl;
             TCPSendPack(newpack, Dport, IP);
           }
           myfile.close();
        }
    }
*/
    void sendFile(char *file, int Dport, char* IP, struct transferInfo* transIP)
    {
        cout<<"SEND_FILE ::PACKING FILE FOR SENDFILE "<<endl;
        ifstream inFile;
        inFile.open(file);//open the input file

        stringstream strStream;
        strStream << inFile.rdbuf();//read the file
        //string s = strStream.str();//str holds the content of the file
       const std::string& tmp = strStream.str();
        const char* str = tmp.c_str();
        unsigned long zeroPad = 0;
        int countOfPack = 0;
        unsigned char newpack[1036];
        unsigned char packed[10000000];
        unsigned long finAdded = 0;
        int sockfd = TCPSendPack( Dport, IP);

        cout<<"SEND_FILE ::PACK :: HEADER FOR SENDFILE ::"<<file<<" DESTINED TO ::"<<CHeader.controlIPAdd<<" NEXT HOP IP ::"<<IP<<endl;
        int size = pack(newpack, (char*)"LCC", transIP->destRouterIP, transIP->TransferID, transIP->TTL);
      //  char str[100000000];
      //  strcpy(str, (char*)s.c_str());
        int totalLen = strlen(str);
        int i = 0;
        unsigned long seq = (transIP->SeqNum).back();
        int maxsz = 1024;


        int j = 0;

        while(i < totalLen)
        {
          // ************ subset 1st index is inclusive 2nd index is exclusive ************ //
/*          string subset;
          if(s.length()>=1022)
          {
              subset = s.substr(0,1022);
              s = s.substr(1022);
          }
          else
          {
              subset = s;
              s = "";
              finAdded = ((1<<7));
          }
  */
            if(totalLen - i <= 1024)
            {
               maxsz = totalLen - i;
               finAdded = (1<<31);
//               cout<<"SETTING FIN ::"<<finAdded<<" maxsz ::"<<maxsz<<endl;
            }
//            char subset[1024];
//            memcpy(subset, );
//          cout<<"SEND_FILE ::PACKED :: SEND PACKET NOW ::to ::"<<(transIP->destRouterIP)<<" length ::"<<strlen((char*)subset.c_str())<<" SEQUENCE NUMBER ::"<<(transIP->SeqNum).back()<<" finAdded ::"<<finAdded<<endl;
          cout<<"SEND_FILE ::PACK :: SEQ ::"<<seq<<endl;

//          memcpy(newpack+12,(char*)subset.c_str(),1024);
          size = pack(newpack+6, (char*)"HL", seq, finAdded);//, subset);//(char*)subset.c_str());
          // ***************** 1 argument subset is the first index to the end of the string ******************* //
          memcpy(newpack+12, str+i, maxsz);
          memset(newpack+maxsz, 0, 1024-maxsz);
          memcpy(packed+i+j,newpack,1036);
          j += 12;
//          countOfPack++;
          seq++;
          (transIP->SeqNum).push_back(seq);
          if(totalLen - i < 2048)
          {
            memcpy(n.secLastSentPack, n.lastSentPack, 1036);
            memcpy(n.lastSentPack, newpack, 1036);

      //      n.save_The_Packet(newpack);
          }
        //  cout<<" STRING ::"<<(newpack+12)<<endl;
          if(finAdded > 0)
          {
             break;
          }
          i += 1024;
        }
          cout<<" STRING ::"<<(newpack+12)<<endl;
        sending(sockfd, packed, i+j);
        pack(newpack, (char*)"LCC", CHeader.controlIPAdd, CHeader.controlCode,zeroPad);
        //pack(newpack, (char*)"LL", CHeader.controlIPAdd, zeroPad);
        sendPacket(newpack, newfd);

        close(sockfd);
      //  cout<<"SEND_FILE ::"<<"NUMBER OF PACKETS ::"<<countOfPack<<endl;
  //      inFile.close();
    }

         void sending(int sockfd, unsigned char* newpack, int i)
        {
          int size = 0, len = i, sz = 0;
          while((sz = send(sockfd, newpack + size, len, 0)) > 0)
          {
             size += sz;
             len -= sz;
    //         cout<<"SUCCESS :: Sending ::"<<(newpack)<<endl;
          }

    //      cout<<"END :: FILE DATA CONTENT SENT"<<endl;

        }

             void sendTCP(int sockfd, unsigned char* newpack)
            {
              int size = 0, len = 1036, sz = 0;
              if((sz = send(sockfd, newpack + size, len, 0)) > 0)
              {
                 size += sz;
                 len -= sz;
        //         cout<<"SUCCESS :: Sending ::"<<(newpack)<<endl;
              }

        //      cout<<"END :: FILE DATA CONTENT SENT"<<endl;

            }
    int TCPSendPack(int port, char *IP)//, char* packd)
    {
        int sockfd, numbytes;
        struct addrinfo hints, *servinfo, *p;
        int rv;
        char s[INET6_ADDRSTRLEN];

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        unsigned char PORT[2];
        sprintf((char*)PORT,"%d",port);
    //    cout<<"*** TCP DATA CONNECTION ON IP ::"<<IP<<" PORT ::"<<PORT<<" WHILE WHAT I RECEIVED WAS IP ::"<<IP<<" PORT ::"<<port<<endl;
//        unsigned char ip[100];
//        pack(ip,(char*)"L",IP);
        if ((rv = getaddrinfo(IP, (char*)PORT, &hints, &servinfo)) != 0) {
            fprintf(stderr, "TCP DATA GET_ADDR_INFO: %s\n", gai_strerror(rv));
            return 0;
        }

        // loop through all the results and connect to the first we can
        for(p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd = socket(p->ai_family, p->ai_socktype,
              p->ai_protocol)) == -1) {
                fprintf(stderr, "FAILED :: TCP DATA PORT SOCKET CONNECTION\n");
                continue;
            }

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
                close(sockfd);
                fprintf(stderr, "FAILED :: TCP DATA PORT CONNECTION\n");
                continue;
            }

            break;
        }

        if (p == NULL) {
            fprintf(stderr, "FAILED :: TCP DATA PORT CANNOT BE CONNECTED\n");
            return 0;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
           s, sizeof s);
        printf("TCP DATA CLIENT :: connecting to %s\n", s);

        freeaddrinfo(servinfo); // all done with this structure
        return sockfd;

    }

    void unpackPayload()
    {
      unsigned char newpack[100000];
      unsigned long zeroPad = 0;
      switch(CHeader.controlCode)
      {
        /*************** Create Packet for the Author **************/
         case 0:
         {
    //       cout<<"//************ ControlCode :: 0 Author *"<<endl;
           char *payload = (char*)"I, keshavsh, have read and understood the course academic integrity policy.";
  //         cout<<"Payload Size :"<<strlen(payload)<<endl;
           pack(newpack, (char*)"LCCs", CHeader.controlIPAdd, CHeader.controlCode,zeroPad,payload);
           sendPacket(newpack, newfd);
           break;
         }
         /************* Create Routing Table and send empty Packet to Controller ****************/
        case 1:
        {
//             cout<<"//************ ControlCode :: 1 INIT Routing Table *"<<endl;
             n.initTable(CHeader.pload);
             pack(newpack, (char*)"LCC", CHeader.controlIPAdd, CHeader.controlCode,zeroPad);
             sendPacket(newpack, newfd);
             OwnRoutrPrt = (n.OwnIDinfo)->DestRouterPort; OwnDataPrt = (n.OwnIDinfo)->DestDataPort;
             OwnID = (n.OwnIDinfo)->DestID;
             OwnIPAdd = (n.OwnIDinfo)->DestIP;
            // TODO :: Check For Routing Updates .. It's Doutful
             //************* Add Socket Port for Own Router Port **************//
             if(OwnRouterFd == -1)
                OwnRouterFd = addSocketPort(OwnRoutrPrt, SOCK_DGRAM);
             //************* Add Socket Port for Own Data Port **************//
             if(OwnDataFd == -1)
                OwnDataFd = addSocketPort(OwnDataPrt, SOCK_STREAM);
  //           cout<<"Router Address of current router :"<<OwnRoutrPrt<<endl;
  //           cout<<"Data Address of current router :"<<OwnDataPrt<<endl;
             timeout.tv_sec = n.OwnIDinfo->Time;//n.getMinTimer();
             n.sendRoutingTableToNeigh();
             break;
        }
        /************* Request for Forwarding Table from Controller ****************/
        case 2:
        {
    //         cout<<"//************ ControlCode :: 2 Send Routing Table to Controller *"<<endl;
             n.sendRoutingTable(CHeader.controlIPAdd, CHeader.controlCode, newfd);
             break;
        }
        /************* Update Cost and send empty Packet to Controller ****************/
        case 3:
        {
  //        cout<<"//************ ControlCode :: 3 Update Cost  *"<<endl;
             n.updatePath(CHeader.pload);
             //pack(newpack, (char*)"LL", CHeader.controlIPAdd, zeroPad);
             pack(newpack, (char*)"LCC", CHeader.controlIPAdd, CHeader.controlCode,zeroPad);
             sendPacket(newpack, newfd);
             break;
        }
        /********** Exit to show Crash and send empty Packet to Controller *************/
        case 4:
        {
    //         cout<<"//************ ControlCode :: 4 Crash *"<<endl;
             //pack(newpack, (char*)"LL", CHeader.controlIPAdd, zeroPad);
             pack(newpack, (char*)"LCC", CHeader.controlIPAdd, CHeader.controlCode,zeroPad);
             sendPacket(newpack, newfd);
             exit(0);
             break;
        }
        // ********** Sendfile to the Router ID and send empty Packet to Controller ***//
        // ************** TODO :: Send File to Neighbour ******//
        // ************* TODO :: send the response message after the packet with the FIN bit set (last packet) has been sent to the next hop.
        case 5:
        {
//             cout<<endl<<"CASE 5 :: ControlCode :: 5 SendFile"<<endl;
             struct transferInfo *transIT = createTransPack();
  //           cout<<"CASE 5 :: Storing Transfer info *"<<endl;
             vector<struct transferInfo*>::iterator it = StoredTransInfo.begin();
  //           cout<<"CASE 5 :: Stored Transfer info *"<<endl;
             for(; it != StoredTransInfo.end(); ++it)
             {
                 if((*it)->TransferID == transIT->TransferID)
                 {
                    ((*it)->SeqNum).push_back((transIT->SeqNum).back());
    //                cout<<"CASE 5 :: "<<"Found TRANSID AND STORED THE NEW SEQ NUM"<<endl;
                    break;
                 }
             }
  //           cout<<"CASE 5 :: "<<"OUT OF THE LOOP TO FIND TRANS ID"<<endl;
             if(it == StoredTransInfo.end())
             {
  //               cout<<"CASE 5 :: "<<"OUT OF THE LOOP TO FIND trans"<<endl;
                 StoredTransInfo.push_back(transIT);
             }
  //           cout<<"CASE 5 :: "<<"CREATED AND STORED THE TRANS ID DEST IP ::"<<(transIT->destRouterIP)<<endl;

             if(n.DestbyIPInfo.empty())
             {
//                cout<<"CASE 5 :: Routing Table not Yet Initialized"<<endl;
                break;
             }
             int nextHopID = (n.DestbyIPInfo[transIT->destRouterIP])->nextHopID;
//             cout<<"CASE 5 :: "<<"NEXT HOP ID ::"<<nextHopID<<endl;
             int nextHopDPrt = (n.DestbyIDInfo[nextHopID])->DestDataPort;
  //           cout<<"CASE 5 :: "<<"NEXT HOP DPORT ::"<<nextHopDPrt<<endl;
             char* nextHopIP = (n.DestbyIDInfo[nextHopID])->DestIPstr;
  //           cout<<"CASE 5 :: "<<"NEXT HOP IP ::"<<nextHopIP<<endl;
             unsigned long destIP = (transIT->destRouterIP);
  //           cout<<"CASE 5 :: DESTINATION IP :: "<<destIP<<endl<<endl;
  //           cout<<"CASE 5 :: NEXT HOP IP :: "<<nextHopIP<<endl<<endl;
  //           cout<<"CASE 5 :: DESTINATION DATAPORT :: "<<nextHopDPrt<<endl<<endl;
             // *************** Send to nexthop for the Destination Address *****//
             sendFile(transIT->FileName, nextHopDPrt, nextHopIP, transIT);

             // ************** Send Receipt of Sending to Controller ****//
//             cout<<"CASE 5 :: END :: Sent File And Replied to Controller *******//"<<endl<<endl;
//             exit(0);
             break;
        }
        /********** SENDFILE-STATS*************/
        case 6 :
        {
//             cout<<"CASE 6 :: ControlCode :: 6 SENDFILE-STATS *"<<endl;
             unsigned int transferID = 0;
             unpack((unsigned char*)CHeader.pload, (char*)"C", &transferID);
             vector<struct transferInfo*>::iterator it = StoredTransInfo.begin();
             for(; it != StoredTransInfo.end(); ++it)
             {
//                cout<<"CASE 6 :: ControlCode :: 6 SENDFILE-STATS inside loop (*it)->TransferID) ::"<<((*it)->TransferID)<<" transferID ::"<<transferID<<endl;
                 if(((*it)->TransferID) == transferID)
                 {
  //                  cout<<"CASE 6 ::Packing up Header inside"<<endl;
                    int size = pack(newpack, (char*)"LCC", CHeader.controlIPAdd, CHeader.controlCode,zeroPad);////////////change address to controller IP Address
                    //********************** Pack TransferID TTL padding SeqNum **************************//
                    unsigned char tempPack[100000];
                    int sz = pack(tempPack, (char*)"CCH", ((*it)->TransferID), ((*it)->TTL), zeroPad);
                    vector<unsigned long> x = ((*it)->SeqNum);
                    unsigned char *temp = (tempPack+sz);
                    for(vector<unsigned long>::iterator sit = x.begin(); sit != x.end(); ++(sit) )
                    {
  //                      cout<<"CASE 6 ::Putting Sequence Number in the packet :"<<(*sit)<<endl;
                        *temp++ = (*sit)>>8;
                        *temp++ = (*sit);
                        sz += 2;
                    }
  //                  cout<<"CASE 6 ::Total Size of Header :"<<size<<endl;
  //                  cout<<"CASE 6 ::Total Size of Payload :"<<sz<<endl;
  //                  cout<<"CASE 6 ::Total Size of Payload :"<<sz<<endl;
                    *(newpack+size) = sz>>8;
                    *(newpack+size + 1) = sz;
//                    strcpy((newpack + size + 2), tempPack, sz);
//                    pack((newpack+size), "s", tempPack);
                      memcpy(newpack+size+2, tempPack, sz);
              //      pack(newpack+size, (char*)"s",tempPack);
                    sendPacket(newpack, newfd);
//                    sendPacket(tempPack, newfd);
                    break;
                 }
             }
             break;
        }
        case 7:
        {
  //         cout<<"CASE 7 :: ControlCode :: 7 Last Sent *****"<<endl;
           unsigned char* lastSent = n.getLastSent();
           unsigned long lenOfPayLoad = strlen((const char*)lastSent);
  //         cout<<"CASE 7 :: size of Payload :"<<lenOfPayLoad<<endl;
           unsigned char packet[1000];
        //   int size = pack(packet, (char*)"LHH", OwnID, zeroPad, lenOfPayLoad);
          int size = pack(newpack, (char*)"LCC", CHeader.controlIPAdd, CHeader.controlCode,zeroPad);
          int sz = 1036;
          *(newpack+size) = sz>>8;
          *(newpack+size + 1) = sz;
          memcpy(newpack+size+2, lastSent, sz);

          ////////////change address to controller IP Address
//          *(newpack+size) = sz>>8;
  //        *(newpack+size + 1) = sz;
  //        sendPacket(packet, newfd);
           sendPacket(newpack, newfd);
           break;
        }
        case 8:
        {

           unsigned char* seclastSent = n.getSecLastSent();
           unsigned long lenOfPayLoad = strlen((const char*)seclastSent);
  //         cout<<"CASE 7 :: size of Payload :"<<lenOfPayLoad<<endl;
           unsigned char packet[1000];
        //   int size = pack(packet, (char*)"LHH", OwnID, zeroPad, lenOfPayLoad);
          int size = pack(newpack, (char*)"LCC", CHeader.controlIPAdd, CHeader.controlCode,zeroPad);
          int sz = 1036;
          *(newpack+size) = sz>>8;
          *(newpack+size + 1) = sz;
          memcpy(newpack+size+2, seclastSent, sz);

          ////////////change address to controller IP Address
//          *(newpack+size) = sz>>8;
  //        *(newpack+size + 1) = sz;
  //        sendPacket(packet, newfd);
           sendPacket(newpack, newfd);
           break;
        }
        default :
        {
             cout<<"Something Wrong came in default with ControlCode :"<<CHeader.controlCode<<endl;
        }
      }
    }

    struct transferInfo *createTransPack()
    {
      unsigned int initSeqNo = 0;
      struct transferInfo *ti = new (struct transferInfo)();
      unpack((unsigned char*)CHeader.pload, (char*)"LCCH", &(ti->destRouterIP), &(ti->TTL), &(ti->TransferID), &initSeqNo);
      char *tempPoint = CHeader.pload;
      tempPoint += 8;
      strcpy(ti->FileName, tempPoint);

//      cout<<"FileName :"<<(ti->FileName)<<endl;
//      cout<<"Destination Router IP :"<<(ti->destRouterIP)<<endl;
//      cout<<"Init TTL :"<<(ti->TTL)<<endl;
//      cout<<"TransferID :"<<(ti->TransferID)<<endl;
//      cout<<"Init Seq. Num. :"<<initSeqNo<<endl;
      (ti->SeqNum).push_back(initSeqNo);
      return ti;
    }

};

int main(int argc, char **argv)
{
    if(argc<=1)
        return 0;

    RegisServer SRVR(argv[1]);

    return 0;
}
