#include <iostream>
#include <vector>
#include <queue>
#include <cstring>
#include <unistd.h>
#include <ctime>
#include <mutex>
#include <algorithm>
#include <thread>
#include <condition_variable>
#include <map>
#include <sys/stat.h> // for mkfifo
#include <fcntl.h>    // for open
#include <unistd.h>   // for read, write
#include <errno.h>    // for errno
#include <string.h>   // for strerror
#include <array>
#include <SFML/Graphics.hpp>   // For window, shapes, colors, text
#include <SFML/Window.hpp>     // For window events
#include <SFML/System.hpp>     // For time, threads, vectors
#include <SFML/Audio.hpp>      // For sound effects and music
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstring>   // for strerror


using namespace std;


// Constants
const int COMMERCIAL = 0;
const int CARGO = 1;
const int MILITARY = 2;
const int MEDICAL = 3;

const int HOLDING = 0;
const int APPROACH = 1;
const int LANDING = 2;
const int TAXI = 3;
const int AT_GATE = 4;
const int TAKEOFF_ROLL = 5;
const int CLIMB = 6;
const int DEPARTURE = 7;

const int RWY_A = 0;
const int RWY_B = 1;
const int RWY_C = 2;

const int DIR_NORTH = 0;
const int DIR_SOUTH = 1;
const int DIR_EAST = 2;
const int DIR_WEST = 3;

const int EMERGENCY_PRIORITY = 999;

// Speed limits structure
struct SpeedLimit {
    int phase;
    int minSpeed;
    int maxSpeed;
};

const int MAX_PHASES = 8;
SpeedLimit speedLimits[MAX_PHASES] = {
    {HOLDING, 400, 600},
    {APPROACH, 240, 290},
    {LANDING, 30, 240},
    {TAXI, 15, 30},
    {AT_GATE, 0, 5},
    {TAKEOFF_ROLL, 50, 290},
    {CLIMB, 300, 463},
    {DEPARTURE, 800, 900}
};

// AVN structure
struct AVN {
    char avnId[100];
    char flightName[30];
    char airline[30];
    int aircraftType;
    int recordedSpeed;
    int permissibleSpeed;
    time_t issueTime;
    double fineAmount;
    bool isPaid;
    time_t dueDate;  // Format: YYYY-MM-DD

};

struct PhaseData {
    int timer = 0;
    bool thresholdCrossed = false;
    int sustainedSpeed = 0;
};

// Aircraft structure
struct Aircraft {
    char flightName[30];
    char airline[30];
    int type;
    int direction;
    int airlinenumber;
    int scheduledTime;
    int priority;
    int phase;
    bool isAssigned;
    bool completed;
    int waitingTime;
    int assignedRunway;
    char status[50];
    int speed;
    bool hasSpeedViolation;
    bool isEmergency=false;
    time_t entryTime;
     PhaseData phaseData;
    float visualX = 0.f;  // Position for rendering
    std::chrono::steady_clock::time_point phaseStartTime;  // For time-based movement
    std::chrono::steady_clock::time_point waitStartTime;

};

// Runway structure
struct Runway {
    int type;
    bool isAvailable;
    char currentFlight[30];
};

// Global Variables
vector<Aircraft> arrivalFlights;
vector<Aircraft> departureFlights;
Runway runways[3] = {{RWY_A, true, ""}, {RWY_B, true, ""}, {RWY_C, true, ""}};
vector<AVN> aviationViolationNotices;

// Synchronization objects
std::mutex runwayMutex;  // Single mutex for all runways to prevent race conditions
std::mutex activeMutex;  // Mutex for active flights collection
std::mutex displayMutex; // Mutex for display operations
std::mutex avnMutex;     // Mutex for violation notices
std::condition_variable runwayCV; // Condition variable for runway availability
 std::mutex scheduledMutex;



queue<Aircraft>  arrivalQueue;
queue<Aircraft> departureQueue;
vector<Aircraft*> activeFlights;
 vector<Aircraft*> flightsForThisMinute;

std::map<std::string, float> flightFadeMap;  // flightName -> opacity value (0 to 255)


// New variables for synchronized processing
vector<Aircraft*> priorityOrderedFlights;
std::mutex priorityMutex;
int currentPriorityIndex = 0;

// Declarations
void getFlightData();
void simulateATC();
void assignToRunway(Aircraft &flight, int runwayIndex);
void monitorSpeed(Aircraft &flight);
void checkSpeedViolations(Aircraft &flight);
void generateAVN(const Aircraft &flight, int recordedSpeed, int permissibleSpeed);
void handleGroundFault(Aircraft &flight);
int findFlightIndex(const char* flightName);
void displayDashboard(int currentTime);
void freeRunway(const char* flightName);
const char* getDirectionName(int dir);
const char* getPhaseName(int phase);
const char* getAircraftTypeName(int type);
int findSpeedLimit(int phase);
void flightLifecycle(Aircraft *flight);
void runSFMLVisualization();
void createPipeIfNotExists(const char* path);
void sendExitSignal() ;
void clearScreen();

