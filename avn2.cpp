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
#include <signal.h>
#include <atomic>  // [new added]

using namespace std;

// Constants for named pipes
#define ATC_TO_AVN_PIPE "/tmp/atc_to_avn"
#define AVN_TO_PORTAL_PIPE "/tmp/avn_to_portal"
#define AVN_TO_STRIPE_PIPE "/tmp/avn_to_stripe"
#define STRIPE_TO_AVN_PIPE "/tmp/stripe_to_avn"

// Mutex for protecting AVN access
std::mutex avnMutex;

// [new added] Atomic flag to signal threads to exit
std::atomic<bool> exitRequested(false);

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

void sendExitSignal() {
    int fd_p = open(AVN_TO_PORTAL_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd_p != -1) {
        const char* exitMsg = "EXIT";
        write(fd_p, exitMsg, strlen(exitMsg));
        close(fd_p);
    }
    
      int fd_s = open(AVN_TO_STRIPE_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd_s != -1) {
        const char* exitMsg = "EXIT";
        write(fd_s, exitMsg, strlen(exitMsg));
        close(fd_s);
    }
}

// Vector to store all AVNs
std::vector<AVN> avnList;

// Function to create named pipes if they don't exist
void createPipesIfNotExist() {
    if (mkfifo(ATC_TO_AVN_PIPE, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating ATC_TO_AVN pipe: " << strerror(errno) << endl;
        exit(1);
    }

    if (mkfifo(AVN_TO_PORTAL_PIPE, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating AVN_TO_PORTAL pipe: " << strerror(errno) << endl;
        exit(1);
    }

    if (mkfifo(AVN_TO_STRIPE_PIPE, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating AVN_TO_STRIPE pipe: " << strerror(errno) << endl;
        exit(1);
    }

    if (mkfifo(STRIPE_TO_AVN_PIPE, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating STRIPE_TO_AVN pipe: " << strerror(errno) << endl;
        exit(1);
    }

    cout << "All pipes created successfully" << endl;
}

// Parse AVN from buffer
AVN parseAVN(const char* buffer) {
    AVN newAVN;
    memset(&newAVN, 0, sizeof(AVN));

    char* token = strtok(const_cast<char*>(buffer), "|");
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

// Forward AVN to Portal and StripePay processes
void forwardAVN(const AVN& avn) {
    char buffer[512];
    serializeAVN(avn, buffer, sizeof(buffer));

    // Forward to Portal
    int fd_portal = open(AVN_TO_PORTAL_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd_portal != -1) {
        write(fd_portal, buffer, strlen(buffer));
        close(fd_portal);
        cout << "AVN forwarded to Portal: " << avn.avnId << " | " << avn.flightName << endl;
    } else {
        cerr << "Could not open portal pipe for writing: " << strerror(errno) << endl;
    }

    // Forward to StripePay
    int fd_stripe = open(AVN_TO_STRIPE_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd_stripe != -1) {
        write(fd_stripe, buffer, strlen(buffer));
        close(fd_stripe);
        cout << "AVN forwarded to StripePay: " << avn.avnId << " | " << avn.flightName << endl;
    } else {
        cerr << "Could not open stripe pipe for writing: " << strerror(errno) << endl;
    }
}

// Read from ATC and process AVNs
void readFromATCtoAVNPipe() {
    int fd = open(ATC_TO_AVN_PIPE, O_RDONLY);
    if (fd == -1) {
        perror("Error opening ATC_TO_AVN pipe");
        return;
    }

    char buffer[1024];
    while (!exitRequested) {  // [new added]
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            if (strncmp(buffer, "EXIT", 4) == 0) {
                cout << "[AVN Pipe] Received exit signal. Shutting down..." << endl;
                exitRequested = true;  // [new added]
                   sendExitSignal();
                
                break;
            }

            AVN newAVN = parseAVN(buffer);

            {
                std::lock_guard<std::mutex> lock(avnMutex);
                avnList.push_back(newAVN);
                cout << "[AVN Pipe] New AVN received: " << newAVN.avnId << " | " << newAVN.flightName << endl;
            }

            forwardAVN(newAVN);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // [new added]
        }
    }

    close(fd);
}

// Listen for payment updates from StripePay
void listenForPaymentUpdates() {
    int fd = open(STRIPE_TO_AVN_PIPE, O_RDONLY);
    if (fd == -1) {
        perror("Error opening STRIPE_TO_AVN pipe");
        return;
    }

    char buffer[1024];
    while (!exitRequested) {  // [new added]
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';

            AVN updatedAVN = parseAVN(buffer);

            {
                std::lock_guard<std::mutex> lock(avnMutex);
                for (auto& avn : avnList) {
                    if (strcmp(avn.avnId, updatedAVN.avnId) == 0) {
                        avn.isPaid = updatedAVN.isPaid;
                        cout << "[AVN Pipe] Payment status updated for AVN " << avn.avnId 
                             << " | " << avn.flightName << " | Paid: " << (avn.isPaid ? "Yes" : "No") << endl;

                        forwardAVN(avn);
                        break;
                    }
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // [new added]
        }
    }

    close(fd);
}

// Display current AVNs
void displayAVNs() {
    while (!exitRequested) {  // [new added]
        for (int i = 0; i < 50 && !exitRequested; ++i) {  // ~5 seconds with exit check
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // [new added]
        }

        if (exitRequested) break;  // [new added]

        system("clear");

        cout << "=======================================================" << endl;
        cout << "                  AVN GENERATOR DASHBOARD               " << endl;
        cout << "=======================================================" << endl;

        {
            std::lock_guard<std::mutex> lock(avnMutex);

            if (avnList.empty()) {
                cout << "No AVNs generated yet." << endl;
            } else {
                cout << "ID       | Flight     | Airline          | Speed/Limit | Fine(PKR)  | Status" << endl;
                cout << "---------|------------|------------------|-------------|------------|--------" << endl;

                for (const auto& avn : avnList) {
                    cout << avn.avnId << " | " 
                         << avn.flightName << " | " 
                         << avn.airline << " | " 
                         << avn.recordedSpeed << "/" << avn.permissibleSpeed << " km/h | " 
                         << avn.fineAmount << " | " 
                         << (avn.isPaid ? "PAID" : "UNPAID") << endl;
                }
            }
        }

        cout << "=======================================================" << endl;
    }
}

int main() {
    createPipesIfNotExist();
    cout << "AVN Generator Process Started" << endl;

    std::thread atcReaderThread(readFromATCtoAVNPipe);
    std::thread paymentListenerThread(listenForPaymentUpdates);
    std::thread displayThread(displayAVNs);

    atcReaderThread.join();
    paymentListenerThread.join();
    displayThread.join();

    cout << "AVN Generator Process Terminated Gracefully." << endl;  // [new added]
        sendExitSignal();
    return 0;
}

