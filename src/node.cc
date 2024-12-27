// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "node.h"
#include "MyMessage_m.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <bitset>

Define_Module(Node);

void Node::readFile(const char *filename)
{
    std::ifstream fstream;
    std::string line;

    fstream.open(filename, std::ifstream::in);

    if (fstream)
    {
        while (getline(fstream, line))
        {
            errors.push_back(std::bitset<4>((line.substr(0, 4))));
            originalMsg.push_back(line.substr(5, line.length() - 5));
        }
    }
    else
    {
        throw cRuntimeError("Error opening file");
    }
    fstream.close();
}

std::string Node::framing(std::string payload)
{
    std::string frame = "$";
    for (auto character : payload)
    {
        if (character == '$' || character == '/')
            frame += '/';
        frame += character;
    }
    frame += '$';
    return frame;
}

std::string Node::deFraming(std::string framedPayload)
{
    std::string payload = "";
    size_t i = 1;
    while (i < framedPayload.length() - 1)
    {
        char currentChar = framedPayload[i];
        if (currentChar == '/')
        {
            i++;
            payload += framedPayload[i];
        }
        else
        {
            payload += currentChar;
        }
        i++;
    }
    return payload;
}

void Node::printReadingMessage(int m, int nextFrameToSendTemp, bool resend)
{
    omnetpp::simtime_t time = simTime() + par("PT").doubleValue() * (m - nextFrameToSendTemp);


    EV << "At time [" << time << "]," << " Node[" << getIndex()
            << "] , Introducing channel error with code = [" << errors[m] << "] ." << endl;

    fout.open("output.txt", std::fstream::app);
    fout << "At time [" << time << "]," << " Node[" << getIndex()
                << "] , Introducing channel error with code =[" << errors[m] << "] ." << endl;
    fout.close();
}

void Node::printTimeoutMessage(int ackExpected)
{
    fout.open("output.txt", std::fstream::app);
    fout << "Time out event at time [" << simTime() << "], at Node[" << getIndex() << "] for frame with seq_num= [" << ackExpected % (windowSize+1) << "]" << endl;
    fout.close();

    EV << "Time out event at time [" << simTime() << "], at Node[" << getIndex() << "] for frame with seq_num= [" << ackExpected % (windowSize+1) << "]" << endl;
}

void Node::printSendingMessage(MyMessage_Base* message, int bitToModify, std::string lossMsg, int dup, int delay,int m, int nextFrameToSendTemp, bool resend) {
    omnetpp::simtime_t time = simTime() + par("PT").doubleValue() * (m + 1 - nextFrameToSendTemp);
    if(dup == 2) {
        time  += par("DD").doubleValue();
    }
    std::bitset<8> trailer(message->getTrailer());

    fout.open("output.txt", std::fstream::app);
    fout <<  "At time [" << time << "]," <<
        " Node[" << getIndex() << "] [sent] frame with seq_num=[" << message->getHeader() << "] and payload=[" << message->getPayload() << "]" <<
        " and trailer=["<< trailer<< "] , Modified [" << bitToModify +1   << "] "
        ", Lost [" << lossMsg << "], Duplicate [" << dup << "], "
        "Delay [" << (delay ? par("ED").doubleValue() : 0) << "]. "<< endl;
    fout.close();

    EV <<  "At time [" << time << "]," <<
        " Node[" << getIndex() << "] [sent] frame with seq_num=[" << message->getHeader() << "] and payload=[" << message->getPayload() << "]" <<
        " and trailer=["<< trailer<< "] , Modified [" <<bitToModify +1 << "] "
        ", Lost [" << lossMsg << "], Duplicate [" << dup << "], "
        "Delay [" << (delay ? par("ED").doubleValue() : 0) << "]. "<< endl;
}

void Node::printSendingReceiverMessage(std::string lossMsg, std::string isAck, int number)
{
    EV << "At time [" << simTime() + par("PT").doubleValue() << "], Node[" << getIndex() << "] Sending [" << isAck
            << "] with number [" << number << "] ,loss [" << lossMsg << "]." << endl;

    fout.open("output.txt", std::fstream::app);
    fout << "At time [" << simTime() + par("PT").doubleValue() << "], Node[" << getIndex() << "] Sending [" << isAck
            << "] with number [" << number << "] ,loss [" << lossMsg << "]." << endl;
    fout.close();
}


std::vector<std::bitset<8>> stringToVector(std::string message)
{
    std::vector<std::bitset<8>> bitsVector;

    bitsVector.push_back(std::bitset<8>('$'));
    for (int i = 0; i < message.size(); i++)
    {
        if (message[i] == '$' || message[i] == '/')
        {
            bitsVector.push_back(std::bitset<8>('/'));
        }
        bitsVector.push_back(std::bitset<8>(message[i]));
    }
    bitsVector.push_back(std::bitset<8>('$'));

    return bitsVector;
}

char calculateParity(std::string data) {
    int parity = 0;
    for (char ch : data) {
        parity ^= ch;
    }
    return static_cast<char>(parity);
}