class Visualizer {
private:
    sf::RenderWindow window;
    sf::Font font;
    sf::Clock simClock;
    int currentTime = 0;
    std::chrono::steady_clock::time_point currStartTime;
    int scheduledIndex = 0;
    vector<Aircraft*> scheduledFlights;
    vector<std::thread> flightThreads;
    std::atomic<bool> simulationRunning{true};
    sf::Texture airportTexture;
    sf::Sprite airportSprite;
    sf::Texture runwayTexture;
    sf::Sprite runwaySpriteTemplate;
    sf::Texture flightTexture;
    sf::Sprite flightSpriteTemplate;

public:
    Visualizer() : window(sf::VideoMode(1400, 1000), "ATC Simulation") {
        window.setFramerateLimit(60);
        if (!font.loadFromFile("Howdy Frog.ttf")) {
            std::cerr << "Failed to load font.\n";
        }
        currStartTime = std::chrono::steady_clock::now();

        for (auto &f : arrivalFlights)
        {
             f.waitStartTime = std::chrono::steady_clock::now();
            scheduledFlights.push_back(&f);
            }
        for (auto &f : departureFlights)
        {
                 f.waitStartTime = std::chrono::steady_clock::now();
            scheduledFlights.push_back(&f);
            }
        sort(scheduledFlights.begin(), scheduledFlights.end(), [](Aircraft* a, Aircraft* b) {
            return a->scheduledTime < b->scheduledTime;
        });
        
        
               if (!airportTexture.loadFromFile("/home/huma-taj/Downloads/airport_bg.jpeg")) {
                    std::cerr << "Failed to load airport background.\n";
                    }
          airportSprite.setTexture(airportTexture);
          // Set dimmed transparency (alpha) to the background
          sf::Vector2u textureSize = airportTexture.getSize();
          sf::Vector2u windowSize = window.getSize();
          float scaleX = static_cast<float>(windowSize.x) / textureSize.x;
          float scaleY = static_cast<float>(windowSize.y) / textureSize.y;
          airportSprite.setScale(scaleX, scaleY);
          airportSprite.setColor(sf::Color(255, 255, 255, 80));  // Still dim



          if (!runwayTexture.loadFromFile("/home/huma-taj/Downloads/runway.png")) {
              std::cerr << "Failed to load runway texture.\n";
          }
          runwaySpriteTemplate.setTexture(runwayTexture);
          sf::Vector2u runwaySize = runwayTexture.getSize();
         float runwayScaleX = static_cast<float>(windowSize.x) / runwaySize.x;
          float runwayScaleY = 1.5f;  // Keep or adjust based on height needs
          runwaySpriteTemplate.setScale(runwayScaleX, runwayScaleY);


          if (!flightTexture.loadFromFile("/home/huma-taj/Downloads/plane_icon.png")) {
              std::cerr << "Failed to load aircraft texture.\n";
          }
          flightSpriteTemplate.setTexture(flightTexture);
          flightSpriteTemplate.setScale(0.5f, 0.5f);  // Adjust for aircraft size

    }

void runSFML() {
    cout << "========== STARTING SIMULATION ==========\n";
    cout << "Total flights to process: " << scheduledFlights.size() << endl;

    std::atomic<bool> simulationFinished = false;  // new shared flag
      // Load and play background music
    sf::Music bgMusic;
    if (!bgMusic.openFromFile("/home/huma-taj/Downloads/bg_music.ogg")) {
        std::cerr << "Failed to load background music!" << std::endl;
    } else {
        bgMusic.setLoop(true);
        bgMusic.setVolume(40);
        bgMusic.play();
    }
    // Create simulation thread
    std::thread simThread([this, &simulationFinished]() {
        while (simulationRunning) {
            simulateStep();

            if (currentTime > 300) {  // Or your end condition
                simulationRunning = false;
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        simulationFinished = true;
    });

    // Render loop
    while (window.isOpen()) {
        if (simulationFinished) {
            window.close();  // Auto-close window when simulation completes
            break;
        }

        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed || !simulationRunning) {
                simulationRunning = false;
                   sendExitSignal();
                window.close();
            }
        }

        render();
    }

    // Clean up
    simThread.join();

    for (auto& t : flightThreads) {
        t.join();
    }

    return;
}

// Add these new members to your Visualizer class
private:
   
    // Structure to track visual state of aircraft
    struct AircraftVisualState {
        std::chrono::steady_clock::time_point lastStateChange;
        int currentPhase;
        int runwayIndex;
        float targetX;
        float currentX;
        bool isActive;
        bool pendingRemoval;
        // Additional visual state properties
        float speed;
        bool completed;
        bool speedViolation;
        bool avionicsActive;
    };
     // For smooth visualization
    std::unordered_map<std::string, float> flightFadeMap;  // Alpha values for fading
    std::unordered_map<std::string, AircraftVisualState> flightVisualStates;


    // Helper function to get color based on flight phase
    sf::Color getColorForPhase(int phase) {
        switch (phase) {
            case APPROACH: return sf::Color(100, 150, 255);  // Blue
            case LANDING: return sf::Color(50, 200, 50);     // Green
            case TAXI: return sf::Color(200, 200, 50);       // Yellow
            case HOLDING: return sf::Color(200, 150, 50);    // Orange
            case TAKEOFF_ROLL: return sf::Color(255, 100, 100); // Red
            case DEPARTURE: return sf::Color(150, 100, 255); // Purple
            case AT_GATE: return sf::Color(100, 255, 200);   // Teal
            case CLIMB: return sf::Color(255, 150, 200);     // Pink
            default: return sf::Color::White;
        }
    }

