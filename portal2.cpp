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
#include <ctime>
#include <atomic>  // ✅ Added for atomic flag

using namespace std;

// Constants for named pipes
#define AVN_TO_PORTAL_PIPE "/tmp/avn_to_portal"

// Mutex for protecting AVN access
std::mutex avnMutex;

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

// Map to store AVNs by airline
std::map<std::string, std::vector<AVN>> airlineAVNs;

// Function to create the pipe if it doesn't exist
void createPipeIfNotExist() {
    if (mkfifo(AVN_TO_PORTAL_PIPE, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating AVN_TO_PORTAL pipe: " << strerror(errno) << endl;
        exit(1);
    }
}

// Parse AVN from buffer
AVN parseAVN(const char* buffer) {
    AVN newAVN;
    memset(&newAVN, 0, sizeof(AVN));
    
    // Create a copy of the buffer to tokenize
    char bufferCopy[1024];
    strncpy(bufferCopy, buffer, sizeof(bufferCopy) - 1);
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

// Format date and time
std::string formatTime(time_t timestamp) {
    struct tm* timeinfo = localtime(&timestamp);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

// Return aircraft type as string
std::string getAircraftTypeName(int type) {
    switch (type) {
        case 0: return "Commercial";
        case 1: return "Cargo";
        case 2: return "Military";
        case 3: return "Medical";
        default: return "Unknown";
    }
}

// Listen for AVNs from AVN Generator
void listenForAVNs() {
    int fd = open(AVN_TO_PORTAL_PIPE, O_RDONLY);
    if (fd == -1) {
        perror("Error opening AVN_TO_PORTAL pipe");
        return;
    }
    
    char buffer[1024];
    while (running) {
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            if (strncmp(buffer, "EXIT", 4) == 0) {
                cout << "Received exit signal. Shutting down..." << endl;
                                running = false;  // ✅ Stop the program

                break;
            }
            // Parse the AVN
            AVN receivedAVN = parseAVN(buffer);
            
            // Update in the map with mutex protection
            {
                std::lock_guard<std::mutex> lock(avnMutex);
                
                // Get airline name as string for map key
                std::string airlineName(receivedAVN.airline);
                
                // Check if this airline exists in the map
                auto it = airlineAVNs.find(airlineName);
                if (it != airlineAVNs.end()) {
                    // Check if this AVN already exists
                    bool found = false;
                    for (auto& avn : it->second) {
                        if (strcmp(avn.avnId, receivedAVN.avnId) == 0) {
                            // Update existing AVN
                            avn = receivedAVN;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        // Add new AVN
                        it->second.push_back(receivedAVN);
                    }
                } else {
                    // Create new entry for this airline
                    std::vector<AVN> avnList;
                    avnList.push_back(receivedAVN);
                    airlineAVNs[airlineName] = avnList;
                }
                
                cout << "[Portal] Received AVN: " << receivedAVN.avnId << " | " 
                     << receivedAVN.flightName << " | Status: " 
                     << (receivedAVN.isPaid ? "PAID" : "UNPAID") << endl;
            }
        }
    }
    
    close(fd);
}

// Display the portal dashboard
void displayPortalDashboard() {
    while (running) {
        sleep(3); // Update every 3 seconds
        
        system("clear"); // Clear console
        
        cout << "=======================================================" << endl;
        cout << "                    AIRLINE PORTAL                      " << endl;
        cout << "=======================================================" << endl;
        
        {
            std::lock_guard<std::mutex> lock(avnMutex);
            
            if (airlineAVNs.empty()) {
                cout << "No AVNs received yet." << endl;
            } else {
                // Display AVNs by airline
                for (const auto& [airline, avnList] : airlineAVNs) {
                    cout << "\n--- " << airline << " ---" << endl;
                    cout << "AVN ID    | Flight     | Type       | Speed/Limit | Fine(PKR)  | Issue Date        | Status" << endl;
                    cout << "----------|------------|------------|-------------|------------|-------------------|--------" << endl;
                    
                    for (const auto& avn : avnList) {
                        cout << avn.avnId << " | " 
                             << avn.flightName << " | " 
                             << getAircraftTypeName(avn.aircraftType) << " | " 
                             << avn.recordedSpeed << "/" << avn.permissibleSpeed << " km/h | " 
                             << avn.fineAmount << " | " 
                             << formatTime(avn.issueTime) << " | " 
                             << (avn.isPaid ? "PAID" : "UNPAID") << endl;
                    }
                    
                    // Calculate and display totals
                    int totalAVNs = avnList.size();
                    int paidAVNs = 0;
                    double totalFines = 0;
                    double paidFines = 0;
                    
                    for (const auto& avn : avnList) {
                        totalFines += avn.fineAmount;
                        if (avn.isPaid) {
                            paidAVNs++;
                            paidFines += avn.fineAmount;
                        }
                    }
                    
                    cout << "\nSummary: Total AVNs: " << totalAVNs 
                         << " | Paid: " << paidAVNs 
                         << " | Unpaid: " << (totalAVNs - paidAVNs)
                         << " | Total Fines: PKR " << totalFines 
                         << " | Paid: PKR " << paidFines <<endl;

                }
            }
        }
        
    }
}

int main() {
    // Create pipe
    createPipeIfNotExist();
    
    cout << "Airline Portal Started" << endl;
    
    // Start threads
    std::thread avnListenerThread(listenForAVNs);
    std::thread dashboardThread(displayPortalDashboard);
    
    // Wait for threads to finish
    avnListenerThread.join();
    dashboardThread.join();
    
      unlink(AVN_TO_PORTAL_PIPE);
    cout << "Airline Portal terminated cleanly." << endl;
    
    return 0;
}
