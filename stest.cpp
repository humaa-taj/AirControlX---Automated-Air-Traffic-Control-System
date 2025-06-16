#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <errno.h>
#include <mutex>
#include <thread>
#include <map>
#include <atomic>

using namespace std;

// Pipe constants
#define AVN_TO_STRIPE_PIPE "/tmp/avn_to_stripe"
#define STRIPE_TO_AVN_PIPE "/tmp/stripe_to_avn"
#define STRIPE_TO_PORTAL_PIPE "/tmp/stripe_to_portal"

// Mutex for protecting shared data
std::mutex dataMutex;

// Atomic flag for thread control
std::atomic<bool> running(true);

// Structure for AVN
struct AVN {
    char avnId[20];
    char flightName[30];
    char airline[30];
    int aircraftType;
    int recordedSpeed;
    int permissibleSpeed;
    time_t issueTime;
    double fineAmount;
    bool isPaid;
};

// Map to store AVNs by flight name
std::map<std::string, AVN> avnMap;

// Function to create pipes if they don't exist
void createPipesIfNotExist() {
    if (mkfifo(AVN_TO_STRIPE_PIPE, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating AVN_TO_STRIPE pipe: " << strerror(errno) << endl;
        exit(1);
    }

    if (mkfifo(STRIPE_TO_AVN_PIPE, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating STRIPE_TO_AVN pipe: " << strerror(errno) << endl;
        exit(1);
    }

    if (mkfifo(STRIPE_TO_PORTAL_PIPE, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating STRIPE_TO_PORTAL pipe: " << strerror(errno) << endl;
        exit(1);
    }

    cout << "All pipes created successfully" << endl;
}

// Parse AVN from buffer
AVN parseAVN(const char* buffer) {
    AVN newAVN;
    memset(&newAVN, 0, sizeof(AVN));

    char bufferCopy[512];
    strncpy(bufferCopy, buffer, sizeof(bufferCopy));
    bufferCopy[sizeof(bufferCopy) - 1] = '\0';

    char* token = strtok(bufferCopy, "|");
    if (token) strncpy(newAVN.avnId, token, sizeof(newAVN.avnId) - 1);

    token = strtok(NULL, "|");
    if (token) strncpy(newAVN.flightName, token, sizeof(newAVN.flightName) - 1);

    token = strtok(NULL, "|");
    if (token) strncpy(newAVN.airline, token, sizeof(newAVN.airline) - 1);

    token = strtok(NULL, "|");
    if (token) newAVN.aircraftType = atoi(token);

    token = strtok(NULL, "|");
    if (token) newAVN.recordedSpeed = atoi(token);

    token = strtok(NULL, "|");
    if (token) newAVN.permissibleSpeed = atoi(token);

    token = strtok(NULL, "|");
    if (token) newAVN.issueTime = atol(token);

    token = strtok(NULL, "|");
    if (token) newAVN.fineAmount = atof(token);

    token = strtok(NULL, "|");
    if (token) newAVN.isPaid = atoi(token) == 1;

    return newAVN;
}

// Serialize AVN to string
void serializeAVN(const AVN& avn, char* buffer, size_t size) {
    snprintf(buffer, size, "%s|%s|%s|%d|%d|%d|%ld|%.2f|%d\n",
        avn.avnId,
        avn.flightName,
        avn.airline,
        avn.aircraftType,
        avn.recordedSpeed,
        avn.permissibleSpeed,
        avn.issueTime,
        avn.fineAmount,
        avn.isPaid ? 1 : 0
    );
}

// Send payment status to AVN and Portal
void sendPaymentStatus(const AVN& avn) {
    char buffer[512];
    serializeAVN(avn, buffer, sizeof(buffer));

    // Send to AVN
    int fd_avn = open(STRIPE_TO_AVN_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd_avn != -1) {
        write(fd_avn, buffer, strlen(buffer));
        close(fd_avn);
    }

    // Send to Portal
    int fd_portal = open(STRIPE_TO_PORTAL_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd_portal != -1) {
        write(fd_portal, buffer, strlen(buffer));
        close(fd_portal);
    }
}

// Listen for AVNs from AVN Generator
void listenForAVNs() {
    int fd = open(AVN_TO_STRIPE_PIPE, O_RDONLY);
    if (fd == -1) {
        perror("Error opening AVN_TO_STRIPE pipe");
        return;
    }

    char buffer[1024];
    while (running) {
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            
            if (strncmp(buffer, "EXIT", 4) == 0) {
                cout << "Received exit signal. Shutting down..." << endl;
                running = false;
                return;
                break;
                
            }

            AVN newAVN = parseAVN(buffer);

            {
                std::lock_guard<std::mutex> lock(dataMutex);
                avnMap[newAVN.flightName] = newAVN;
                cout << "AVN received for flight: " << newAVN.flightName << endl;
                cout << "Enter flight name to pay  ";
                cout.flush();
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    close(fd);
}

// Process payment for a flight
void processPayment(const string& flightName) {
    AVN avnToPay;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        auto it = avnMap.find(flightName);
        if (it != avnMap.end()) {
            avnToPay = it->second;
            found = true;
        }
    }

    if (!found) {
        cout << "No AVN found for flight: " << flightName << endl;
        return;
    }

    cout << "Processing payment for flight: " << flightName << endl;
    cout << "Fine amount: PKR " << avnToPay.fineAmount << endl;

    // Simulate payment processing
    avnToPay.isPaid = true;
    cout << "Payment successful for flight: " << flightName << endl;

    // Update and send payment status
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        avnMap[flightName] = avnToPay;
        sendPaymentStatus(avnToPay);
    }
}

// Handle user input for payments
void handleUserInput() {
    string input;
    while (running) {
        cout << "Enter flight name to pay  ";
        getline(cin, input);

        if (!running) break;

     /*   if (input == "exit") {
            running = false;
            break;
        }*/

        if (!input.empty()) {
            processPayment(input);
        }
    }
    if(!running)
    {
        return;
    }
}

int main() {
    createPipesIfNotExist();
    cout << "StripePay Process Started" << endl;

    // Start threads
    std::thread avnListenerThread(listenForAVNs);
    std::thread inputThread(handleUserInput);

    // Wait for threads to finish
    avnListenerThread.join();
    inputThread.join();

    // Clean up pipes
    unlink(AVN_TO_STRIPE_PIPE);
    unlink(STRIPE_TO_AVN_PIPE);
    unlink(STRIPE_TO_PORTAL_PIPE);

    cout << "StripePay Process Terminated" << endl;
    return 0;
}