    // Function to update visual state based on backend state
    void updateVisualStates() {
        auto now = std::chrono::steady_clock::now();
        
        // Get local copies of backend state under locks
        std::array<Runway, 3> localRunways;
        {
            std::lock_guard<std::mutex> lock(runwayMutex);
            for (int i = 0; i < 3; ++i)
                localRunways[i] = runways[i];
        }
        
        std::vector<Aircraft*> localActive;
        {
            std::lock_guard<std::mutex> lock(activeMutex);
          localActive = activeFlights;
    
        }
        
        // Update all active flights
        std::unordered_set<std::string> currentActiveFlights;
        for (Aircraft* flight : localActive) {
            if (!flight) continue;
            
            currentActiveFlights.insert(flight->flightName);
            
            // Create or update visual state
            auto& state = flightVisualStates[flight->flightName];
            
            // Check if phase changed
            bool stateChanged = (state.currentPhase != flight->phase || 
                                state.runwayIndex != flight->assignedRunway || 
                                !state.isActive);
            
            // Always update properties even if phase didn't change
            state.speed = flight->speed;
            state.completed = flight->completed;
            state.speedViolation = flight->hasSpeedViolation;
            state.avionicsActive = flight->hasSpeedViolation;
            
            if (stateChanged) {
                state.lastStateChange = now;
                state.currentPhase = flight->phase;
                state.runwayIndex = flight->assignedRunway;
                state.isActive = true;
                state.pendingRemoval = false;
                
                // Set target position based on phase - DO NOT MODIFY FLIGHT PROPERTIES
                switch (flight->phase) {
                    case APPROACH:
                        state.targetX = 200.0f;
                        break;
                    case LANDING:
                        state.targetX = 400.0f;
                        break;
                    case TAXI:
                        state.targetX = 600.0f;
                        break;
                    case HOLDING:
                        state.targetX = 400.0f;
                        break;
                    case TAKEOFF_ROLL:
                        state.targetX = 800.0f;
                        break;
                    case DEPARTURE:
                        state.targetX = 1000.0f;
                        break;
                    case AT_GATE:
                        state.targetX = 300.0f;
                        break;
                    case CLIMB:
                        state.targetX = 900.0f;
                        break;
                    default:
                        state.targetX = 500.0f;
                }
            }
            
            // Ensure fade-in effect for new flights
            if (flightFadeMap.find(flight->flightName) == flightFadeMap.end())
                flightFadeMap[flight->flightName] = 0.0f;
        }
        
        // Mark flights for removal that are no longer active
        for (auto& pair : flightVisualStates) {
            if (currentActiveFlights.find(pair.first) == currentActiveFlights.end() && 
                !pair.second.pendingRemoval) {
                pair.second.pendingRemoval = true;
                pair.second.lastStateChange = now;
                pair.second.targetX = 1200.0f;  // Move off screen to the right
            }
        }
        
        // Remove flights that have been pending removal for more than 3 seconds
        std::vector<std::string> toRemove;
        for (auto& pair : flightVisualStates) {
            if (pair.second.pendingRemoval) {
                float elapsed = std::chrono::duration<float>(
                    now - pair.second.lastStateChange).count();
                if (elapsed > 3.0f) {
                    toRemove.push_back(pair.first);
                }
            }
        }
        
        for (const auto& key : toRemove) {
            flightVisualStates.erase(key);
            flightFadeMap.erase(key);
        }
    }

public:
    void render() {
        // Update visual states based on backend data
        updateVisualStates();
        
        window.clear();
        window.draw(airportSprite);  // Background
        
        // 1. Lock local copies
        std::array<Runway, 3> localRunways;
        {
            std::lock_guard<std::mutex> lock(runwayMutex);
            for (int i = 0; i < 3; ++i)
                localRunways[i] = runways[i];
        }
        
        std::vector<Aircraft*> localScheduled;
        {
            std::lock_guard<std::mutex> lock(scheduledMutex);
            localScheduled = scheduledFlights;
        }
        
        // 2. Draw Runways with color based on availability
        for (int i = 0; i < 3; i++) {
            sf::Sprite runwaySprite = runwaySpriteTemplate;
            runwaySprite.setPosition(5, 250 + i * 200);
            runwaySprite.setColor(localRunways[i].isAvailable
                ? sf::Color(200, 255, 200)  // greenish
                : sf::Color(255, 120, 120)  // reddish
            );
            window.draw(runwaySprite);
            
            // Draw runway label
            sf::Text label("Runway ", font, 16);
            label.setString("Runway " + std::string(1, 'A' + i));
            label.setPosition(runwaySprite.getPosition().x, 
                             runwaySprite.getPosition().y - 20);
            label.setFillColor(sf::Color::White);
            window.draw(label);
            
            // If runway is occupied, show aircraft name
            if (!localRunways[i].isAvailable && strlen(localRunways[i].currentFlight) > 0) {
                sf::Text occupiedLabel(localRunways[i].currentFlight, font, 14);
                occupiedLabel.setPosition(runwaySprite.getPosition().x + 100, 
                                         runwaySprite.getPosition().y - 20);
                occupiedLabel.setFillColor(sf::Color::Yellow);
                window.draw(occupiedLabel);
            }
        }
        
        // 3. Draw Scheduled Flights (left side list)
        {
           int y = 100;
           /* for (Aircraft* flight : localScheduled) {
                if (!flight) continue;
                
                // Skip if this flight is already active
                if (flightVisualStates.find(flight->flightName) != flightVisualStates.end() &&
                    flightVisualStates[flight->flightName].isActive)
                    continue;
                
                sf::CircleShape aircraft(10);
                aircraft.setFillColor(sf::Color(255, 255, 255, 200));
                aircraft.setPosition(50, y);
                window.draw(aircraft);
                
                sf::Text label(flight->flightName, font, 12);
                label.setPosition(70, y - 5);
                label.setFillColor(sf::Color::White);
                window.draw(label);
                
                // Add scheduled time
                sf::Text timeLabel("T+" + std::to_string(flight->scheduledTime), font, 12);
                timeLabel.setPosition(170, y - 5);
                timeLabel.setFillColor(sf::Color(200, 200, 200));
                window.draw(timeLabel);
                
                y += 30;
            }*/
            
            sf::Text label("Scheduled Flights", font, 14);
            label.setPosition(30, 50);
            label.setFillColor(sf::Color::White);
            window.draw(label);
        }
        
        // 4. Draw Active and Pending Removal Flights (On Runways)
        auto now = std::chrono::steady_clock::now();
        int waitingX=0;
        for (auto& pair : flightVisualStates) {
            const std::string& flightName = pair.first;
            AircraftVisualState& state = pair.second;
            
            // Calculate elapsed time since last state change
            float elapsed = std::chrono::duration<float>(now - state.lastStateChange).count();
            
            // Update fade effect
            float& alpha = flightFadeMap[flightName];
            if (state.pendingRemoval) {
                // Fade out effect
                alpha = std::max(0.0f, 255.0f - (elapsed * 85.0f));
            } else {
                // Fade in effect
                alpha = std::min(255.0f, alpha + 5.0f);
            }
            
            // Smooth position animation (with easing)
            float t = std::min(1.0f, elapsed * 2.0f);  // Faster animation
            t = 1.0f - std::pow(1.0f - t, 3.0f);  // Cubic ease out
            
            float startX = (state.currentX == 0) ? 0.0f : state.currentX;
            state.currentX = startX + t * (state.targetX - startX);
            if(state.runwayIndex != 0 && state.runwayIndex != 1 && state.runwayIndex !=2  )
            {
                     state.currentX= 300 + waitingX;
                     waitingX += 100;
            }
            
            // Determine Y from assigned runway
            float y = 220 + state.runwayIndex * 200;
            
            // Set color based on phase with alpha
            sf::Color color = getColorForPhase(state.currentPhase);
            
            // Override color for speed violation
            if (state.speedViolation) {
                color = sf::Color(255, 50, 50); // Bright red for violation
            }
            
            color.a = static_cast<sf::Uint8>(alpha);
            
            // Draw sprite
            sf::Sprite sprite = flightSpriteTemplate;
            sprite.setPosition(state.currentX, y);
            sprite.setColor(color);
            
            // If completed, add a visual indicator (slight rotation)
            if (state.completed) {
                sprite.setRotation(15.0f); 
                continue;
            }
            
            window.draw(sprite);
            
            // Draw status indicators background panel
            sf::RectangleShape statusPanel;
            statusPanel.setSize(sf::Vector2f(200, 50));
            statusPanel.setPosition(state.currentX + 25, y - 5);
            statusPanel.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(alpha * 0.7f)));
            window.draw(statusPanel);
            
