# AirControlX — Automated Air Traffic Control System

**AirControlX** is a high-fidelity simulation of an Automated Air Traffic Control System (ATCS) for a multi-runway international airport, developed in modern **C++** with real-time visualizations using **SFML**. This project encapsulates real-world ATC logic, process synchronization, airspace violation enforcement, and payment systems — all simulated through coordinated processes and threads.

## 🚀 Why AirControlX Matters
AirControlX is more than just a simulation — it's a demonstration of how **core Operating System principles** and **system-level programming** can come together to build a responsive, concurrent, and extensible system. It showcases expertise in:

- 🔄 Multi-process orchestration  
- 🧵 Thread-level synchronization (mutexes, semaphores)  
- 📡 Real-time monitoring & violation handling  
- 📊 Live analytics dashboard  
- 🎮 Interactive visual simulation using SFML  
- 🧾 IPC-based Violation Notices & Payment Systems  

## 🛠️ Core Technologies & Concepts
- C/C++ (Modular Design)  
- POSIX Threads (pthreads)  
- Inter-process Communication (Named Pipes / FIFO)  
- SFML (Graphics + Audio)  
- Custom Scheduling and Queueing Algorithms  
- Airspace Violation System with Fine Calculation and Payment Integration  

## 🖥️ Modules & Architecture
This project is architected into 4 primary processes:

1. **ATCS Controller (`module3.cpp`)** – Handles flight logic, runway management, aircraft tracking, and real-time animation.  
2. **AVN Generator (`avn2.cpp`)** – Creates Airspace Violation Notices, calculates fines, and coordinates payment validation.  
3. **Airline Portal (`portal2.cpp`)** – Provides airline representatives with violation dashboards and payment history.  
4. **StripePay System (`stest.cpp`)** – Simulates secure fine payment processing and communicates status back to other modules.  

These processes communicate through **named pipes** and maintain strict concurrency control using **mutexes** and **semaphores**.

## 🧪 Compilation & Execution

### 🔧 Prerequisite: Install SFML
Ensure **SFML** is installed and correctly linked. If installed in a custom location, you may need to set `-I` and `-L` paths for includes and libraries.

**Install on Ubuntu:**
sudo apt-get install libsfml-dev
### 🔨 Compile the ATCS Controller

g++ -o atc_sim module3.cpp -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -pthread

### 🚦 Launch the simulation

Open 4 separate terminals, and run each process as follows:

**Terminal 1 – ATC Simulation**

./atc_sim

**Terminal 2 – AVN Generator**

g++ -o avn avn2.cpp -pthread
./avn

**Terminal 3 – StripePay**

g++ -o stripe stest.cpp -pthread
./stripe

**Terminal 4 – Airline Portal**

g++ -o portal portal2.cpp -pthread
./portal

  Note: You may need to update SFML include/library paths if you installed it manually or are using a different platform.

## 📊 Visual Simulation Features

- ✈️ Animated aircraft behavior across all flight phases

- 🛬 Dynamic, thread-safe runway assignment

- 🚨 Real-time AVN generation for rule violations

- 🎨 Color-coded aircraft and runway graphics

- 💡 Live aircraft state display: Holding, Taxiing, Taking Off, Cruising, etc.

## 💡 What This Project Demonstrates

- Mastery of low-level concurrency and synchronization

- Real-world multi-process coordination and IPC

- Effective use of real-time graphics rendering

- Ability to simulate complex, rules-based systems

- Clean, modular, extensible software design