bool checkMessage(MyMessage_Base* message)
{
    std::string data = message->getPayload();
    char receivedParity = message->getTrailer();
    char calculatedParity = calculateParity(data);
    return receivedParity == calculatedParity;
}

void Node::initialize()
{
    windowSize = par("WS").intValue();

    nbuffered = 0;
    nextFrameToSend = 0;
    ackExpected = 0;
    frameExpected = 0;

    std::string outputFileName = "output.txt";
    fout.open(outputFileName);
    fout.close();
}

void Node::sendingMessageHandler(MyMessage_Base *message, const std::bitset<4> currentErrors, int m, int nextFrameToSendTemp, bool resend)
{
    int modify = currentErrors[3];
    int loss = currentErrors[2];
    int dup = currentErrors[1];
    int delay = currentErrors[0];

    double time = 0;
    int bitToModify = -2;
    std::string lossMsg = "No";
    time = par("PT").doubleValue() * (m + 1 - nextFrameToSendTemp) + par("TD").doubleValue();

    std::string sendMsg;
    std::vector<std::bitset<8>> currentMsgbits;
    currentMsgbits = stringToVector(originalMsg[m]);

    if (loss)
    {
        lossMsg = "Yes";
    }
    if (modify)
    {
        bitToModify = int(uniform(0, currentMsgbits.size() * 8));
        currentMsgbits[bitToModify / 8].flip(8 - (bitToModify % 8) - 1);
    }
    if (delay)
    {
        time += par("ED").doubleValue();
    }

    for (int i = 0; i < currentMsgbits.size(); i++)
    {
        sendMsg += (char)currentMsgbits[i].to_ulong();
    }
    message->setPayload(sendMsg.c_str());

    printSendingMessage(message, bitToModify, lossMsg, dup, delay, m, nextFrameToSendTemp, resend);

    if (dup)
    {
        MyMessage_Base *dupFrame = message->dup();
        std::bitset<8> trailer2(dupFrame->getTrailer());
        printSendingMessage(dupFrame, bitToModify, lossMsg, 2, delay, m, nextFrameToSendTemp, resend);
        if (!loss)
            sendDelayed(dupFrame, (time + par("DD").doubleValue()), "out");
    }

    if (!loss)
    {
        sendDelayed(message, time, "out");
    }
}