            // Flight name
            sf::Text nameLabel(flightName, font, 12);
            nameLabel.setPosition(state.currentX + 30, y);
            nameLabel.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(alpha)));
            window.draw(nameLabel);
            
            // Draw phase indicator
            std::string phaseText;
            switch (state.currentPhase) {
                case APPROACH: phaseText = "Approaching"; break;
                case LANDING: phaseText = "Landing"; break;
                case TAXI: phaseText = "Taxiing"; break;
                case HOLDING: phaseText = "holding"; break;
                case TAKEOFF_ROLL: phaseText = "Takeoff"; break;
                case DEPARTURE: phaseText = "Departing"; break;
                case AT_GATE: phaseText = "At Gate"; break;
                case CLIMB: phaseText = "Climbing"; break;
                default: phaseText = "Unknown";
            }
            
            sf::Text phaseLabel(phaseText, font, 10);
            phaseLabel.setPosition(state.currentX + 30, y + 15);
            phaseLabel.setFillColor(sf::Color(200, 200, 200, static_cast<sf::Uint8>(alpha)));
            window.draw(phaseLabel);
            
            // Draw speed
            sf::Text speedLabel("Speed: " + std::to_string(static_cast<int>(state.speed)) + " kts", font, 10);
            speedLabel.setPosition(state.currentX + 30, y + 30);
            speedLabel.setFillColor(
                state.speedViolation 
                    ? sf::Color(255, 100, 100, static_cast<sf::Uint8>(alpha)) 
                    : sf::Color(150, 255, 150, static_cast<sf::Uint8>(alpha))
            );
            window.draw(speedLabel);
            
            // Draw avionics status
            std::string avionicsText = state.avionicsActive ? "AVN: ON" : "AVN: OFF";
            sf::Text avionicsLabel(avionicsText, font, 10);
            avionicsLabel.setPosition(state.currentX + 120, y + 30);
            avionicsLabel.setFillColor(
                state.avionicsActive 
                    ? sf::Color(150, 255, 150, static_cast<sf::Uint8>(alpha)) 
                    : sf::Color(255, 150, 150, static_cast<sf::Uint8>(alpha))
            );
            window.draw(avionicsLabel);
            
            // Draw runway assignment
            sf::Text runwayLabel("RWY: " + std::string(1, 'A' + state.runwayIndex), font, 10);
            runwayLabel.setPosition(state.currentX + 120, y + 15);
            runwayLabel.setFillColor(sf::Color(200, 200, 200, static_cast<sf::Uint8>(alpha)));
            window.draw(runwayLabel);
            
            // Draw completed status if applicable
            if (state.completed) {
                sf::Text completedLabel("COMPLETED", font, 10);
                completedLabel.setPosition(state.currentX + 120, y);
                completedLabel.setFillColor(sf::Color(100, 255, 100, static_cast<sf::Uint8>(alpha)));
                window.draw(completedLabel);
            }
        }
        
        // 5. Draw legend at the bottom
        {
            float legendY = window.getSize().y - 100;
            sf::Text legendTitle("STATUS INDICATORS:", font, 14);
            legendTitle.setPosition(30, legendY);
            legendTitle.setFillColor(sf::Color::White);
            window.draw(legendTitle);
            
            // Status icons
            float xOffset = 30;
            float yOffset = legendY + 25;
            
            // Speed violation indicator
            sf::CircleShape speedViolation(8);
            speedViolation.setFillColor(sf::Color(255, 50, 50));
            speedViolation.setPosition(xOffset, yOffset);
            window.draw(speedViolation);
            sf::Text speedLabel("Speed Violation", font, 12);
            speedLabel.setPosition(xOffset + 20, yOffset - 5);
            speedLabel.setFillColor(sf::Color::White);
            window.draw(speedLabel);
            
            // Avionics status
            xOffset += 150;
            sf::CircleShape avionicsIcon(8);
            avionicsIcon.setFillColor(sf::Color(150, 255, 150));
            avionicsIcon.setPosition(xOffset, yOffset);
            window.draw(avionicsIcon);
            sf::Text avionicsLabel("AVN Active", font, 12);
            avionicsLabel.setPosition(xOffset + 20, yOffset - 5);
            avionicsLabel.setFillColor(sf::Color::White);
            window.draw(avionicsLabel);
            
            // Completed status
            xOffset += 150;
            sf::CircleShape completedIcon(8);
            completedIcon.setFillColor(sf::Color::White);
            completedIcon.setPosition(xOffset, yOffset);
            completedIcon.setRotation(15.0f);
            window.draw(completedIcon);
            sf::Text completedLabel("Completed", font, 12);
            completedLabel.setPosition(xOffset + 20, yOffset - 5);
            completedLabel.setFillColor(sf::Color::White);
            window.draw(completedLabel);
        }
        
        // 6. Draw current simulation time
        sf::Text timeDisplay("Simulation Time: T+" + std::to_string(currentTime), font, 18);
        timeDisplay.setPosition(30, 20);
        timeDisplay.setFillColor(sf::Color::White);
        window.draw(timeDisplay);
        
        window.display();
    }
