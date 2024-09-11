// #include <iostream>
// #include <thread>
// #include <vector>
// #include "HazardPointerManager.hpp"

// // Define a simple class to use with HazardPointerManager
// struct TestNode {
//     int data;
//     TestNode(int d) : data(d) {
//         std::cout << "TestNode with data " << data << " is created.\n";
//     }
//     ~TestNode() {
//         std::cout << "TestNode with data " << data << " is deleted.\n";
//     }
// };

// // A simple function to retire nodes using HazardPointerManager
// void retire_nodes(HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager) {
//     for (int i = 0; i < 10; ++i) {
//         auto node = std::make_unique<TestNode>(i);  // Use unique_ptr for safer memory management
//         manager.retire(std::move(node));  // Retire the node
//         std::this_thread::sleep_for(std::chrono::milliseconds(50));
//     }
// }

// // A function to acquire and release hazard pointers
// void use_hazard_pointers(HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager) {
//     for (int i = 0; i < 5; ++i) {
//         auto hp = manager.acquire();
//         if (hp) {
//             auto node = std::make_unique<TestNode>(i * 100);  // Simulate work with the hazard pointer
//             hp->pointer.reset(node.release());  // Transfer ownership to hazard pointer
//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//             manager.release(hp);  // Release the hazard pointer when done
//         }
//     }
// }

// // Main test to show behavior
// int main() {
//     HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager = HazardSystem::HazardPointerManager<TestNode, 10, 5>::instance();

//     std::cout << "Starting hazard pointer management example.\n";

//     // Launch threads to retire and use hazard pointers concurrently
//     std::thread t1(retire_nodes, std::ref(manager));
//     std::thread t2(use_hazard_pointers, std::ref(manager));
//     std::thread t3(retire_nodes, std::ref(manager));
//     std::thread t4(use_hazard_pointers, std::ref(manager));

//     // Join the threads to ensure they finish execution
//     t1.join();
//     t2.join();
//     t3.join();
//     t4.join();

//     std::cout << "Reclaiming retired nodes.\n";
//     manager.reclaim();  // Reclaim nodes that are no longer hazards

//     std::cout << "Clearing all retired nodes.\n";
//     manager.reclaim_all();  // Clear all retired nodes

//     std::cout << "Finished hazard pointer management example.\n";
//     return 0;
// }

//--------------------------------------------------------------

// #include <iostream>
// #include <thread>
// #include <vector>
// #include <chrono>
// #include "atomic_unique_ptr.hpp"

// // Define a simple struct to use with atomic_unique_ptr
// struct TestNode {
//     int data;
//     TestNode(int d) : data(d) {
//         std::cout << "TestNode with data " << data << " is created.\n";
//     }
//     ~TestNode() {
//         std::cout << "TestNode with data " << data << " is deleted.\n";
//     }
// };

// // Function to test reset and deletion of pointers
// void reset_and_delete(HazardSystem::atomic_unique_ptr<TestNode>& ptr, int thread_id) {
//     for (int i = 0; i < 10; ++i) {
//         ptr.reset(new TestNode(i + thread_id * 100));  // Reset the pointer
//         std::this_thread::sleep_for(std::chrono::milliseconds(50));
//         ptr.reset();  // Delete the current pointer
//         std::this_thread::sleep_for(std::chrono::milliseconds(50));
//     }
// }

// // Function to test moving atomic_unique_ptr between threads
// void move_pointer(HazardSystem::atomic_unique_ptr<TestNode>& source, HazardSystem::atomic_unique_ptr<TestNode>& destination) {
//     destination = std::move(source);  // Move ownership
//     std::this_thread::sleep_for(std::chrono::milliseconds(100));
// }

// int main() {
//     HazardSystem::atomic_unique_ptr<TestNode> ptr1(new TestNode(0));  // Create a new TestNode
//     HazardSystem::atomic_unique_ptr<TestNode> ptr2;  // Empty atomic_unique_ptr

//     std::cout << "Starting multithreaded test with atomic_unique_ptr.\n";

//     // Launch threads to test reset and delete
//     std::thread t1(reset_and_delete, std::ref(ptr1), 1);
//     std::thread t2(reset_and_delete, std::ref(ptr2), 2);

//     // Launch threads to test moving ownership between threads
//     std::thread t3(move_pointer, std::ref(ptr1), std::ref(ptr2));
//     std::thread t4(move_pointer, std::ref(ptr2), std::ref(ptr1));

//     // Join the threads
//     t1.join();
//     t2.join();
//     t3.join();
//     t4.join();

//     std::cout << "Finished multithreaded test with atomic_unique_ptr.\n";

//     return 0;
// }

//--------------------------------------------------------------
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include "HazardPointerManager.hpp"  // Assuming the manager is in this header

// Define a simple class to use with HazardPointerManager
struct TestNode {
    int data;
    TestNode(int d) : data(d) {
        std::cout << "TestNode with data " << data << " created.\n";
    }
    ~TestNode() {
        std::cout << "TestNode with data " << data << " deleted.\n";
    }
};

// A function to retire nodes using HazardPointerManager
void retire_nodes(HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager) {
    for (int i = 0; i < 10; ++i) {
        auto node = std::make_unique<TestNode>(i);  // Use unique_ptr for safe memory management
        manager.retire(std::move(node));  // Retire the node safely
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // Simulate workload
    }
}

// A function to acquire and use hazard pointers
void use_hazard_pointers(HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager) {
    for (int i = 0; i < 5; ++i) {
        // Acquire a hazard pointer
        auto hp = manager.acquire();
        if (hp) {
            // Create a new TestNode and assign it to the hazard pointer
            auto node = std::make_unique<TestNode>(i * 100);  // Simulate work with hazard pointer
            hp->pointer.reset(node.release());  // Transfer ownership to hazard pointer
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Simulate more work
            manager.release(hp);  // Release the hazard pointer when done
        }
    }
}

// Main test to demonstrate correct hazard pointer usage
int main() {
    // Create a shared instance of HazardPointerManager
    HazardSystem::HazardPointerManager<TestNode, 10, 5>& manager = HazardSystem::HazardPointerManager<TestNode, 10, 5>::instance();

    std::cout << "Starting hazard pointer management example.\n";

    // Launch multiple threads to retire and use hazard pointers concurrently
    std::thread t1(retire_nodes, std::ref(manager));
    std::thread t2(use_hazard_pointers, std::ref(manager));
    std::thread t3(retire_nodes, std::ref(manager));
    std::thread t4(use_hazard_pointers, std::ref(manager));

    // Join the threads to ensure they finish execution
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    // Perform reclamation of retired nodes
    std::cout << "Reclaiming retired nodes.\n";
    manager.reclaim();  // Reclaim nodes that are no longer hazardous

    // Perform final cleanup
    std::cout << "Clearing all retired nodes.\n";
    manager.reclaim_all();  // Ensure all retired nodes are cleared

    std::cout << "Finished hazard pointer management example.\n";

    return 0;
}