void Node::handleMessage(cMessage *msg)
{
    if (strcmp(msg->getName(), "GO!") == 0)
    {
        int me = getIndex();
        if (me == 0)
        {
            readFile("input0.txt");
        }
        else
        {
            readFile("input1.txt");
        }

        for (int m = 0; m < (windowSize) && m < originalMsg.size(); m++)
        {
            MyMessage_Base *message = new MyMessage_Base();
            std::bitset<4> currentErrors = errors[m];

            printReadingMessage(m, 0, false);

            message->setHeader(m);
            message->setTrailer(calculateParity(framing(originalMsg[m])));
            message->setFrameType(2);

            cMessage *timeoutEvent = new cMessage("timeoutEvent");
            timeoutEvents.push_back(timeoutEvent);
            scheduleAt(simTime() + par("TO").doubleValue() + (par("PT").doubleValue() * (m + 1)), timeoutEvents[m]);

            nbuffered += 1;
            nextFrameToSend = nextFrameToSend + 1;
            sendingMessageHandler(message, currentErrors, m, 0, false);
        }
    }
    else if (strcmp(msg->getName(), "timeoutEvent") == 0)
    {
        printTimeoutMessage(ackExpected);

        nextFrameToSend = ackExpected;
        int nextFrameToSendTemp = nextFrameToSend;
        nbuffered = 0;
        for (int m = nextFrameToSendTemp; m < nextFrameToSendTemp + (windowSize) && m < originalMsg.size(); m++)
        {
            MyMessage_Base *message2 = new MyMessage_Base();

            message2->setHeader(m % (windowSize + 1));
            message2->setTrailer(calculateParity(framing(originalMsg[m])));
            message2->setFrameType(2);

            nbuffered += 1;
            nextFrameToSend = nextFrameToSend + 1;

            cancelEvent(timeoutEvents[m]);
            scheduleAt(simTime() + par("TO").doubleValue() + (par("PT").doubleValue() * (m + 1 - nextFrameToSendTemp)), timeoutEvents[m]);

            if (m == nextFrameToSendTemp)
            {
                std::bitset<4> currentErrors = 0000;
                sendingMessageHandler(message2, currentErrors, m, nextFrameToSendTemp, true);
            }
            else
            {
                printReadingMessage(m, nextFrameToSendTemp, true);
                sendingMessageHandler(message2, errors[m], m, nextFrameToSendTemp, true);
            }
        }
    }
    else if (typeid(*msg) == typeid(MyMessage_Base)) {
        MyMessage_Base *message = check_and_cast<MyMessage_Base *>(msg);
        if (message->getFrameType() == 1 && message->getAckNack() == (ackExpected) % (windowSize + 1)){
           //ignore
        }
        else if (message->getFrameType() == 1 && message->getAckNack() == (ackExpected + 1) % (windowSize + 1)){
            nbuffered = nbuffered - 1;
            cancelEvent(timeoutEvents[ackExpected]);
            ackExpected = ackExpected + 1;
        }
        else if (message->getFrameType() == 1 && abs(message->getAckNack() - ((ackExpected) % (windowSize + 1))) <= nbuffered - 1){
            while (message->getAckNack() + 1 != ((ackExpected) % (windowSize + 1) + 1)){
                nbuffered = nbuffered - 1;
                cancelEvent(timeoutEvents[ackExpected]);
                ackExpected = ackExpected + 1;
            }
        }
        else if (message->getFrameType() == 0 && message->getAckNack() == (ackExpected) % (windowSize + 1)){
            nextFrameToSend = ackExpected;
            int nextFrameToSendTemp = nextFrameToSend;
            nbuffered = 0;
            for (int m = nextFrameToSendTemp; m < nextFrameToSendTemp + (windowSize) && m < originalMsg.size(); m++)
            {
                MyMessage_Base *message2 = new MyMessage_Base();

                message2->setHeader(m % (windowSize + 1));
                message2->setTrailer(calculateParity(framing(originalMsg[m])));
                message2->setFrameType(2);

                nbuffered += 1;
                nextFrameToSend = nextFrameToSend + 1;

                cancelEvent(timeoutEvents[m]);
                scheduleAt(simTime() + par("TO").doubleValue() + (par("PT").doubleValue() * (m + 1 - nextFrameToSendTemp)), timeoutEvents[m]);
                if (m == nextFrameToSendTemp)
                {
                    std::bitset<4> currentErrors = 0000;
                    sendingMessageHandler(message2, currentErrors, m, nextFrameToSendTemp, true);
                }
                else
                {
                    printReadingMessage(m, nextFrameToSendTemp, true);
                    sendingMessageHandler(message2, errors[m], m, nextFrameToSendTemp, true);
                }
            }
        }
        if (!(message->getFrameType() == 2) && nbuffered < (windowSize))
        {
            if (nextFrameToSend < originalMsg.size())
            {
                int nextFrameToSendTemp = nextFrameToSend;
                int old_nbuffered = nbuffered;
                for (int m = nextFrameToSendTemp; m < nextFrameToSendTemp + (windowSize - old_nbuffered) && m < originalMsg.size(); m++)
                {
                    MyMessage_Base *message = new MyMessage_Base();

                    std::bitset<4> currentErrors = errors[m];
                    printReadingMessage(m, nextFrameToSendTemp, false);

                    message->setHeader(m % (windowSize + 1));
                    message->setTrailer(calculateParity(framing(originalMsg[m])));
                    message->setFrameType(2);

                    nbuffered += 1;
                    nextFrameToSend = nextFrameToSend + 1;

                    cMessage *timeoutEvent = new cMessage("timeoutEvent");
                    timeoutEvents.push_back(timeoutEvent);
                    scheduleAt(simTime() + par("TO").doubleValue() + (par("PT").doubleValue() * (m + 1 - nextFrameToSendTemp)), timeoutEvents[m]);

                    sendingMessageHandler(message, currentErrors, m, nextFrameToSendTemp, false);
                }
            }
        }
        if (message->getFrameType() == 2)
        {
            if (message->getHeader() == frameExpected % (windowSize + 1))
            {
                MyMessage_Base *newMessage = message->dup();
                bool isTrue = checkMessage(message);
                bool isLost = uniform(0, 100) < (int)par("LP").doubleValue();

                int number = 0;
                std::string isAck = "NACK";
                std::string lossMsg = "Yes";
                newMessage->setFrameType(0);

                if (isTrue) {
                    frameExpected = frameExpected + 1;
                    newMessage->setFrameType(1);
                    isAck = "ACK";
                }
                number = frameExpected % (windowSize + 1);
                newMessage->setAckNack(number);

                if (!isLost){
                    lossMsg = "No";
                }

                printSendingReceiverMessage(lossMsg, isAck, number);

                if (!isLost){
                    sendDelayed(newMessage, par("TD").doubleValue() + par("PT").doubleValue(), "out");
                }
            }
            else
            {
                MyMessage_Base *newMessage = message->dup();
                std::string isAck = "ACK";
                std::string lossMsg = "Yes";

                bool isLost = uniform(0, 100) < (int)par("LP").doubleValue();
                newMessage->setFrameType(1);
                int number = frameExpected % (windowSize + 1);
                newMessage->setAckNack(number);

                if (!isLost){
                    lossMsg = "No";
                }

                printSendingReceiverMessage(lossMsg, isAck, number);

                if (!isLost){
                    sendDelayed(newMessage, par("TD").doubleValue() + par("PT").doubleValue(), "out");
                }
            }
        }
        cancelAndDelete(message);
    }
}

void Node::finish()
{
    EV << "End of simulation" << endl;
}