private:
    void simulateStep() {
        if (currentTime > 300) 
        {//simulationRunning = false;
        return;}

        std::vector<Aircraft*> flightsToProcess;
        
        {
            std::lock_guard<std::mutex> lock(scheduledMutex);
            flightsForThisMinute.clear();

            while (scheduledIndex < scheduledFlights.size() &&
                   scheduledFlights[scheduledIndex]->scheduledTime == currentTime) {
                flightsForThisMinute.push_back(scheduledFlights[scheduledIndex]);
                scheduledIndex++;
            }
            
        }
        
        cout << "Flights scheduled for this minute: " << flightsForThisMinute.size() << endl;
            if (!flightsForThisMinute.empty()) {
            // Sort the flights by priority - key change: sort by priority and type before processing
            sort(flightsForThisMinute.begin(), flightsForThisMinute.end(), 
                [](Aircraft* a, Aircraft* b) {
                    // First by emergency status
                    if (a->isEmergency && !b->isEmergency) return true;
                    if (!a->isEmergency && b->isEmergency) return false;
                    
                  
                  /*  
                    // Then by type (MILITARY > MEDICAL > CARGO > COMMERCIAL)
                    if (a->type != b->type) {
                        if (a->type == MILITARY) return true;
                        if (b->type == MILITARY) return false;
                        if (a->type == MEDICAL) return true;
                        if (b->type == MEDICAL) return false;
                        if (a->type == CARGO) return true;
                        if (b->type == CARGO) return false;
                    }*/
                      // Then by priority
                    if (a->priority != b->priority)
                        return a->priority > b->priority;
                    // Finally by arrival time
                    return a->entryTime < b->entryTime;
                });
            
            cout << "Sorted flights by priority (from highest to lowest):" << endl;
            for (auto flight : flightsForThisMinute) {
                cout << "  - " << flight->flightName << " (Priority: " << flight->priority 
                     << ", Type: " << getAircraftTypeName(flight->type) << ")" << endl;
            }
            
               // Create a copy to work with outside the lock
            flightsToProcess = flightsForThisMinute;
            
            // Process flights in priority order
            for (auto flight : flightsToProcess) {
                // Add to active flights
                {
                    std::lock_guard<std::mutex> lock(activeMutex);
                    flight->phaseStartTime = std::chrono::steady_clock::now();
                    activeFlights.push_back(flight);
                }
                
                cout << "Flight " << flight->flightName << " is now active at time " << currentTime << endl;

                // Create a thread for each flight but use a modified flightLifecycle function
                flightThreads.push_back(std::thread([flight, this]() {
                    // Set thread-local current time for this flight
                    flight->entryTime = currentTime;
                    
                    // Call the modified flightLifecycle
                    flightLifecycle(flight);
                }));
                
                // Brief pause to allow thread setup and prevent race conditions
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        
        // Display runway status
        cout << "\nCurrent Runway Status:" << endl;
        {
            std::lock_guard<std::mutex> lock(runwayMutex);
            for (int i = 0; i < 3; i++) {
                cout << "  Runway " << (char)('A' + i) << ": " 
                     << (runways[i].isAvailable ? "Available" : "Occupied by " + string(runways[i].currentFlight)) << endl;
            }
        }
        
        // Display dashboard
        {
            std::lock_guard<std::mutex> lock(displayMutex);
            displayDashboard(currentTime);
        }
      
   auto now = std::chrono::steady_clock::now();
  currentTime = std::chrono::duration_cast<std::chrono::seconds>(now - currStartTime).count();

    }
    
    
    
};

// pipes
const char* ATC_TO_AVN_PIPE = "/tmp/atc_to_avn";

// Create named pipe (FIFO) if it doesn't exist
void createPipeIfNotExists(const char* path) {
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        cerr << "Error creating pipe " << path << ": " << strerror(errno) << endl;
        exit(1);
    }
}
void sendExitSignal() {
    int fd = open(ATC_TO_AVN_PIPE, O_WRONLY);
    if (fd != -1) {
        const char* exitMsg = "EXIT";
        write(fd, exitMsg, strlen(exitMsg));
        close(fd);
    }
}

int main() {
    srand(time(0));
    createPipeIfNotExists(ATC_TO_AVN_PIPE);
    getFlightData();

    Visualizer().runSFML();

    sendExitSignal();
    return 0;
}



void getFlightData() {
    int n,emer=0;
    cout << "Enter number of flights to schedule: ";
    while (!(cin >> n) || n <= 0) {
        cout << "Invalid input. Please enter a positive number: ";
        cin.clear();
        cin.ignore(10000, '\n');
    }

    for (int i = 0; i < n; i++) {
        Aircraft flight;
        cout << "\nEnter details for flight " << (i + 1) << ":\n";

        // Flight Name
        cout << "Flight Name: ";
        cin >> flight.flightName;

        // Airline Selection
        cout << "Airline:\n";
        cout << "1. PIA\n2. Pakistan Airforce\n3. AirBlue\n4. FedEx\n5. Blue Dart\n6. Agha Khan Air Ambulance\n";
        while (!(cin >> flight.airlinenumber) || flight.airlinenumber < 1 || flight.airlinenumber > 6) {
            cout << "Invalid choice. Enter a number between 1 and 6: ";
            cin.clear();
            cin.ignore(10000, '\n');
        }

        switch (flight.airlinenumber) {
            case 1: strcpy(flight.airline, "PIA"); break;
            case 2: strcpy(flight.airline, "Pakistan Airforce"); break;
            case 3: strcpy(flight.airline, "AirBlue"); break;
            case 4: strcpy(flight.airline, "FedEx"); break;
            case 5: strcpy(flight.airline, "Blue Dart"); break;
            case 6: strcpy(flight.airline, "Agha Khan Air Ambulance"); break;
        }

        // Aircraft Type
        cout << "Aircraft Type (0-Commercial, 1-Cargo, 2-Military, 3-Medical): ";
        while (!(cin >> flight.type) || flight.type < 0 || flight.type > 3) {
            cout << "Invalid input. Enter a number between 0 and 3: ";
            cin.clear();
            cin.ignore(10000, '\n');
        }

        // Direction
        cout << "Direction (0-North, 1-South, 2-East, 3-West): ";
        while (!(cin >> flight.direction) || flight.direction < 0 || flight.direction > 3) {
            cout << "Invalid input. Enter a number between 0 and 3: ";
            cin.clear();
            cin.ignore(10000, '\n');
        }

        // Scheduled Time
        cout << "Scheduled Time (minute, e.g., 5 means at 5th min): ";
        while (!(cin >> flight.scheduledTime) || flight.scheduledTime < 0) {
            cout << "Invalid input. Enter a non-negative number: ";
            cin.clear();
            cin.ignore(10000, '\n');
        }

        // Priority
        cout << "Priority (0–999, higher number = higher priority): ";
        while (!(cin >> flight.priority) || flight.priority < 0 || flight.priority > 999) {
            cout << "Invalid input. Enter a number between 0 and 999: ";
            cin.clear();
            cin.ignore(10000, '\n');
        }
        cout<<"Is Flight Emergency? (YES = 1 , NO = 0) : ";
        while (!(cin >> emer) || emer < 0 || emer > 1) {
            cout << "Invalid input. Enter a number  0 or 1: ";
            cin.clear();
            cin.ignore(10000, '\n');
        }
        // Set emergency based on type
        if (flight.type == MILITARY || flight.type == MEDICAL || emer) {
            flight.isEmergency = true;
            cout<<"flight is set emer"<<endl;
        } 

        flight.phase = (flight.direction == DIR_NORTH || flight.direction == DIR_SOUTH) ? HOLDING : AT_GATE;
        flight.isAssigned = false;
        flight.completed = false;
        flight.waitingTime = 0;
        flight.waitStartTime = std::chrono::steady_clock::now();
        flight.assignedRunway = -1;
        flight.entryTime = time(NULL);
        flight.hasSpeedViolation = false;
    
        if(flight.phase==HOLDING) flight.speed=600;
        else flight.speed=0;

        strcpy(flight.status, "Waiting");

        if (flight.direction == DIR_NORTH || flight.direction == DIR_SOUTH)
            arrivalFlights.push_back(flight);
        else
            departureFlights.push_back(flight);
    }

    for (auto f : arrivalFlights) arrivalQueue.push(f);
    for (auto f : departureFlights) departureQueue.push(f);
}

