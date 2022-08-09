// test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>

using namespace std;


struct _t
{
    unsigned char h;
    unsigned char len;
    char name[11];
    int age;
};

struct _t2
{
    char name[11];
    int age;
};

int main()
{
    vector<unsigned char*> buffer;
   
    _t pMsg;
    pMsg.h = 0x01;
    sprintf_s(pMsg.name, "TestName");
    pMsg.age = 20;
    pMsg.len = sizeof(pMsg);

    buffer.push_back((unsigned char*)&pMsg);
    //unsigned char buf[15];// = buffer[0];
    //memcpy(buf, buffer[0], sizeof(pMsg));

    _t* lpMsg = (_t*)buffer[0];

    printf("buffer: %s %d", lpMsg->name, (int*)lpMsg + 13);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