void assignToRunway(Aircraft &flight, int runwayIndex) {
    if (runways[runwayIndex].isAvailable) {
        runways[runwayIndex].isAvailable = false;
        strcpy(runways[runwayIndex].currentFlight, flight.flightName);
        flight.isAssigned = true;
        flight.assignedRunway = runwayIndex;
       /* sprintf(flight.status, "Assigned Runway %c", 'A' + runwayIndex);
        cout << "RUNWAY ASSIGNED: Flight " << flight.flightName 
             << " (Priority: " << flight.priority 
             << ", Type: " << getAircraftTypeName(flight.type)
             << ") assigned to Runway " << (char)('A' + runwayIndex) << endl;*/
    } else {
        cout << "ERROR: Tried to assign already occupied Runway " 
             << (char)('A' + runwayIndex) << " to flight " << flight.flightName << endl;
    }
}


void monitorSpeed(Aircraft &flight) {
    string flightID = flight.flightName;
   PhaseData &data = flight.phaseData;

    bool isArrival = (flight.direction == DIR_NORTH || flight.direction == DIR_SOUTH);

    if (data.timer < 10) {
        // If threshold was already crossed, maintain the sustained speed
        if (data.thresholdCrossed) {
            flight.speed = data.sustainedSpeed;
              data.thresholdCrossed = false;
        } else {
                      int change=0;
            // Check threshold based on phase
            if (isArrival) {            
                switch (flight.phase) {
                    case HOLDING:
                      change = (rand() % 100)+1 ; 
                     flight.speed -= change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed <= 400) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 500;//flight.speed;
                        }
                        break;
                    case APPROACH:
                      change = (rand() % 5) +1; 
                     flight.speed -= change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed <= 240) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed =270; //flight.speed;
                        }
                        break;
                    case LANDING:
                      change = (rand() % 30) +1; 
                     flight.speed -= change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed <= 30) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 100;//flight.speed;
                        }
                        break;
                    case TAXI:
                      change = (rand() % 2)+1; 
                     flight.speed -= change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed <= 15) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 20;//flight.speed;
                        }
                        break;
                        
                        case AT_GATE:
                      change = (rand() % 2) ; 
                     flight.speed -= change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed <= 0) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 3;//flight.speed;
                        }
                        break;
                    default: break;
                }
            } else {
                switch (flight.phase) {
                    case AT_GATE:
                    change = (rand() % 2) ; 
                     flight.speed += change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed >= 5) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 1;//flight.speed;
                        }
                        break;
                    case TAXI:
                    change = (rand() %2 )+1 ; 
                     flight.speed += change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed >= 30) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 20;//flight.speed;
                        }
                        break;
                    case TAKEOFF_ROLL:
                    change = (rand() % 30)+1 ; 
                     flight.speed += change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed >= 290) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 220;//flight.speed;
                        }
                        break;
                    case CLIMB:
                    change = (rand() % 20)+1 ; 
                     flight.speed += change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed >= 463) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 400;//flight.speed;
                        }
                        break;
                        case DEPARTURE:
                    change = (rand() % 10)+1 ; 
                     flight.speed += change;
              if (flight.speed < 0) flight.speed = 0; // Ensure speed doesn't go below 0
                        if (flight.speed >= 900) {
                            data.thresholdCrossed = true;
                            data.sustainedSpeed = 860;//flight.speed;
                        }
                        break;
                    default: break;
                }
            }
        }

        // Update flight status text (based on current phase)
        switch (flight.phase) {
            case HOLDING:    sprintf(flight.status, "Holding at %d km/h", flight.speed); break;
            case APPROACH:   sprintf(flight.status, "Approaching at %d km/h", flight.speed); break;
            case LANDING:    sprintf(flight.status, "Landing at %d km/h", flight.speed); break;
            case TAXI:       sprintf(flight.status, "Taxiing at %d km/h", flight.speed); break;
            case AT_GATE:    strcpy(flight.status, "At Gate"); break;
            case TAKEOFF_ROLL: sprintf(flight.status, "Takeoff Roll at %d km/h", flight.speed); break;
            case CLIMB:      sprintf(flight.status, "Climbing at %d km/h", flight.speed); break;
            case DEPARTURE:  sprintf(flight.status, "Departing at %d km/h", flight.speed); break;
            default: break;
        }

        data.timer++;
    } else {
            if (isArrival) {
                switch (flight.phase) {
                    case HOLDING:    flight.phase = APPROACH; flight.speed = 290; break;
                    case APPROACH:   flight.phase = LANDING;  flight.speed = 240;break;
                    case LANDING:    flight.phase = TAXI;     flight.speed = 30; break;
                    case TAXI:       flight.phase = AT_GATE;  flight.speed = 5; strcpy(flight.status, "At Gate"); break;
                    case AT_GATE:    strcpy(flight.status, "Arrived"); flight.completed = true; break;
                    default: break;
            }
            } else {
                switch (flight.phase) {
                    case AT_GATE:    flight.phase = TAXI; flight.speed = 15; break;
                    case TAXI:       flight.phase = TAKEOFF_ROLL; flight.speed = 50; break;
                    case TAKEOFF_ROLL: flight.phase = CLIMB; flight.speed = 300;break;
                    case CLIMB:      flight.phase = DEPARTURE; flight.speed = 800; break;
                    case DEPARTURE:
                            strcpy(flight.status, "Departed");
                            flight.completed = true;
                            break;
                    default: break;
                }
            }


        // Reset for next phase tracking
        data.timer = 0;
        data.thresholdCrossed = false;
        data.sustainedSpeed = 0;
    }
}

void checkSpeedViolations(Aircraft &flight) {
    if (flight.type == MILITARY || flight.type == MEDICAL) return;

    int limitIndex = findSpeedLimit(flight.phase);
    if (limitIndex == -1) return;

    int minSpeed = speedLimits[limitIndex].minSpeed;
    int maxSpeed = speedLimits[limitIndex].maxSpeed;

    if (flight.speed < minSpeed || flight.speed > maxSpeed) {
        //if (!flight.hasSpeedViolation) {
            generateAVN(flight, flight.speed, (flight.speed < minSpeed) ? minSpeed : maxSpeed);
            flight.hasSpeedViolation = true;
   //     }
    }
}

void generateAVN(const Aircraft &flight, int recordedSpeed, int permissibleSpeed) {
    std::lock_guard<std::mutex> lock(avnMutex);
    
    AVN newAVN;
    sprintf(newAVN.avnId, "AVN-%d", (int)aviationViolationNotices.size() + 1);
    strncpy(newAVN.flightName, flight.flightName, 10);
    strncpy(newAVN.airline, flight.airline, 30);
    newAVN.aircraftType = flight.type;
    newAVN.recordedSpeed = recordedSpeed;
    newAVN.permissibleSpeed = permissibleSpeed;
    newAVN.issueTime = time(NULL);
    newAVN.dueDate = newAVN.issueTime + (3 * 24 * 60 * 60);  // 3 days later

    switch (flight.type) {
        case COMMERCIAL: newAVN.fineAmount = 500000; break;
        case CARGO: newAVN.fineAmount = 700000; break;
        default: newAVN.fineAmount = 0; break;
    }

    if (newAVN.fineAmount > 0)
        newAVN.fineAmount *= 1.15;

    newAVN.isPaid = false;
    aviationViolationNotices.push_back(newAVN);

    cout << "!!! AVN ISSUED for " << flight.flightName << " Speed Violation!" << endl;
    // Serialize AVN to string
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s|%s|%s|%d|%d|%d|%ld|%.2f|%d\n",
        newAVN.avnId,
        newAVN.flightName,
        newAVN.airline,
        newAVN.aircraftType,
        newAVN.recordedSpeed,
        newAVN.permissibleSpeed,
        newAVN.issueTime,
        newAVN.fineAmount,
        newAVN.isPaid ? 1 : 0
    );

    // Write to pipe
    int fd = open(ATC_TO_AVN_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd != -1) {
        write(fd, buffer, strlen(buffer));
        close(fd);
    } else {
        cerr << "Could not open pipe for writing: " << strerror(errno) << endl;
    }
}

void handleGroundFault(Aircraft &flight) {
    cout << "!!! GROUND FAULT detected for " << flight.flightName << " - removing from system" << endl;
    
    {
        std::lock_guard<std::mutex> lock(activeMutex);
        
        int index = findFlightIndex(flight.flightName);
        if (index != -1) {
            // Free the runway if this aircraft was using one
            freeRunway(flight.flightName);
            
            // Remove from active flights
            activeFlights.erase(activeFlights.begin() + index);
        }
    }
}

int findFlightIndex(const char* flightName) {
    for (int i = 0; i < activeFlights.size(); i++) {
        if (strcmp(activeFlights[i]->flightName, flightName) == 0) {
            return i;
        }
    }
    return -1;
}

void displayDashboard(int currentTime) {
    // clearScreen();
    cout << "================= AirControlX Dashboard =================\n";
    cout << "Current Time: " << currentTime << " minutes\n\n";

    cout << "Active Flights:\n";
    for (auto flight : activeFlights) {
        cout << "- " << flight->flightName << " | " << flight->airline
             << " | Type: " << getAircraftTypeName(flight->type)
             << " | Dir: " << getDirectionName(flight->direction)
             << " | Status: " << flight->status
             << " | Phase: " << getPhaseName(flight->phase)
             << " | Speed: " << flight->speed << " km/h"
             << " | Wait: " << flight->waitingTime << " min"
             << " | Priority: " << flight->priority;
        
        if (flight->hasSpeedViolation)
            cout << " [SPEED VIOLATION]";
            
        cout << "\n";
    }

    cout << "\nRunway Status:\n";
    for (int i = 0; i < 3; i++) {
        const char* rwyName = (i == 0) ? "RWY-A" : (i == 1) ? "RWY-B" : "RWY-C";
        cout << "- " << rwyName << ": " << (runways[i].isAvailable ? "Available" : runways[i].currentFlight) << "\n";
    }

    if (!aviationViolationNotices.empty()) {
        cout << "\nAviation Violation Notices (" << aviationViolationNotices.size() << "):\n";
        int displayCount = min(5, (int)aviationViolationNotices.size()); // Show only latest 5 AVNs
        for (int i = aviationViolationNotices.size() - 1; i >= (int)aviationViolationNotices.size() - displayCount; i--) {
            AVN &avn = aviationViolationNotices[i];
            cout << "- " << avn.avnId << " | " << avn.flightName 
                 << " | Speed: " << avn.recordedSpeed << "/" << avn.permissibleSpeed << " km/h"
                 << " | Fine: PKR " << avn.fineAmount << "\n";
        }
    }

    cout << "========================================================\n\n";
}

void freeRunway(const char* flightName) {
    std::lock_guard<std::mutex> lock(runwayMutex);
    
    for (int i = 0; i < 3; i++) {
        if (strcmp(runways[i].currentFlight, flightName) == 0) {
            runways[i].isAvailable = true;
            strcpy(runways[i].currentFlight, "");
            cout << "RUNWAY FREED: Runway " << (char)('A' + i) << " is now available" << endl;
            
            // Notify waiting threads that a runway is available
            runwayCV.notify_all();
            break;
        }
    }
}

const char* getDirectionName(int dir) {
    switch (dir) {
        case DIR_NORTH: return "North";
        case DIR_SOUTH: return "South";
        case DIR_EAST: return "East";
        case DIR_WEST: return "West";
        default: return "Unknown";
    }
}

const char* getPhaseName(int phase) {
    switch (phase) {
        case HOLDING: return "Holding";
        case APPROACH: return "Approach";
        case LANDING: return "Landing";
        case TAXI: return "Taxi";
        case AT_GATE: return "At Gate";
        case TAKEOFF_ROLL: return "Takeoff Roll";
        case CLIMB: return "Climb";
        case DEPARTURE: return "Departure";
        default: return "Unknown";
    }
}

const char* getAircraftTypeName(int type) {
    switch (type) {
        case COMMERCIAL: return "Commercial";
        case CARGO: return "Cargo";
        case MILITARY: return "Military";
        case MEDICAL: return "Medical";
        default: return "Unknown";
    }
}

int findSpeedLimit(int phase) {
    for (int i = 0; i < MAX_PHASES; i++) {
        if (speedLimits[i].phase == phase) {
            return i;
        }
    }
    return -1;
}

void flightLifecycle(Aircraft *flight) {
    //cout << "Thread started for flight " << flight->flightName << " with priority " << flight->priority << endl;
    // Try to obtain a runway with proper priority handling
    while (!flight->isAssigned && !flight->completed) {
        // Try to get a runway immediately
        {
            std::unique_lock<std::mutex> lock(runwayMutex);
        //    cout << "Flight " << flight->flightName << " trying to get runway" << endl;
            
            // Directly try to assign runway - simplified approach
            bool assigned = false;
            
            // For cargo flights, only try Runway C
            if (flight->type == CARGO) {
                if (runways[RWY_C].isAvailable) {
                    assignToRunway(*flight, RWY_C);
                    assigned = true;
                }
            } 
            // For emergency flights, try any runway
            else if (flight->isEmergency || flight->type == MILITARY || flight->type == MEDICAL) {
             //cout<<"emer "<<flight->isEmergency<<endl;
           //  cout<<flight->type<<endl;
           //  cout<<flight->isEmergency<<endl;
                // Emergency arrival
                if (flight->direction == DIR_NORTH || flight->direction == DIR_SOUTH) {
                    if (runways[RWY_A].isAvailable) {
                                    cout<<"assigning"<<flight->flightName<<"runway a emergency "<<endl;
                        assignToRunway(*flight, RWY_A);
                        assigned = true;
                    } else if (runways[RWY_C].isAvailable) {
                        assignToRunway(*flight, RWY_C);
                        assigned = true;
                    } else if (runways[RWY_B].isAvailable) {
                        assignToRunway(*flight, RWY_B);
                        assigned = true;
                    }
                } else { // Emergency departure
                    if (runways[RWY_B].isAvailable) {
                        assignToRunway(*flight, RWY_B);
                        assigned = true;
                    } else if (runways[RWY_C].isAvailable) {
                        assignToRunway(*flight, RWY_C);
                        assigned = true;
                    } else if (runways[RWY_A].isAvailable) {
                        assignToRunway(*flight, RWY_A);
                        assigned = true;
                    }
                }
            }
            // For arrival flights (North/South), prefer Runway A
            else if (flight->direction == DIR_NORTH || flight->direction == DIR_SOUTH) {
                if (runways[RWY_A].isAvailable) {
                cout<<"assigning"<<flight->flightName<<"runway a"<<endl;
                    assignToRunway(*flight, RWY_A);
                    assigned = true;
                } else if (runways[RWY_C].isAvailable) {
                  cout<<"assigning"<<flight->flightName<<"runway c bcz runway a not avaiilabe"<<endl;
                    assignToRunway(*flight, RWY_C);
                    assigned = true;
                }
            }
            // For departure flights (East/West), prefer Runway B
            else {
                if (runways[RWY_B].isAvailable) {
                                cout<<"assigning"<<flight->flightName<<"runway b"<<endl;
                    assignToRunway(*flight, RWY_B);
                    assigned = true;
                } else if (runways[RWY_C].isAvailable) {
                                  cout<<"assigning"<<flight->flightName<<"runway a bcz runway c not avaiilabe"<<endl;
                    assignToRunway(*flight, RWY_C);
                    assigned = true;
                }
            }
            
            if (assigned) {
                cout << "SUCCESS: Flight " << flight->flightName << " (priority " << flight->priority 
                     << ") assigned to runway " << (char)('A' + flight->assignedRunway) << endl;
            } else {
               auto now = std::chrono::steady_clock::now();
flight->waitingTime = std::chrono::duration_cast<std::chrono::seconds>(now - flight->waitStartTime).count();

             //   cout << "Flight " << flight->flightName << " waiting for runway, time: " << flight->waitingTime << endl;
                }
            }
      //      std::cout << "[WAITING] " << flight->flightName << " isAssigned=" << flight->isAssigned 
         // << " completed=" << flight->completed << std::endl;

        }
        
        // If not assigned, wait a bit before trying again
        // Use std::this_thread::sleep_for instead of sleep() for better cross-platform support
        if (!flight->isAssigned) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait 500ms before trying again
        }
    
          // Process the flight after getting a runway
    while (!flight->completed) {
        if (flight->isAssigned) {
            monitorSpeed(*flight);
            checkSpeedViolations(*flight);

            if (rand() % 100 == 0 && (flight->phase == TAXI || flight->phase == AT_GATE)) {
                handleGroundFault(*flight);
                break; // Exit if removed
            }
        }   
        // Sleep for shorter time to be more responsive
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //    std::cout << "[PROCESSING] " << flight->flightName 
   //       << " Phase=" << flight->phase 
    //      << " completed=" << flight->completed << std::endl;

        }
         freeRunway(flight->flightName);
    //     std::cout << "Flight thread finished: " << flight->flightName << std::endl;
}
    
    

void clearScreen() {
    cout << "\033[2J\033[1;1H";
}
